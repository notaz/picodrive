#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "sh2.h"
#include "compiler.h"
#include "../drc/cmn.h"

#define BLOCK_CYCLE_LIMIT 100

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

#define MAX_BLOCK_COUNT 1024
static block_desc *block_table;
static int block_count;

#define MAX_HASH_ENTRIES 1024
#define HASH_MASK (MAX_HASH_ENTRIES - 1)

#ifdef DRC_DEBUG
#include "mame/sh2dasm.h"
#include <platform/linux/host_dasm.h>
static void *tcache_dsm_ptr = tcache;
#endif

static void *tcache_ptr;

#include "../drc/emit_x86.c"

extern void sh2_drc_entry(SH2 *sh2, void *block);
extern void sh2_drc_exit(void);

// tmp
extern void __attribute__((regparm(2))) sh2_do_op(SH2 *sh2, int opcode);

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

  emith_ctx_write(0, SHR_PC * 4);
}

// FIXME: this is broken, delayed insn shouldn't affect branch
#define DELAYED_OP \
  if (delayed_op < 0) { \
    delayed_op = op; \
    goto next_op; \
  } \
  delayed_op = -1; \
  pc -= 2 /* adjust back */

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

static void *sh2_translate(SH2 *sh2, block_desc *other_block)
{
  void *block_entry = tcache_ptr;
  block_desc *this_block;
  unsigned int pc = sh2->pc;
  int op, delayed_op = -1;
  int cycles = 0;
  u32 tmp;

  this_block = dr_add_block(pc, block_entry);
  if (other_block != NULL) {
    printf("hash collision between %08x and %08x\n", pc, other_block->addr);
    this_block->next = other_block;
  }
  HASH_FUNC(sh2->pc_hashtab, pc) = this_block;

#ifdef DRC_DEBUG
  printf("== %csh2 block #%d %08x %p\n", sh2->is_slave ? 's' : 'm',
    block_count, pc, block_entry);
#endif

  while (cycles < BLOCK_CYCLE_LIMIT)
  {
    if (delayed_op >= 0)
      op = delayed_op;
    else {
next_op:
      op = p32x_sh2_read16(pc, sh2->is_slave);

#ifdef DRC_DEBUG
      {
        char buff[64];
        DasmSH2(buff, pc, op);
        printf("%08x %04x %s\n", pc, op, buff);
      }
#endif
    }

    pc += 2;
    cycles++;

    switch ((op >> 12) & 0x0f)
    {
    case 0x00:
      // RTS        0000000000001011
      if (op == 0x000b) {
        DELAYED_OP;
        emit_move_r_r(SHR_PC, SHR_PR);
        cycles++;
        goto end_block;
      }
      // RTE        0000000000101011
      if (op == 0x002b) {
        DELAYED_OP;
        cycles++;
        //emit_move_r_r(SHR_PC, SHR_PR);
        emit_move_r_imm32(SHR_PC, pc - 4);
        emith_pass_arg(2, sh2, op);
        emith_call(sh2_do_op);
        goto end_block;
      }
      // BRAF Rm    0000mmmm00100011
      if (op == 0x0023) {
        DELAYED_OP;
        cycles++;
        emit_braf((op >> 8) & 0x0f, pc);
        goto end_block;
      }
      // BSRF Rm    0000mmmm00000011
      if (op == 0x0003) {
        DELAYED_OP;
        emit_move_r_imm32(SHR_PR, pc);
        emit_braf((op >> 8) & 0x0f, pc);
        cycles++;
        goto end_block;
      }
      goto default_;

    case 0x04:
      // JMP  @Rm   0100mmmm00101011
      if ((op & 0xff) == 0x2b) {
        DELAYED_OP;
        emit_move_r_r(SHR_PC, (op >> 8) & 0x0f);
        cycles++;
        goto end_block;
      }
      // JSR  @Rm   0100mmmm00001011
      if ((op & 0xff) == 0x0b) {
        DELAYED_OP;
        emit_move_r_imm32(SHR_PR, pc);
        emit_move_r_r(SHR_PC, (op >> 8) & 0x0f);
        cycles++;
        goto end_block;
      }
      goto default_;

    case 0x08: {
      int adj = 2;
      switch (op & 0x0f00) {
      // BT/S label 10001101dddddddd
      case 0x0d00:
      // BF/S label 10001111dddddddd
      case 0x0f00:
        DELAYED_OP;
        cycles--;
        adj = 0;
        // fallthrough
      // BT   label 10001001dddddddd
      case 0x0900:
      // BF   label 10001011dddddddd
      case 0x0b00:
        cycles += 2;
        emit_move_r_imm32(SHR_PC, pc);
        emith_test_t();
        tmp = ((signed int)(op << 24) >> 23);
        EMIT_CONDITIONAL(emit_move_r_imm32(SHR_PC, pc + tmp + adj), (op & 0x0200) ? 1 : 0);
        goto end_block;
      }
      goto default_;
    }

    case 0x0a:
      // BRA  label 1010dddddddddddd
      DELAYED_OP;
    do_bra:
      tmp = ((signed int)(op << 20) >> 19);
      emit_move_r_imm32(SHR_PC, pc + tmp);
      cycles++;
      goto end_block;

    case 0x0b:
      // BSR  label 1011dddddddddddd
      DELAYED_OP;
      emit_move_r_imm32(SHR_PR, pc);
      goto do_bra;

    default:
    default_:
      emit_move_r_imm32(SHR_PC, pc - 2);
      emith_pass_arg(2, sh2, op);
      emith_call(sh2_do_op);
      break;
    }

#ifdef DRC_DEBUG
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

#ifdef DRC_DEBUG
  host_dasm(tcache_dsm_ptr, (char *)tcache_ptr - (char *)tcache_dsm_ptr);
  tcache_dsm_ptr = tcache_ptr;
#endif
  return block_entry;

unimplemented:
  // last op
#ifdef DRC_DEBUG
  host_dasm(tcache_dsm_ptr, (char *)tcache_ptr - (char *)tcache_dsm_ptr);
  tcache_dsm_ptr = tcache_ptr;
#endif
  exit(1);
}

void __attribute__((noinline)) sh2_drc_dispatcher(SH2 *sh2)
{
  while (((signed int)sh2->sr >> 12) > 0)
  {
    block_desc *bd = HASH_FUNC(sh2->pc_hashtab, sh2->pc);
    void *block = NULL;

    if (bd != NULL) {
      if (bd->addr == sh2->pc)
        block = bd->tcache_ptr;
      else
        block = dr_find_block(bd, sh2->pc);
    }

    if (block == NULL)
      block = sh2_translate(sh2, bd);

#ifdef DRC_DEBUG
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


static int cmn_init_done;

static int common_init(void)
{
  block_count = 0;
  block_table = calloc(MAX_BLOCK_COUNT, sizeof(*block_table));
  if (block_table == NULL)
    return -1;

  tcache_ptr = tcache;

  cmn_init_done = 1;
  return 0;
}

int sh2_drc_init(SH2 *sh2)
{
  if (!cmn_init_done) {
    int ret = common_init();
    if (ret)
      return ret;
  }

  assert(sh2->pc_hashtab == NULL);
  sh2->pc_hashtab = calloc(sizeof(sh2->pc_hashtab[0]), MAX_HASH_ENTRIES);
  if (sh2->pc_hashtab == NULL)
    return -1;

  return 0;
}

