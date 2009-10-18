/*
 * vim:shiftwidth=2:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "sh2.h"
#include "compiler.h"
#include "../drc/cmn.h"

#ifndef DRC_DEBUG
#define DRC_DEBUG 0
#endif

#if DRC_DEBUG
#include "mame/sh2dasm.h"
#include <platform/linux/host_dasm.h>
static int insns_compiled, hash_collisions, host_insn_count;
#endif
#if (DRC_DEBUG & 2)
static void *tcache_dsm_ptr = tcache;
static char sh2dasm_buff[64];
#endif

#define BLOCK_CYCLE_LIMIT 100

static void *tcache_ptr;

#include "../drc/emit_x86.c"

typedef enum {
  SHR_R0 = 0, SHR_R15 = 15,
  SHR_PC,  SHR_PPC, SHR_PR,   SHR_SR,
  SHR_GBR, SHR_VBR, SHR_MACH, SHR_MACL,
} sh2_reg_e;

typedef struct block_desc_ {
  u32 addr;			// SH2 PC address
  void *tcache_ptr;		// translated block for above PC
  struct block_desc_ *next;	// next block with the same PC hash
} block_desc;

#define MAX_BLOCK_COUNT (4*1024)
static block_desc *block_table;
static int block_count;

#define MAX_HASH_ENTRIES 1024
#define HASH_MASK (MAX_HASH_ENTRIES - 1)

extern void sh2_drc_entry(SH2 *sh2, void *block);
extern void sh2_drc_exit(void);

// tmp
extern void __attribute__((regparm(2))) sh2_do_op(SH2 *sh2, int opcode);
static void __attribute__((regparm(1))) sh2_test_irq(SH2 *sh2);

static void *dr_find_block(block_desc *tab, u32 addr)
{
  for (tab = tab->next; tab != NULL; tab = tab->next)
    if (tab->addr == addr)
      break;

  if (tab != NULL)
    return tab->tcache_ptr;

  printf("block miss for %08x\n", addr);
  return NULL;
}

static block_desc *dr_add_block(u32 addr, void *tcache_ptr)
{
  block_desc *bd;

  if (block_count == MAX_BLOCK_COUNT) {
    // FIXME: flush cache instead
    printf("block descriptor overflow\n");
    exit(1);
  }

  bd = &block_table[block_count];
  bd->addr = addr;
  bd->tcache_ptr = tcache_ptr;
  block_count++;

  return bd;
}

#define HASH_FUNC(hash_tab, addr) \
  ((block_desc **)(hash_tab))[(addr) & HASH_MASK]

// ---------------------------------------------------------------

static void emit_move_r_imm32(sh2_reg_e dst, u32 imm)
{
  int host_dst = reg_map_g2h[dst];
  int tmp = 0;

  if (host_dst != -1)
    tmp = host_dst;
  emith_move_r_imm(tmp, imm);
  if (host_dst == -1)
    emith_ctx_write(tmp, dst * 4);
}

static void emit_move_r_r(sh2_reg_e dst, sh2_reg_e src)
{
  int host_dst = reg_map_g2h[dst], host_src = reg_map_g2h[src];
  int tmp = 0;

  if (host_dst != -1 && host_src != -1) {
    emith_move_r_r(host_dst, host_src);
    return;
  }

  if (host_src != -1)
    tmp = host_src;
  if (host_dst != -1)
    tmp = host_dst;

  if (host_src == -1)
    emith_ctx_read(tmp, src * 4);
  if (host_dst == -1)
    emith_ctx_write(tmp, dst * 4);
}

static void emit_braf(sh2_reg_e reg, u32 pc)
{
  int host_reg = reg_map_g2h[reg];
  if (host_reg == -1) {
    emith_ctx_read(0, reg * 4);
  } else
    emith_move_r_r(0, host_reg);
  emith_add_r_imm(0, pc);

  emith_ctx_write(0, SHR_PPC * 4);
}

/*
static int sh2_translate_op4(int op)
{
  switch (op & 0x000f)
  {
  case 0x0b:
  default:
    emith_pass_arg(2, sh2, op);
    emith_call(sh2_do_op);
    break;
  }

  return 0;
}
*/

#define DELAYED_OP \
  delayed_op = 2

#define CHECK_UNHANDLED_BITS(mask) { \
  if ((op & (mask)) != 0) \
    goto default_; \
}

static void *sh2_translate(SH2 *sh2, block_desc *other_block)
{
  void *block_entry = tcache_ptr;
  block_desc *this_block;
  unsigned int pc = sh2->pc;
  int op, delayed_op = 0, test_irq = 0;
  int cycles = 0;
  u32 tmp, tmp2;

  this_block = dr_add_block(pc, block_entry);
  if (other_block != NULL)
    this_block->next = other_block;

  HASH_FUNC(sh2->pc_hashtab, pc) = this_block;

#if (DRC_DEBUG & 1)
  printf("== %csh2 block #%d %08x -> %p\n", sh2->is_slave ? 's' : 'm',
    block_count, pc, block_entry);
  if (other_block != NULL) {
    printf(" hash collision with %08x\n", other_block->addr);
    hash_collisions++;
  }
#endif

  while (cycles < BLOCK_CYCLE_LIMIT || delayed_op)
  {
    if (delayed_op > 0)
      delayed_op--;

    op = p32x_sh2_read16(pc, sh2);

#if (DRC_DEBUG & 3)
    insns_compiled++;
#if (DRC_DEBUG & 2)
    DasmSH2(sh2dasm_buff, pc, op);
    printf("%08x %04x %s\n", pc, op, sh2dasm_buff);
#endif
#endif

    pc += 2;
    cycles++;

    switch ((op >> 12) & 0x0f)
    {
    case 0x00:
      switch (op & 0x0f) {
      case 0x03:
        CHECK_UNHANDLED_BITS(0xd0);
        // BRAF Rm    0000mmmm00100011
        // BSRF Rm    0000mmmm00000011
        DELAYED_OP;
        if (!(op & 0x20))
          emit_move_r_imm32(SHR_PR, pc + 2);
        emit_braf((op >> 8) & 0x0f, pc + 2);
        cycles++;
        goto end_op;
      case 0x09:
        CHECK_UNHANDLED_BITS(0xf0);
        // NOP        0000000000001001
        goto end_op;
      case 0x0b:
        CHECK_UNHANDLED_BITS(0xd0);
        DELAYED_OP;
        if (!(op & 0x20)) {
          // RTS        0000000000001011
          emit_move_r_r(SHR_PPC, SHR_PR);
          cycles++;
        } else {
          // RTE        0000000000101011
          //emit_move_r_r(SHR_PC, SHR_PR);
          emit_move_r_imm32(SHR_PC, pc - 2);
          emith_pass_arg(2, sh2, op);
          emith_call(sh2_do_op);
          emit_move_r_r(SHR_PPC, SHR_PC);
          test_irq = 1;
          cycles += 3;
        }
        goto end_op;
      }
      goto default_;

    case 0x04:
      switch (op & 0x0f) {
      case 0x07:
        if ((op & 0xf0) != 0)
          goto default_;
        // LDC.L @Rm+,SR  0100mmmm00000111
        test_irq = 1;
        goto default_;
      case 0x0b:
        if ((op & 0xd0) != 0)
          goto default_;
        // JMP  @Rm   0100mmmm00101011
        // JSR  @Rm   0100mmmm00001011
        DELAYED_OP;
        if (!(op & 0x20))
          emit_move_r_imm32(SHR_PR, pc + 2);
        emit_move_r_r(SHR_PPC, (op >> 8) & 0x0f);
        cycles++;
        goto end_op;
      case 0x0e:
        if ((op & 0xf0) != 0)
          goto default_;
        // LDC Rm,SR  0100mmmm00001110
        test_irq = 1;
        goto default_;
      }
      goto default_;

    case 0x08:
      switch (op & 0x0f00) {
      // BT/S label 10001101dddddddd
      case 0x0d00:
      // BF/S label 10001111dddddddd
      case 0x0f00:
        DELAYED_OP;
        cycles--;
        // fallthrough
      // BT   label 10001001dddddddd
      case 0x0900:
      // BF   label 10001011dddddddd
      case 0x0b00:
        tmp = ((signed int)(op << 24) >> 23);
        tmp2 = delayed_op ? SHR_PPC : SHR_PC;
        emit_move_r_imm32(tmp2, pc + (delayed_op ? 2 : 0));
        emith_test_t();
        EMIT_CONDITIONAL(emit_move_r_imm32(tmp2, pc + tmp + 2), (op & 0x0200) ? 1 : 0);
        cycles += 2;
        if (!delayed_op)
          goto end_block;
        goto end_op;
      }
      goto default_;

    case 0x0a:
      // BRA  label 1010dddddddddddd
      DELAYED_OP;
    do_bra:
      tmp = ((signed int)(op << 20) >> 19);
      emit_move_r_imm32(SHR_PPC, pc + tmp + 2);
      cycles++;
      break;

    case 0x0b:
      // BSR  label 1011dddddddddddd
      DELAYED_OP;
      emit_move_r_imm32(SHR_PR, pc + 2);
      goto do_bra;

    default:
    default_:
      emit_move_r_imm32(SHR_PC, pc - 2);
      emith_pass_arg(2, sh2, op);
      emith_call(sh2_do_op);
      break;
    }

end_op:
    if (delayed_op == 1)
      emit_move_r_r(SHR_PC, SHR_PPC);

    if (test_irq && delayed_op != 2) {
      emith_pass_arg(1, sh2);
      emith_call(sh2_test_irq);
      break;
    }
    if (delayed_op == 1)
      break;

#if (DRC_DEBUG & 2)
    host_dasm(tcache_dsm_ptr, (char *)tcache_ptr - (char *)tcache_dsm_ptr);
    tcache_dsm_ptr = tcache_ptr;
#endif
  }

end_block:
  if ((char *)tcache_ptr - (char *)tcache > DRC_TCACHE_SIZE) {
    printf("tcache overflow!\n");
    fflush(stdout);
    exit(1);
  }

  if (reg_map_g2h[SHR_SR] == -1) {
    emith_ctx_sub(cycles << 12, SHR_SR * 4);
  } else
    emith_sub_r_imm(reg_map_g2h[SHR_SR], cycles << 12);
  emith_jump(sh2_drc_exit);

#if (DRC_DEBUG & 2)
  host_dasm(tcache_dsm_ptr, (char *)tcache_ptr - (char *)tcache_dsm_ptr);
  tcache_dsm_ptr = tcache_ptr;
#endif
#if (DRC_DEBUG & 1)
  printf(" tcache %d/%d, hash collisions %d/%d, insns %d -> %d %.3f\n",
    (char *)tcache_ptr - (char *)tcache, DRC_TCACHE_SIZE,
    hash_collisions, block_count, insns_compiled, host_insn_count,
    (double)host_insn_count / insns_compiled);
#endif
  return block_entry;

unimplemented:
  // last op
#if (DRC_DEBUG & 2)
  host_dasm(tcache_dsm_ptr, (char *)tcache_ptr - (char *)tcache_dsm_ptr);
  tcache_dsm_ptr = tcache_ptr;
#endif
  exit(1);
}

void __attribute__((noinline)) sh2_drc_dispatcher(SH2 *sh2)
{
  while (((signed int)sh2->sr >> 12) > 0)
  {
    void *block = NULL;
    block_desc *bd;

    // FIXME: must avoid doing it so often..
    sh2_test_irq(sh2);

    bd = HASH_FUNC(sh2->pc_hashtab, sh2->pc);

    if (bd != NULL) {
      if (bd->addr == sh2->pc)
        block = bd->tcache_ptr;
      else
        block = dr_find_block(bd, sh2->pc);
    }

    if (block == NULL)
      block = sh2_translate(sh2, bd);

#if (DRC_DEBUG & 4)
    printf("= %csh2 enter %08x %p\n", sh2->is_slave ? 's' : 'm', sh2->pc, block);
#endif
    sh2_drc_entry(sh2, block);
  }
}

void sh2_execute(SH2 *sh2, int cycles)
{
  sh2->cycles_aim += cycles;
  cycles = sh2->cycles_aim - sh2->cycles_done;

  // cycles are kept in SHR_SR unused bits (upper 20)
  sh2->sr &= 0x3f3;
  sh2->sr |= cycles << 12;
  sh2_drc_dispatcher(sh2);

  sh2->cycles_done += cycles - ((signed int)sh2->sr >> 12);
}

static void __attribute__((regparm(1))) sh2_test_irq(SH2 *sh2)
{
  if (sh2->pending_level > ((sh2->sr >> 4) & 0x0f))
  {
    if (sh2->pending_irl > sh2->pending_int_irq)
      sh2_do_irq(sh2, sh2->pending_irl, 64 + sh2->pending_irl/2);
    else {
      sh2_do_irq(sh2, sh2->pending_int_irq, sh2->pending_int_vector);
      sh2->pending_int_irq = 0; // auto-clear
      sh2->pending_level = sh2->pending_irl;
    }
  }
}

int sh2_drc_init(SH2 *sh2)
{
  if (block_table == NULL) {
    block_count = 0;
    block_table = calloc(MAX_BLOCK_COUNT, sizeof(*block_table));
    if (block_table == NULL)
      return -1;

    tcache_ptr = tcache;
#if (DRC_DEBUG & 1)
    hash_collisions = 0;
#endif
  }

  //assert(sh2->pc_hashtab == NULL);
  sh2->pc_hashtab = calloc(sizeof(sh2->pc_hashtab[0]), MAX_HASH_ENTRIES);
  if (sh2->pc_hashtab == NULL)
    return -1;

  return 0;
}

void sh2_drc_finish(SH2 *sh2)
{
  if (block_table != NULL) {
    free(block_table);
    block_table = NULL;
  }

  free(sh2->pc_hashtab);
  sh2->pc_hashtab = NULL;
}
