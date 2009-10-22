/*
 * vim:shiftwidth=2:expandtab
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../../pico/pico_int.h"
#include "sh2.h"
#include "compiler.h"
#include "../drc/cmn.h"

#ifndef DRC_DEBUG
#define DRC_DEBUG 0
#endif

#define dbg(l,...) { \
  if ((l) & DRC_DEBUG) \
    elprintf(EL_STATUS, ##__VA_ARGS__); \
}

#if DRC_DEBUG
#include "mame/sh2dasm.h"
#include <platform/linux/host_dasm.h>
static int insns_compiled, hash_collisions, host_insn_count;
#endif
#if (DRC_DEBUG & 2)
static u8 *tcache_dsm_ptrs[3];
static char sh2dasm_buff[64];
#define do_host_disasm(tcid) \
  host_dasm(tcache_dsm_ptrs[tcid], tcache_ptr - tcache_dsm_ptrs[tcid]); \
  tcache_dsm_ptrs[tcid] = tcache_ptr
#else
#define do_host_disasm(x)
#endif

#define BLOCK_CYCLE_LIMIT 100
#define MAX_BLOCK_SIZE (BLOCK_CYCLE_LIMIT * 6 * 6)

// we have 3 translation cache buffers, split from one drc/cmn buffer.
// BIOS shares tcache with data array because it's only used for init
// and can be discarded early
static const int tcache_sizes[3] = {
  DRC_TCACHE_SIZE * 6 / 8, // ROM, DRAM
  DRC_TCACHE_SIZE / 8, // BIOS, data array in master sh2
  DRC_TCACHE_SIZE / 8, // ... slave
};

static u8 *tcache_bases[3];
static u8 *tcache_ptrs[3];

// ptr for code emiters
static u8 *tcache_ptr;

#ifdef ARM
#include "../drc/emit_arm.c"

static const int reg_map_g2h[] = {
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
};

#else
#include "../drc/emit_x86.c"

static const int reg_map_g2h[] = {
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
};

#endif

typedef enum {
  SHR_R0 = 0, SHR_R15 = 15,
  SHR_PC,  SHR_PPC, SHR_PR,   SHR_SR,
  SHR_GBR, SHR_VBR, SHR_MACH, SHR_MACL,
} sh2_reg_e;

typedef struct block_desc_ {
  u32 addr;			// SH2 PC address
  u32 end_addr;                 // TODO rm?
  void *tcache_ptr;		// translated block for above PC
  struct block_desc_ *next;     // next block with the same PC hash
#if (DRC_DEBUG & 1)
  int refcount;
#endif
} block_desc;

static const int block_max_counts[3] = {
  4*1024,
  256,
  256,
};
static block_desc *block_tables[3];
static int block_counts[3];

// ROM hash table
#define MAX_HASH_ENTRIES 1024
#define HASH_MASK (MAX_HASH_ENTRIES - 1)
static void **hash_table;

extern void sh2_drc_entry(SH2 *sh2, void *block);
extern void sh2_drc_exit(void);

// tmp
extern void __attribute__((regparm(2))) sh2_do_op(SH2 *sh2, int opcode);
static void __attribute__((regparm(1))) sh2_test_irq(SH2 *sh2);

static void flush_tcache(int tcid)
{
  printf("tcache #%d flush! (%d/%d, bds %d/%d)\n", tcid,
    tcache_ptrs[tcid] - tcache_bases[tcid], tcache_sizes[tcid],
    block_counts[tcid], block_max_counts[tcid]);

  block_counts[tcid] = 0;
  tcache_ptrs[tcid] = tcache_bases[tcid];
  if (tcid == 0) { // ROM, RAM
    memset(hash_table, 0, sizeof(hash_table[0]) * MAX_HASH_ENTRIES);
    memset(Pico32xMem->drcblk_ram, 0, sizeof(Pico32xMem->drcblk_ram));
  }
  else
    memset(Pico32xMem->drcblk_da[tcid - 1], 0, sizeof(Pico32xMem->drcblk_da[0]));
#if (DRC_DEBUG & 2)
  tcache_dsm_ptrs[tcid] = tcache_bases[tcid];
#endif
}

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

static block_desc *dr_add_block(u32 addr, int tcache_id, int *blk_id)
{
  int *bcount = &block_counts[tcache_id];
  block_desc *bd;

  if (*bcount >= block_max_counts[tcache_id])
    return NULL;

  bd = &block_tables[tcache_id][*bcount];
  bd->addr = addr;
  bd->tcache_ptr = tcache_ptr;
  *blk_id = *bcount;
  (*bcount)++;

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
  void *block_entry;
  block_desc *this_block;
  unsigned int pc = sh2->pc;
  int op, delayed_op = 0, test_irq = 0;
  int tcache_id = 0, blkid = 0;
  int cycles = 0;
  u32 tmp, tmp2;

  // validate PC
  tmp = sh2->pc >> 29;
  if ((tmp != 0 && tmp != 1 && tmp != 6) || sh2->pc == 0) {
    printf("invalid PC, aborting: %08x\n", sh2->pc);
    // FIXME: be less destructive
    exit(1);
  }

  if ((sh2->pc & 0xe0000000) == 0xc0000000 || (sh2->pc & ~0xfff) == 0) {
    // data_array, BIOS have separate tcache (shared)
    tcache_id = 1 + sh2->is_slave;
  }

  tcache_ptr = tcache_ptrs[tcache_id];
  this_block = dr_add_block(pc, tcache_id, &blkid);

  tmp = tcache_ptr - tcache_bases[tcache_id];
  if (tmp > tcache_sizes[tcache_id] - MAX_BLOCK_SIZE || this_block == NULL) {
    flush_tcache(tcache_id);
    tcache_ptr = tcache_ptrs[tcache_id];
    other_block = NULL; // also gone too due to flush
    this_block = dr_add_block(pc, tcache_id, &blkid);
  }

  this_block->next = other_block;
  if ((sh2->pc & 0xc6000000) == 0x02000000) // ROM
    HASH_FUNC(hash_table, pc) = this_block;

  block_entry = tcache_ptr;
#if (DRC_DEBUG & 1)
  printf("== %csh2 block #%d,%d %08x -> %p\n", sh2->is_slave ? 's' : 'm',
    tcache_id, block_counts[tcache_id], pc, block_entry);
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
          emith_pass_arg_r(0, CONTEXT_REG);
          emith_pass_arg_imm(1, op);
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
        EMITH_CONDITIONAL(emit_move_r_imm32(tmp2, pc + tmp + 2), (op & 0x0200) ? 1 : 0);
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
      emith_pass_arg_r(0, CONTEXT_REG);
      emith_pass_arg_imm(1, op);
      emith_call(sh2_do_op);
      break;
    }

end_op:
    if (delayed_op == 1)
      emit_move_r_r(SHR_PC, SHR_PPC);

    if (test_irq && delayed_op != 2) {
      emith_pass_arg_r(0, CONTEXT_REG);
      emith_call(sh2_test_irq);
      break;
    }
    if (delayed_op == 1)
      break;

    do_host_disasm(tcache_id);
  }

end_block:
  this_block->end_addr = pc;

  // mark memory blocks as containing compiled code
  if ((sh2->pc & 0xe0000000) == 0xc0000000 || (sh2->pc & ~0xfff) == 0) {
    // data array, BIOS
    u16 *drcblk = Pico32xMem->drcblk_da[sh2->is_slave];
    tmp =  (this_block->addr & 0xfff) >> SH2_DRCBLK_DA_SHIFT;
    tmp2 = (this_block->end_addr & 0xfff) >> SH2_DRCBLK_DA_SHIFT;
    Pico32xMem->drcblk_da[sh2->is_slave][tmp] = (blkid << 1) | 1;
    for (++tmp; tmp < tmp2; tmp++) {
      if (drcblk[tmp])
        break; // dont overwrite overlay block
      drcblk[tmp] = blkid << 1;
    }
  }
  else if ((this_block->addr & 0xc7fc0000) == 0x06000000) { // DRAM
    tmp =  (this_block->addr & 0x3ffff) >> SH2_DRCBLK_RAM_SHIFT;
    tmp2 = (this_block->end_addr & 0x3ffff) >> SH2_DRCBLK_RAM_SHIFT;
    Pico32xMem->drcblk_ram[tmp] = (blkid << 1) | 1;
    for (++tmp; tmp < tmp2; tmp++) {
      if (Pico32xMem->drcblk_ram[tmp])
        break;
      Pico32xMem->drcblk_ram[tmp] = blkid << 1;
    }
  }

  if (reg_map_g2h[SHR_SR] == -1) {
    emith_ctx_sub(cycles << 12, SHR_SR * 4);
  } else
    emith_sub_r_imm(reg_map_g2h[SHR_SR], cycles << 12);
  emith_jump(sh2_drc_exit);
  tcache_ptrs[tcache_id] = tcache_ptr;

  do_host_disasm(tcache_id);
  dbg(1, " block #%d,%d tcache %d/%d, insns %d -> %d %.3f",
    tcache_id, block_counts[tcache_id],
    tcache_ptr - tcache_bases[tcache_id], tcache_sizes[tcache_id],
    insns_compiled, host_insn_count, (double)host_insn_count / insns_compiled);
  if ((sh2->pc & 0xc6000000) == 0x02000000) // ROM
    dbg(1, "  hash collisions %d/%d", hash_collisions, block_counts[tcache_id]);
  return block_entry;
/*
unimplemented:
  // last op
  do_host_disasm(tcache_id);
  exit(1);
*/
}

void __attribute__((noinline)) sh2_drc_dispatcher(SH2 *sh2)
{
  while (((signed int)sh2->sr >> 12) > 0)
  {
    void *block = NULL;
    block_desc *bd = NULL;

    // FIXME: must avoid doing it so often..
    sh2_test_irq(sh2);

    // we have full block id tables for data_array and RAM
    // BIOS goes to data_array table too
    if ((sh2->pc & 0xff000000) == 0xc0000000 || (sh2->pc & ~0xfff) == 0) {
      int blkid = Pico32xMem->drcblk_da[sh2->is_slave][(sh2->pc & 0xfff) >> SH2_DRCBLK_DA_SHIFT];
      if (blkid & 1) {
        bd = &block_tables[1 + sh2->is_slave][blkid >> 1];
        block = bd->tcache_ptr;
      }
    }
    // RAM
    else if ((sh2->pc & 0xc6000000) == 0x06000000) {
      int blkid = Pico32xMem->drcblk_ram[(sh2->pc & 0x3ffff) >> SH2_DRCBLK_RAM_SHIFT];
      if (blkid & 1) {
        bd = &block_tables[0][blkid >> 1];
        block = bd->tcache_ptr;
      }
    }
    // ROM
    else if ((sh2->pc & 0xc6000000) == 0x02000000) {
      bd = HASH_FUNC(hash_table, sh2->pc);

      if (bd != NULL) {
        if (bd->addr == sh2->pc)
          block = bd->tcache_ptr;
        else
          block = dr_find_block(bd, sh2->pc);
      }
    }

    if (block == NULL)
      block = sh2_translate(sh2, bd);

    dbg(4, "= %csh2 enter %08x %p, c=%d", sh2->is_slave ? 's' : 'm',
      sh2->pc, block, (signed int)sh2->sr >> 12);
#if (DRC_DEBUG & 1)
    if (bd != NULL)
      bd->refcount++;
#endif
    sh2_drc_entry(sh2, block);
  }
}

static void sh2_smc_rm_block(u16 *drcblk, u16 *p, block_desc *btab, u32 a)
{
  u16 id = *p >> 1;
  block_desc *bd = btab + id;

  dbg(1, "  killing block %08x", bd->addr);
  bd->addr = bd->end_addr = 0;

  while (p > drcblk && (p[-1] >> 1) == id)
    p--;

  // check for possible overlay block
  if (p > 0 && p[-1] != 0) {
    bd = btab + (p[-1] >> 1);
    if (bd->addr <= a && a < bd->end_addr)
      sh2_smc_rm_block(drcblk, p - 1, btab, a);
  }

  do {
    *p++ = 0;
  }
  while ((*p >> 1) == id);
}

void sh2_drc_wcheck_ram(unsigned int a, int val, int cpuid)
{
  u16 *drcblk = Pico32xMem->drcblk_ram;
  u16 *p = drcblk + ((a & 0x3ffff) >> SH2_DRCBLK_RAM_SHIFT);

  dbg(1, "%csh2 smc check @%08x", cpuid ? 's' : 'm', a);
  sh2_smc_rm_block(drcblk, p, block_tables[0], a);
}

void sh2_drc_wcheck_da(unsigned int a, int val, int cpuid)
{
  u16 *drcblk = Pico32xMem->drcblk_da[cpuid];
  u16 *p = drcblk + ((a & 0xfff) >> SH2_DRCBLK_DA_SHIFT);

  dbg(1, "%csh2 smc check @%08x", cpuid ? 's' : 'm', a);
  sh2_smc_rm_block(drcblk, p, block_tables[1 + cpuid], a);
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

#if (DRC_DEBUG & 1)
static void block_stats(void)
{
  int c, b, i, total = 0;

  for (b = 0; b < ARRAY_SIZE(block_tables); b++)
    for (i = 0; i < block_counts[b]; i++)
      if (block_tables[b][i].addr != 0)
        total += block_tables[b][i].refcount;

  for (c = 0; c < 10; c++) {
    block_desc *blk, *maxb = NULL;
    int max = 0;
    for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
      for (i = 0; i < block_counts[b]; i++) {
        blk = &block_tables[b][i];
        if (blk->addr != 0 && blk->refcount > max) {
          max = blk->refcount;
          maxb = blk;
        }
      }
    }
    if (maxb == NULL)
      break;
    printf("%08x %9d %2.3f%%\n", maxb->addr, maxb->refcount,
      (double)maxb->refcount / total * 100.0);
    maxb->refcount = 0;
  }
}
#endif

int sh2_drc_init(SH2 *sh2)
{
  if (block_tables[0] == NULL) {
    int i, cnt;

    drc_cmn_init();

    cnt = block_max_counts[0] + block_max_counts[1] + block_max_counts[2];
    block_tables[0] = calloc(cnt, sizeof(*block_tables[0]));
    if (block_tables[0] == NULL)
      return -1;

    memset(block_counts, 0, sizeof(block_counts));
    tcache_bases[0] = tcache_ptrs[0] = tcache;

    for (i = 1; i < ARRAY_SIZE(block_tables); i++) {
      block_tables[i] = block_tables[i - 1] + block_max_counts[i - 1];
      tcache_bases[i] = tcache_ptrs[i] = tcache_bases[i - 1] + tcache_sizes[i - 1];
    }

#if (DRC_DEBUG & 2)
    for (i = 0; i < ARRAY_SIZE(block_tables); i++)
      tcache_dsm_ptrs[i] = tcache_bases[i];
#endif
#if (DRC_DEBUG & 1)
    hash_collisions = 0;
#endif
  }

  if (hash_table == NULL) {
    hash_table = calloc(sizeof(hash_table[0]), MAX_HASH_ENTRIES);
    if (hash_table == NULL)
      return -1;
  }

  return 0;
}

void sh2_drc_finish(SH2 *sh2)
{
  if (block_tables[0] != NULL) {
#if (DRC_DEBUG & 1)
    block_stats();
#endif
    free(block_tables[0]);
    memset(block_tables, 0, sizeof(block_tables));

    drc_cmn_cleanup();
  }

  if (hash_table != NULL) {
    free(hash_table);
    hash_table = NULL;
  }
}
