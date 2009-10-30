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

#if DRC_DEBUG
#define dbg(l,...) { \
  if ((l) & DRC_DEBUG) \
    elprintf(EL_STATUS, ##__VA_ARGS__); \
}

#include "mame/sh2dasm.h"
#include <platform/linux/host_dasm.h>
static int insns_compiled, hash_collisions, host_insn_count;
#define COUNT_OP \
	host_insn_count++
#else // !DRC_DEBUG
#define COUNT_OP
#define dbg(...)
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

// host register tracking
enum {
  HR_FREE,
  HR_CACHED, // 'val' has sh2_reg_e
  HR_CACHED_DIRTY,
  HR_CONST,  // 'val' has constant
  HR_TEMP,   // reg used for temp storage
};

typedef struct {
  u8 reg;
  u8 type;
  u16 stamp; // kind of a timestamp
  u32 val;
} temp_reg_t;

// note: reg_temp[] must have at least the amount of
// registers used by handlers in worst case (currently 4)
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

static temp_reg_t reg_temp[] = {
  {  0, },
  {  1, },
  { 12, },
  { 14, },
  {  2, },
  {  3, },
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

// ax, cx, dx are usually temporaries by convention
static temp_reg_t reg_temp[] = {
  { xAX, },
  { xBX, },
  { xCX, },
  { xDX, },
};

#endif

#define T	0x00000001
#define S	0x00000002
#define I	0x000000f0
#define Q	0x00000100
#define M	0x00000200

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
extern void REGPARM(2) sh2_do_op(SH2 *sh2, int opcode);
static void REGPARM(1) sh2_test_irq(SH2 *sh2);

static void flush_tcache(int tcid)
{
  dbg(1, "tcache #%d flush! (%d/%d, bds %d/%d)", tcid,
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

// register chache
static u16 rcache_counter;

static temp_reg_t *rcache_evict(void)
{
  // evict reg with oldest stamp
  int i, oldest = -1;
  u16 min_stamp = (u16)-1;

  for (i = 0; i < ARRAY_SIZE(reg_temp); i++) {
    if (reg_temp[i].type == HR_CACHED || reg_temp[i].type == HR_CACHED_DIRTY)
      if (reg_temp[i].stamp <= min_stamp) {
        min_stamp = reg_temp[i].stamp;
        oldest = i;
      }
  }

  if (oldest == -1) {
    printf("no registers to evict, aborting\n");
    exit(1);
  }

  i = oldest;
  if (reg_temp[i].type == HR_CACHED_DIRTY) {
    // writeback
    emith_ctx_write(reg_temp[i].reg, reg_temp[i].val * 4);
  }

  return &reg_temp[i];
}

typedef enum {
  RC_GR_READ,
  RC_GR_WRITE,
  RC_GR_RMW,
} rc_gr_mode;

// note: must not be called when doing conditional code
static int rcache_get_reg(sh2_reg_e r, rc_gr_mode mode)
{
  temp_reg_t *tr;
  int i;

  // maybe already statically mapped?
  i = reg_map_g2h[r];
  if (i != -1)
    return i;

  rcache_counter++;

  // maybe already cached?
  for (i = ARRAY_SIZE(reg_temp) - 1; i >= 0; i--) {
    if ((reg_temp[i].type == HR_CACHED || reg_temp[i].type == HR_CACHED_DIRTY) &&
         reg_temp[i].val == r)
    {
      reg_temp[i].stamp = rcache_counter;
      if (mode != RC_GR_READ)
        reg_temp[i].type = HR_CACHED_DIRTY;
      return reg_temp[i].reg;
    }
  }

  // use any free reg
  for (i = ARRAY_SIZE(reg_temp) - 1; i >= 0; i--) {
    if (reg_temp[i].type == HR_FREE || reg_temp[i].type == HR_CONST) {
      tr = &reg_temp[i];
      goto do_alloc;
    }
  }

  tr = rcache_evict();

do_alloc:
  if (mode != RC_GR_WRITE)
    emith_ctx_read(tr->reg, r * 4);

  tr->type = mode != RC_GR_READ ? HR_CACHED_DIRTY : HR_CACHED;
  tr->val = r;
  tr->stamp = rcache_counter;
  return tr->reg;
}

static int rcache_get_tmp(void)
{
  temp_reg_t *tr;
  int i;

  for (i = 0; i < ARRAY_SIZE(reg_temp); i++)
    if (reg_temp[i].type == HR_FREE || reg_temp[i].type == HR_CONST) {
      tr = &reg_temp[i];
      goto do_alloc;
    }

  tr = rcache_evict();

do_alloc:
  tr->type = HR_TEMP;
  return tr->reg;
}

static int rcache_get_arg_id(int arg)
{
  int i, r = 0;
  host_arg2reg(r, arg);

  for (i = 0; i < ARRAY_SIZE(reg_temp); i++)
    if (reg_temp[i].reg == r)
      break;

  if (i == ARRAY_SIZE(reg_temp))
    // let's just say it's untracked arg reg
    return r;

  if (reg_temp[i].type == HR_CACHED_DIRTY) {
    // writeback
    emith_ctx_write(reg_temp[i].reg, reg_temp[i].val * 4);
  }
  else if (reg_temp[i].type == HR_TEMP) {
    printf("arg %d reg %d already used, aborting\n", arg, r);
    exit(1);
  }

  return i;
}

// get a reg to be used as function arg
// it's assumed that regs are cleaned before call
static int rcache_get_tmp_arg(int arg)
{
  int id = rcache_get_arg_id(arg);
  reg_temp[id].type = HR_TEMP;

  return reg_temp[id].reg;
}

// same but caches reg. RC_GR_READ only.
static int rcache_get_reg_arg(int arg, sh2_reg_e r)
{
  int i, srcr, dstr, dstid;

  dstid = rcache_get_arg_id(arg);
  dstr = reg_temp[dstid].reg;

  // maybe already statically mapped?
  srcr = reg_map_g2h[r];
  if (srcr != -1)
    goto do_cache;

  // maybe already cached?
  for (i = ARRAY_SIZE(reg_temp) - 1; i >= 0; i--) {
    if ((reg_temp[i].type == HR_CACHED || reg_temp[i].type == HR_CACHED_DIRTY) &&
         reg_temp[i].val == r)
    {
      srcr = reg_temp[i].reg;
      goto do_cache;
    }
  }

  // must read
  srcr = dstr;
  emith_ctx_read(srcr, r * 4);

do_cache:
  if (srcr != dstr)
    emith_move_r_r(dstr, srcr);

  reg_temp[dstid].stamp = ++rcache_counter;
  reg_temp[dstid].type = HR_CACHED;
  reg_temp[dstid].val = r;
  return dstr;
}

static void rcache_free_tmp(int hr)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(reg_temp); i++)
    if (reg_temp[i].reg == hr)
      break;

  if (i == ARRAY_SIZE(reg_temp) || reg_temp[i].type != HR_TEMP) {
    printf("rcache_free_tmp fail: #%i hr %d, type %d\n", i, hr, reg_temp[i].type);
    return;
  }

  reg_temp[i].type = HR_FREE;
}

static void rcache_clean(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(reg_temp); i++)
    if (reg_temp[i].type == HR_CACHED_DIRTY) {
      // writeback
      emith_ctx_write(reg_temp[i].reg, reg_temp[i].val * 4);
      reg_temp[i].type = HR_CACHED;
    }
}

static void rcache_invalidate(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(reg_temp); i++)
    reg_temp[i].type = HR_FREE;
  rcache_counter = 0;
}

static void rcache_flush(void)
{
  rcache_clean();
  rcache_invalidate();
}

// ---------------------------------------------------------------

static void emit_move_r_imm32(sh2_reg_e dst, u32 imm)
{
  int hr = rcache_get_reg(dst, RC_GR_WRITE);
  emith_move_r_imm(hr, imm);
}

static void emit_move_r_r(sh2_reg_e dst, sh2_reg_e src)
{
  int hr_d = rcache_get_reg(dst, RC_GR_WRITE);
  int hr_s = rcache_get_reg(src, RC_GR_READ);

  emith_move_r_r(hr_d, hr_s);
}

// arguments must be ready
// reg cache must be clean before call
static int emit_memhandler_read(int size)
{
  int ctxr;
  host_arg2reg(ctxr, 1);
  emith_move_r_r(ctxr, CONTEXT_REG);
  switch (size) {
  case 0: // 8
    emith_call(p32x_sh2_read8);
    break;
  case 1: // 16
    emith_call(p32x_sh2_read16);
    break;
  case 2: // 32
    emith_call(p32x_sh2_read32);
    break;
  }
  rcache_invalidate();
  // assuming arg0 and retval reg matches
  return rcache_get_tmp_arg(0);
}

static void emit_memhandler_write(int size)
{
  int ctxr;
  host_arg2reg(ctxr, 2);
  emith_move_r_r(ctxr, CONTEXT_REG);
  switch (size) {
  case 0: // 8
    emith_call(p32x_sh2_write8);
    break;
  case 1: // 16
    emith_call(p32x_sh2_write16);
    break;
  case 2: // 32
    emith_call(p32x_sh2_write32);
    break;
  }
  rcache_invalidate();
}

/*
MOV   #imm,Rn       1110nnnniiiiiiii
MOV.W @(disp,PC),Rn 1001nnnndddddddd
MOV.L @(disp,PC),Rn 1101nnnndddddddd
MOV   Rm,Rn         0110nnnnmmmm0011
MOV.B @Rm,Rn        0110nnnnmmmm0000
MOV.W @Rm,Rn        0110nnnnmmmm0001
MOV.L @Rm,Rn        0110nnnnmmmm0010
MOV.B @Rm+,Rn       0110nnnnmmmm0100
MOV.W @Rm+,Rn       0110nnnnmmmm0101
MOV.L @Rm+,Rn       0110nnnnmmmm0110
MOV.B R0,@(disp,Rn) 10000000nnnndddd
MOV.W R0,@(disp,Rn) 10000001nnnndddd
MOV.B @(disp,Rm),R0 10000100mmmmdddd
MOV.W @(disp,Rm),R0 10000101mmmmdddd
MOV.L @(disp,Rm),Rn 0101nnnnmmmmdddd
MOV.B    R0,@(disp,GBR)   11000000dddddddd
MOV.W    R0,@(disp,GBR)   11000001dddddddd
MOV.L    R0,@(disp,GBR)   11000010dddddddd
MOV.B    @(disp,GBR),R0   11000100dddddddd
MOV.W    @(disp,GBR),R0   11000101dddddddd
MOV.L    @(disp,GBR),R0   11000110dddddddd
MOVA     @(disp,PC),R0    11000111dddddddd
SWAP.B Rm,Rn              0110nnnnmmmm1000
SWAP.W Rm,Rn              0110nnnnmmmm1001
ADD         #imm,Rn  0111nnnniiiiiiii
CMP/EQ      #imm,R0  10001000iiiiiiii
EXTS.B Rm,Rn         0110nnnnmmmm1110
EXTS.W Rm,Rn         0110nnnnmmmm1111
EXTU.B Rm,Rn         0110nnnnmmmm1100
EXTU.W Rm,Rn         0110nnnnmmmm1101
MAC       @Rm+,@Rn+  0100nnnnmmmm1111
NEG       Rm,Rn      0110nnnnmmmm1011
NEGC      Rm,Rn      0110nnnnmmmm1010
AND      #imm,R0             11001001iiiiiiii
AND.B    #imm,@(R0,GBR)      11001101iiiiiiii
NOT      Rm,Rn               0110nnnnmmmm0111
OR       #imm,R0             11001011iiiiiiii
OR.B     #imm,@(R0,GBR)      11001111iiiiiiii
TAS.B    @Rn                 0100nnnn00011011
TST      #imm,R0             11001000iiiiiiii
TST.B    #imm,@(R0,GBR)      11001100iiiiiiii
XOR      #imm,R0             11001010iiiiiiii
XOR.B    #imm,@(R0,GBR)      11001110iiiiiiii
SHLL2    Rn        0100nnnn00001000
SHLR2    Rn        0100nnnn00001001
SHLL8    Rn        0100nnnn00011000
SHLR8    Rn        0100nnnn00011001
SHLL16 Rn          0100nnnn00101000
SHLR16 Rn          0100nnnn00101001
LDC      Rm,GBR    0100mmmm00011110
LDC      Rm,VBR    0100mmmm00101110
LDS      Rm,MACH   0100mmmm00001010
LDS      Rm,MACL   0100mmmm00011010
LDS      Rm,PR     0100mmmm00101010
TRAPA    #imm      11000011iiiiiiii
*/

#define DELAYED_OP \
  delayed_op = 2

#define CHECK_UNHANDLED_BITS(mask) { \
  if ((op & (mask)) != 0) \
    goto default_; \
}

#define GET_Fx() \
  ((op >> 4) & 0x0f)

#define GET_Rm GET_Fx

#define GET_Rn() \
  ((op >> 8) & 0x0f)

#define CHECK_FX_LT(n) \
  if (GET_Fx() < n) \
    goto default_

static void *sh2_translate(SH2 *sh2, block_desc *other_block)
{
  void *block_entry;
  block_desc *this_block;
  unsigned int pc = sh2->pc;
  int op, delayed_op = 0, test_irq = 0;
  int tcache_id = 0, blkid = 0;
  int cycles = 0;
  u32 tmp, tmp2, tmp3, tmp4;

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
    /////////////////////////////////////////////
    case 0x00:
      switch (op & 0x0f)
      {
      case 0x02:
        tmp = rcache_get_reg(GET_Rn(), RC_GR_WRITE);
        switch (GET_Fx())
        {
        case 0: // STC SR,Rn  0000nnnn00000010
          tmp2 = SHR_SR;
          break;
        case 1: // STC GBR,Rn 0000nnnn00010010
          tmp2 = SHR_GBR;
          break;
        case 2: // STC VBR,Rn 0000nnnn00100010
          tmp2 = SHR_VBR;
          break;
        default:
          goto default_;
        }
        tmp3 = rcache_get_reg(tmp2, RC_GR_READ);
        emith_move_r_r(tmp, tmp3);
        if (tmp2 == SHR_SR)
          emith_clear_msb(tmp, tmp, 20); // reserved bits defined by ISA as 0
        goto end_op;
      case 0x03:
        CHECK_UNHANDLED_BITS(0xd0);
        // BRAF Rm    0000mmmm00100011
        // BSRF Rm    0000mmmm00000011
        DELAYED_OP;
        if (!(op & 0x20))
          emit_move_r_imm32(SHR_PR, pc + 2);
        tmp = rcache_get_reg(SHR_PPC, RC_GR_WRITE);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        emith_move_r_r(tmp, tmp2);
        emith_add_r_imm(tmp, pc + 2);
        cycles++;
        goto end_op;
      case 0x04: // MOV.B Rm,@(R0,Rn)   0000nnnnmmmm0100
      case 0x05: // MOV.W Rm,@(R0,Rn)   0000nnnnmmmm0101
      case 0x06: // MOV.L Rm,@(R0,Rn)   0000nnnnmmmm0110
        rcache_clean();
        tmp  = rcache_get_reg_arg(0, SHR_R0);
        tmp2 = rcache_get_reg_arg(1, GET_Rm());
        tmp3 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        emith_add_r_r(tmp, tmp3);
        emit_memhandler_write(op & 3);
        goto end_op;
      case 0x07:
        // MUL.L     Rm,Rn      0000nnnnmmmm0111
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE);
        emith_mul(tmp3, tmp2, tmp);
        cycles++;
        goto end_op;
      case 0x08:
        CHECK_UNHANDLED_BITS(0xf00);
        switch (GET_Fx())
        {
        case 0: // CLRT               0000000000001000
          tmp = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_bic_r_imm(tmp, T);
          break;
        case 1: // SETT               0000000000011000
          tmp = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_or_r_imm(tmp, T);
          break;
        case 2: // CLRMAC             0000000000101000
          tmp = rcache_get_reg(SHR_MACL, RC_GR_WRITE);
          emith_move_r_imm(tmp, 0);
          tmp = rcache_get_reg(SHR_MACH, RC_GR_WRITE);
          emith_move_r_imm(tmp, 0);
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x09:
        switch (GET_Fx())
        {
        case 0: // NOP        0000000000001001
          CHECK_UNHANDLED_BITS(0xf00);
          break;
        case 1: // DIV0U      0000000000011001
          CHECK_UNHANDLED_BITS(0xf00);
          tmp = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_bic_r_imm(tmp, M|Q|T);
          break;
        case 2: // MOVT Rn    0000nnnn00101001
          tmp  = rcache_get_reg(SHR_SR, RC_GR_READ);
          tmp2 = rcache_get_reg(GET_Rn(), RC_GR_WRITE);
          emith_clear_msb(tmp2, tmp, 31);
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x0a:
        tmp = rcache_get_reg(GET_Rn(), RC_GR_WRITE);
        switch (GET_Fx())
        {
        case 0: // STS      MACH,Rn   0000nnnn00001010
          tmp2 = SHR_MACH;
          break;
        case 1: // STS      MACL,Rn   0000nnnn00011010
          tmp2 = SHR_MACL;
          break;
        case 2: // STS      PR,Rn     0000nnnn00101010
          tmp2 = SHR_PR;
          break;
        default:
          goto default_;
        }
        tmp2 = rcache_get_reg(tmp2, RC_GR_READ);
        emith_move_r_r(tmp, tmp2);
        goto end_op;
      case 0x0b:
        CHECK_UNHANDLED_BITS(0xf00);
        switch (GET_Fx())
        {
        case 0: // RTS        0000000000001011
          DELAYED_OP;
          emit_move_r_r(SHR_PPC, SHR_PR);
          cycles++;
          break;
        case 1: // SLEEP      0000000000011011
          emit_move_r_imm32(SHR_PC, pc - 2);
          tmp = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_clear_msb(tmp, tmp, 20); // clear cycles
          test_irq = 1;
          cycles = 1;
          break;
        case 2: // RTE        0000000000101011
          //emit_move_r_r(SHR_PC, SHR_PR);
          emit_move_r_imm32(SHR_PC, pc - 2);
          rcache_flush();
          emith_pass_arg_r(0, CONTEXT_REG);
          emith_pass_arg_imm(1, op);
          emith_call(sh2_do_op);
          emit_move_r_r(SHR_PPC, SHR_PC);
          test_irq = 1;
          cycles += 3;
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x0c: // MOV.B    @(R0,Rm),Rn      0000nnnnmmmm1100
      case 0x0d: // MOV.W    @(R0,Rm),Rn      0000nnnnmmmm1101
      case 0x0e: // MOV.L    @(R0,Rm),Rn      0000nnnnmmmm1110
        rcache_clean();
        tmp  = rcache_get_reg_arg(0, SHR_R0);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_add_r_r(tmp, tmp2);
        tmp  = emit_memhandler_read(op & 3);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_WRITE);
        rcache_free_tmp(tmp);
        if ((op & 3) != 2) {
          emith_sext(tmp2, tmp, (op & 1) ? 16 : 8);
        } else
          emith_move_r_r(tmp2, tmp);
        goto end_op;
      case 0x0f: // MAC.L   @Rm+,@Rn+  0000nnnnmmmm1111
        // TODO
        break;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x01:
      // MOV.L Rm,@(disp,Rn) 0001nnnnmmmmdddd
      rcache_clean();
      tmp  = rcache_get_reg_arg(0, GET_Rn());
      tmp2 = rcache_get_reg_arg(1, GET_Rm());
      emith_add_r_imm(tmp, (op & 0x0f) * 4);
      emit_memhandler_write(2);
      goto end_op;

    case 0x02:
      switch (op & 0x0f)
      {
      case 0x00: // MOV.B Rm,@Rn        0010nnnnmmmm0000
      case 0x01: // MOV.W Rm,@Rn        0010nnnnmmmm0001
      case 0x02: // MOV.L Rm,@Rn        0010nnnnmmmm0010
        rcache_clean();
        rcache_get_reg_arg(0, GET_Rn());
        rcache_get_reg_arg(1, GET_Rm());
        emit_memhandler_write(op & 3);
        goto end_op;
      case 0x04: // MOV.B Rm,@–Rn       0010nnnnmmmm0100
      case 0x05: // MOV.W Rm,@–Rn       0010nnnnmmmm0101
      case 0x06: // MOV.L Rm,@–Rn       0010nnnnmmmm0110
        tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        emith_sub_r_imm(tmp, (1 << (op & 3)));
        rcache_clean();
        rcache_get_reg_arg(0, GET_Rn());
        rcache_get_reg_arg(1, GET_Rm());
        emit_memhandler_write(op & 3);
        goto end_op;
      case 0x07: // DIV0S Rm,Rn         0010nnnnmmmm0111
        tmp  = rcache_get_reg(SHR_SR, RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_bic_r_imm(tmp, M|Q|T);
        emith_tst_r_imm(tmp2, (1<<31));
        EMITH_SJMP_START(DCOND_EQ);
        emith_or_r_imm_c(DCOND_NE, tmp, Q);
        EMITH_SJMP_END(DCOND_EQ);
        emith_tst_r_imm(tmp3, (1<<31));
        EMITH_SJMP_START(DCOND_EQ);
        emith_or_r_imm_c(DCOND_NE, tmp, M);
        EMITH_SJMP_END(DCOND_EQ);
        emith_teq_r_r(tmp2, tmp3);
        EMITH_SJMP_START(DCOND_PL);
        emith_or_r_imm_c(DCOND_MI, tmp, T);
        EMITH_SJMP_END(DCOND_PL);
        goto end_op;
      case 0x08: // TST Rm,Rn           0010nnnnmmmm1000
        tmp  = rcache_get_reg(SHR_SR, RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_bic_r_imm(tmp, T);
        emith_tst_r_r(tmp2, tmp3);
        EMITH_SJMP_START(DCOND_NE);
        emith_or_r_imm_c(DCOND_EQ, tmp, T);
        EMITH_SJMP_END(DCOND_NE);
        goto end_op;
      case 0x09: // AND Rm,Rn           0010nnnnmmmm1001
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_and_r_r(tmp, tmp2);
        goto end_op;
      case 0x0a: // XOR Rm,Rn           0010nnnnmmmm1010
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_eor_r_r(tmp, tmp2);
        goto end_op;
      case 0x0b: // OR  Rm,Rn           0010nnnnmmmm1011
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_or_r_r(tmp, tmp2);
        goto end_op;
      case 0x0c: // CMP/STR Rm,Rn       0010nnnnmmmm1100
        tmp  = rcache_get_tmp();
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_eor_r_r_r(tmp, tmp2, tmp3);
        tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
        emith_bic_r_imm(tmp2, T);
        emith_tst_r_imm(tmp, 0x000000ff);
        EMITH_SJMP_START(DCOND_NE);
        emith_or_r_imm_c(DCOND_EQ, tmp2, T);
        EMITH_SJMP_END(DCOND_NE);
        emith_tst_r_imm(tmp, 0x0000ff00);
        EMITH_SJMP_START(DCOND_NE);
        emith_or_r_imm_c(DCOND_EQ, tmp2, T);
        EMITH_SJMP_END(DCOND_NE);
        emith_tst_r_imm(tmp, 0x00ff0000);
        EMITH_SJMP_START(DCOND_NE);
        emith_or_r_imm_c(DCOND_EQ, tmp2, T);
        EMITH_SJMP_END(DCOND_NE);
        emith_tst_r_imm(tmp, 0xff000000);
        EMITH_SJMP_START(DCOND_NE);
        emith_or_r_imm_c(DCOND_EQ, tmp2, T);
        EMITH_SJMP_END(DCOND_NE);
        rcache_free_tmp(tmp);
        goto end_op;
      case 0x0d: // XTRCT  Rm,Rn        0010nnnnmmmm1101
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_lsr(tmp, tmp, 16);
        emith_or_r_r_r_lsl(tmp, tmp, tmp2, 16);
        goto end_op;
      case 0x0e: // MULU.W Rm,Rn        0010nnnnmmmm1110
      case 0x0f: // MULS.W Rm,Rn        0010nnnnmmmm1111
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp  = rcache_get_reg(SHR_MACL, RC_GR_WRITE);
        if (op & 1) {
          emith_sext(tmp, tmp2, 16);
        } else
          emith_clear_msb(tmp, tmp2, 16);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp2 = rcache_get_tmp();
        if (op & 1) {
          emith_sext(tmp2, tmp3, 16);
        } else
          emith_clear_msb(tmp2, tmp3, 16);
        emith_mul(tmp, tmp, tmp2);
        rcache_free_tmp(tmp2);
//      FIXME: causes timing issues in Doom?
//        cycles++;
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x03:
      switch (op & 0x0f)
      {
      case 0x00: // CMP/EQ Rm,Rn        0011nnnnmmmm0000
      case 0x02: // CMP/HS Rm,Rn        0011nnnnmmmm0010
      case 0x03: // CMP/GE Rm,Rn        0011nnnnmmmm0011
      case 0x06: // CMP/HI Rm,Rn        0011nnnnmmmm0110
      case 0x07: // CMP/GT Rm,Rn        0011nnnnmmmm0111
        tmp  = rcache_get_reg(SHR_SR, RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        emith_bic_r_imm(tmp, T);
        emith_cmp_r_r(tmp2, tmp3);
        switch (op & 0x07)
        {
        case 0x00: // CMP/EQ
          EMITH_SJMP_START(DCOND_NE);
          emith_or_r_imm_c(DCOND_EQ, tmp, T);
          EMITH_SJMP_END(DCOND_NE);
          break;
        case 0x02: // CMP/HS
          EMITH_SJMP_START(DCOND_LO);
          emith_or_r_imm_c(DCOND_HS, tmp, T);
          EMITH_SJMP_END(DCOND_LO);
          break;
        case 0x03: // CMP/GE
          EMITH_SJMP_START(DCOND_LT);
          emith_or_r_imm_c(DCOND_GE, tmp, T);
          EMITH_SJMP_END(DCOND_LT);
          break;
        case 0x06: // CMP/HI
          EMITH_SJMP_START(DCOND_LS);
          emith_or_r_imm_c(DCOND_HI, tmp, T);
          EMITH_SJMP_END(DCOND_LS);
          break;
        case 0x07: // CMP/GT
          EMITH_SJMP_START(DCOND_LE);
          emith_or_r_imm_c(DCOND_GT, tmp, T);
          EMITH_SJMP_END(DCOND_LE);
          break;
        }
        goto end_op;
      case 0x04: // DIV1    Rm,Rn       0011nnnnmmmm0100
        // TODO
        break;
      case 0x05: // DMULU.L Rm,Rn       0011nnnnmmmm0101
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_WRITE);
        emith_mul_u64(tmp3, tmp4, tmp, tmp2);
        goto end_op;
      case 0x08: // SUB     Rm,Rn       0011nnnnmmmm1000
      case 0x0c: // ADD     Rm,Rn       0011nnnnmmmm1100
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        if (op & 4) {
          emith_add_r_r(tmp, tmp2);
        } else
          emith_sub_r_r(tmp, tmp2);
        goto end_op;
      case 0x0a: // SUBC    Rm,Rn       0011nnnnmmmm1010
      case 0x0e: // ADDC    Rm,Rn       0011nnnnmmmm1110
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp3 = rcache_get_reg(SHR_SR, RC_GR_RMW);
        if (op & 4) { // adc
          emith_set_carry(tmp3);
          emith_adcf_r_r(tmp, tmp2);
          emith_carry_to_t(tmp3, 0);
        } else {
          emith_set_carry_sub(tmp3);
          emith_sbcf_r_r(tmp, tmp2);
          emith_carry_to_t(tmp3, 1);
        }
        goto end_op;
      case 0x0b: // SUBV    Rm,Rn       0011nnnnmmmm1011
      case 0x0f: // ADDV    Rm,Rn       0011nnnnmmmm1111
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp3 = rcache_get_reg(SHR_SR, RC_GR_RMW);
        emith_bic_r_imm(tmp3, T);
        if (op & 4) {
          emith_addf_r_r(tmp, tmp2);
        } else
          emith_subf_r_r(tmp, tmp2);
        EMITH_SJMP_START(DCOND_VC);
        emith_or_r_imm_c(DCOND_VS, tmp3, T);
        EMITH_SJMP_END(DCOND_VC);
        goto end_op;
      case 0x0d: // DMULS.L Rm,Rn       0011nnnnmmmm1101
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_WRITE);
        emith_mul_s64(tmp3, tmp4, tmp, tmp2);
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x04:
      switch (op & 0x0f)
      {
      case 0x00:
        switch (GET_Fx())
        {
        case 0: // SHLL Rn    0100nnnn00000000
        case 2: // SHAL Rn    0100nnnn00100000
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_lslf(tmp, tmp, 1);
          emith_carry_to_t(tmp2,  0);
          goto end_op;
        case 1: // DT Rn      0100nnnn00010000
          if (p32x_sh2_read16(pc, sh2) == 0x8bfd) { // BF #-2
            emith_sh2_dtbf_loop();
            goto end_op;
          }
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_bic_r_imm(tmp2, T);
          emith_subf_r_imm(tmp, 1);
          EMITH_SJMP_START(DCOND_NE);
          emith_or_r_imm_c(DCOND_EQ, tmp2, T);
          EMITH_SJMP_END(DCOND_NE);
          goto end_op;
        }
        goto default_;
      case 0x01:
        switch (GET_Fx())
        {
        case 0: // SHLR Rn    0100nnnn00000001
        case 2: // SHAR Rn    0100nnnn00100001
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          if (op & 0x20) {
            emith_asrf(tmp, tmp, 1);
          } else
            emith_lsrf(tmp, tmp, 1);
          emith_carry_to_t(tmp2, 0);
          goto end_op;
        case 1: // CMP/PZ Rn  0100nnnn00010001
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_bic_r_imm(tmp2, T);
          emith_cmp_r_imm(tmp, 0);
          EMITH_SJMP_START(DCOND_LT);
          emith_or_r_imm_c(DCOND_GE, tmp2, T);
          EMITH_SJMP_END(DCOND_LT);
          goto end_op;
        }
        goto default_;
      case 0x02:
      case 0x03:
        switch (op & 0x3f)
        {
        case 0x02: // STS.L    MACH,@–Rn 0100nnnn00000010
          tmp = SHR_MACH;
          break;
        case 0x12: // STS.L    MACL,@–Rn 0100nnnn00010010
          tmp = SHR_MACL;
          break;
        case 0x22: // STS.L    PR,@–Rn   0100nnnn00100010
          tmp = SHR_PR;
          break;
        case 0x03: // STC.L    SR,@–Rn   0100nnnn00000011
          tmp = SHR_SR;
          break;
        case 0x13: // STC.L    GBR,@–Rn  0100nnnn00010011
          tmp = SHR_GBR;
          break;
        case 0x23: // STC.L    VBR,@–Rn  0100nnnn00100011
          tmp = SHR_VBR;
          break;
        default:
          goto default_;
        }
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        emith_sub_r_imm(tmp2, 4);
        rcache_clean();
        rcache_get_reg_arg(0, GET_Rn());
        tmp3 = rcache_get_reg_arg(1, tmp);
        if (tmp == SHR_SR)
          emith_clear_msb(tmp3, tmp3, 20); // reserved bits defined by ISA as 0
        emit_memhandler_write(2);
        goto end_op;
      case 0x04:
      case 0x05:
        switch (op & 0x3f)
        {
        case 0x04: // ROTL   Rn          0100nnnn00000100
        case 0x05: // ROTR   Rn          0100nnnn00000101
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          if (op & 1) {
            emith_rorf(tmp, tmp, 1);
          } else
            emith_rolf(tmp, tmp, 1);
          emith_carry_to_t(tmp2, 0);
          goto end_op;
        case 0x24: // ROTCL  Rn          0100nnnn00100100
        case 0x25: // ROTCR  Rn          0100nnnn00100101
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_set_carry(tmp2);
          if (op & 1) {
            emith_rorcf(tmp);
          } else
            emith_rolcf(tmp);
          emith_carry_to_t(tmp2, 0);
          goto end_op;
        case 0x15: // CMP/PL Rn          0100nnnn00010101
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW);
          tmp2 = rcache_get_reg(SHR_SR, RC_GR_RMW);
          emith_bic_r_imm(tmp2, T);
          emith_cmp_r_imm(tmp, 0);
          EMITH_SJMP_START(DCOND_LE);
          emith_or_r_imm_c(DCOND_GT, tmp2, T);
          EMITH_SJMP_END(DCOND_LE);
          goto end_op;
        }
        goto default_;
      case 0x06:
      case 0x07:
        switch (op & 0x3f)
        {
        case 0x06: // LDS.L @Rm+,MACH 0100mmmm00000110
          tmp = SHR_MACH;
          break;
        case 0x16: // LDS.L @Rm+,MACL 0100mmmm00010110
          tmp = SHR_MACL;
          break;
        case 0x26: // LDS.L @Rm+,PR   0100mmmm00100110
          tmp = SHR_PR;
          break;
        case 0x07: // LDC.L @Rm+,SR   0100mmmm00000111
          tmp = SHR_SR;
          break;
        case 0x17: // LDC.L @Rm+,GBR  0100mmmm00010111
          tmp = SHR_GBR;
          break;
        case 0x27: // LDC.L @Rm+,VBR  0100mmmm00100111
          tmp = SHR_VBR;
          break;
        default:
          goto default_;
        }
        rcache_clean();
        rcache_get_reg_arg(0, GET_Rn());
        tmp2 = emit_memhandler_read(2);
        if (tmp == SHR_SR) {
          emith_write_sr(tmp2);
          test_irq = 1;
        } else {
          tmp = rcache_get_reg(tmp, RC_GR_WRITE);
          emith_move_r_r(tmp, tmp2);
        }
        rcache_free_tmp(tmp2);
        tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW);
        emith_add_r_imm(tmp, 4);
        goto end_op;
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

    /////////////////////////////////////////////
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
      case 0x0b00: {
        // jmp_cond ~ cond when guest doesn't jump
        int jmp_cond  = (op & 0x0200) ? DCOND_NE : DCOND_EQ;
        int insn_cond = (op & 0x0200) ? DCOND_EQ : DCOND_NE;
        signed int offs = ((signed int)(op << 24) >> 23);
        tmp = rcache_get_reg(delayed_op ? SHR_PPC : SHR_PC, RC_GR_WRITE);
        emith_move_r_imm(tmp, pc + (delayed_op ? 2 : 0));
        emith_sh2_test_t();
        EMITH_SJMP_START(jmp_cond);
        if (!delayed_op)
          offs += 2;
        if (offs < 0) {
          emith_sub_r_imm_c(insn_cond, tmp, -offs);
        } else
          emith_add_r_imm_c(insn_cond, tmp, offs);
        EMITH_SJMP_END(jmp_cond);
        cycles += 2;
        if (!delayed_op)
          goto end_block;
        goto end_op;
      }}
      goto default_;

    /////////////////////////////////////////////
    case 0x0a:
      // BRA  label 1010dddddddddddd
      DELAYED_OP;
    do_bra:
      tmp = ((signed int)(op << 20) >> 19);
      emit_move_r_imm32(SHR_PPC, pc + tmp + 2);
      cycles++;
      break;

    /////////////////////////////////////////////
    case 0x0b:
      // BSR  label 1011dddddddddddd
      DELAYED_OP;
      emit_move_r_imm32(SHR_PR, pc + 2);
      goto do_bra;

    default:
    default_:
      emit_move_r_imm32(SHR_PC, pc - 2);
      rcache_flush();
      emith_pass_arg_r(0, CONTEXT_REG);
      emith_pass_arg_imm(1, op);
      emith_call(sh2_do_op);
      break;
    }

end_op:
    if (delayed_op == 1)
      emit_move_r_r(SHR_PC, SHR_PPC);

    if (test_irq && delayed_op != 2) {
      rcache_flush();
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

  tmp = rcache_get_reg(SHR_SR, RC_GR_RMW);
  emith_sub_r_imm(tmp, cycles << 12);
  rcache_flush();
  emith_jump(sh2_drc_exit);
  tcache_ptrs[tcache_id] = tcache_ptr;

#ifdef ARM
  cache_flush_d_inval_i(block_entry, tcache_ptr);
#endif

  do_host_disasm(tcache_id);
  dbg(1, " block #%d,%d tcache %d/%d, insns %d -> %d %.3f",
    tcache_id, block_counts[tcache_id],
    tcache_ptr - tcache_bases[tcache_id], tcache_sizes[tcache_id],
    insns_compiled, host_insn_count, (double)host_insn_count / insns_compiled);
  if ((sh2->pc & 0xc6000000) == 0x02000000) // ROM
    dbg(1, "  hash collisions %d/%d", hash_collisions, block_counts[tcache_id]);
#if (DRC_DEBUG & 2)
  fflush(stdout);
#endif

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

static void REGPARM(1) sh2_test_irq(SH2 *sh2)
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

  for (b = 0; b < ARRAY_SIZE(block_tables); b++)
    for (i = 0; i < block_counts[b]; i++)
      block_tables[b][i].refcount = 0;
}
#else
#define block_stats()
#endif

void sh2_drc_flush_all(void)
{
  block_stats();
  flush_tcache(0);
  flush_tcache(1);
  flush_tcache(2);
}

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

    // tmp
    PicoOpt |= POPT_DIS_VDP_FIFO;

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
    block_stats();
    free(block_tables[0]);
    memset(block_tables, 0, sizeof(block_tables));

    drc_cmn_cleanup();
  }

  if (hash_table != NULL) {
    free(hash_table);
    hash_table = NULL;
  }
}
