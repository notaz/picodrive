/*
 * SH2 recompiler
 * (C) notaz, 2009,2010,2013
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 *
 * notes:
 * - tcache, block descriptor, link buffer overflows result in sh2_translate()
 *   failure, followed by full tcache invalidation for that region
 * - jumps between blocks are tracked for SMC handling (in block_entry->links),
 *   except jumps between different tcaches
 *
 * implemented:
 * - static register allocation
 * - remaining register caching and tracking in temporaries
 * - block-local branch linking
 * - block linking (except between tcaches)
 * - some constant propagation
 *
 * TODO:
 * - better constant propagation
 * - stack caching?
 * - bug fixing
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../../pico/pico_int.h"
#include "../../pico/arm_features.h"
#include "sh2.h"
#include "compiler.h"
#include "../drc/cmn.h"
#include "../debug.h"

// features
#define PROPAGATE_CONSTANTS     1
#define LINK_BRANCHES           1
#define BRANCH_CACHE            1
#define CALL_STACK              0
#define ALIAS_REGISTERS         1
#define REMAP_REGISTER          1
#define LOOP_DETECTION          1

// limits (per block)
#define MAX_BLOCK_SIZE          (BLOCK_INSN_LIMIT * 6 * 6)

// max literal offset from the block end
#define MAX_LITERAL_OFFSET      0x200	// max. MOVA, MOV @(PC) offset
#define MAX_LITERALS            (BLOCK_INSN_LIMIT / 4)
#define MAX_LOCAL_BRANCHES      (BLOCK_INSN_LIMIT / 4)

// debug stuff
// 01 - warnings/errors
// 02 - block info/smc
// 04 - asm
// 08 - runtime block entry log
// 10 - smc self-check
// 20 - runtime block entry counter
// 80 - branch cache statistics
// 100 - write trace
// 200 - compare trace
// 400 - block entry backtrace on exit
// 800 - state dump on exit
// {
#ifndef DRC_DEBUG
#define DRC_DEBUG 0x0
#endif

#if DRC_DEBUG
#define dbg(l,...) { \
  if ((l) & DRC_DEBUG) \
    elprintf(EL_STATUS, ##__VA_ARGS__); \
}
#include "mame/sh2dasm.h"
#include <platform/libpicofe/linux/host_dasm.h>
static int insns_compiled, hash_collisions, host_insn_count;
#define COUNT_OP \
	host_insn_count++
#else // !DRC_DEBUG
#define COUNT_OP
#define dbg(...)
#endif


///
#define FETCH_OP(pc) \
  dr_pc_base[(pc) / 2]

#define FETCH32(a) \
  ((dr_pc_base[(a) / 2] << 16) | dr_pc_base[(a) / 2 + 1])

#define CHECK_UNHANDLED_BITS(mask, label) { \
  if ((op & (mask)) != 0) \
    goto label; \
}

#define GET_Fx() \
  ((op >> 4) & 0x0f)

#define GET_Rm GET_Fx

#define GET_Rn() \
  ((op >> 8) & 0x0f)

#define BITMASK1(v0) (1 << (v0))
#define BITMASK2(v0,v1) ((1 << (v0)) | (1 << (v1)))
#define BITMASK3(v0,v1,v2) (BITMASK2(v0,v1) | (1 << (v2)))
#define BITMASK4(v0,v1,v2,v3) (BITMASK3(v0,v1,v2) | (1 << (v3)))
#define BITMASK5(v0,v1,v2,v3,v4) (BITMASK4(v0,v1,v2,v3) | (1 << (v4)))
#define BITMASK6(v0,v1,v2,v3,v4,v5) (BITMASK5(v0,v1,v2,v3,v4) | (1 << (v5)))
#define BITRANGE(v0,v1) (BITMASK1(v1+1)-BITMASK1(v0)) // set with v0..v1

#define SHR_T	SHR_SR // might make them separate someday
#define SHR_MEM	31
#define SHR_TMP -1

#define T	0x00000001
#define S	0x00000002
#define I	0x000000f0
#define Q	0x00000100
#define M	0x00000200
#define T_save	0x00000800

#define I_SHIFT 4
#define Q_SHIFT 8
#define M_SHIFT 9

static struct op_data {
  u8 op;
  u8 cycles;
  u8 size;     // 0, 1, 2 - byte, word, long
  s8 rm;       // branch or load/store data reg
  u32 source;  // bitmask of src regs
  u32 dest;    // bitmask of dest regs
  u32 imm;     // immediate/io address/branch target
               // (for literal - address, not value)
} ops[BLOCK_INSN_LIMIT];

enum op_types {
  OP_UNHANDLED = 0,
  OP_BRANCH,
  OP_BRANCH_N,  // conditional known not to be taken
  OP_BRANCH_CT, // conditional, branch if T set
  OP_BRANCH_CF, // conditional, branch if T clear
  OP_BRANCH_R,  // indirect
  OP_BRANCH_RF, // indirect far (PC + Rm)
  OP_SETCLRT,   // T flag set/clear
  OP_MOVE,      // register move
  OP_LOAD_CONST,// load const to register
  OP_LOAD_POOL, // literal pool load, imm is address
  OP_MOVA,      // MOVA instruction
  OP_SLEEP,     // SLEEP instruction
  OP_RTE,       // RTE instruction
  OP_TRAPA,     // TRAPA instruction
  OP_LDC,       // LDC instruction
  OP_UNDEFINED,
};

#define OP_ISBRANCH(op) (BITRANGE(OP_BRANCH, OP_BRANCH_RF) & BITMASK1(op))
#define OP_ISBRAUC(op) (BITMASK4(OP_BRANCH, OP_BRANCH_R, OP_BRANCH_RF, OP_RTE) \
                                & BITMASK1(op))
#define OP_ISBRACND(op) (BITMASK2(OP_BRANCH_CT, OP_BRANCH_CF) & BITMASK1(op))
#define OP_ISBRAIMM(op) (BITMASK3(OP_BRANCH, OP_BRANCH_CT, OP_BRANCH_CF) \
				& BITMASK1(op))
#define OP_ISBRAIND(op) (BITMASK2(OP_BRANCH_R, OP_BRANCH_RF) & BITMASK1(op))

#ifdef DRC_SH2

#if (DRC_DEBUG & 4)
static u8 *tcache_dsm_ptrs[3];
static char sh2dasm_buff[64];
#define do_host_disasm(tcid) \
  host_dasm(tcache_dsm_ptrs[tcid], emith_insn_ptr() - tcache_dsm_ptrs[tcid]); \
  tcache_dsm_ptrs[tcid] = emith_insn_ptr()
#else
#define do_host_disasm(x)
#endif

#define SH2_DUMP(sh2, reason) { \
	char ms = (sh2)->is_slave ? 's' : 'm'; \
	printf("%csh2 %s %08x\n", ms, reason, (sh2)->pc); \
	printf("%csh2 r0-7  %08x %08x %08x %08x %08x %08x %08x %08x\n", ms, \
		(sh2)->r[0], (sh2)->r[1], (sh2)->r[2], (sh2)->r[3], \
		(sh2)->r[4], (sh2)->r[5], (sh2)->r[6], (sh2)->r[7]); \
	printf("%csh2 r8-15 %08x %08x %08x %08x %08x %08x %08x %08x\n", ms, \
		(sh2)->r[8], (sh2)->r[9], (sh2)->r[10], (sh2)->r[11], \
		(sh2)->r[12], (sh2)->r[13], (sh2)->r[14], (sh2)->r[15]); \
	printf("%csh2 pc-ml %08x %08x %08x %08x %08x %08x %08x %08x\n", ms, \
		(sh2)->pc, (sh2)->ppc, (sh2)->pr, (sh2)->sr&0x3ff, \
		(sh2)->gbr, (sh2)->vbr, (sh2)->mach, (sh2)->macl); \
	printf("%csh2 tmp-p  %08x %08x %08x %08x %08x %08x %08x %08x\n", ms, \
		(sh2)->drc_tmp, (sh2)->irq_cycles, \
		(sh2)->pdb_io_csum[0], (sh2)->pdb_io_csum[1], (sh2)->state, \
		(sh2)->poll_addr, (sh2)->poll_cycles, (sh2)->poll_cnt); \
}

#if (DRC_DEBUG & (8|256|512|1024)) || defined(PDB)
static SH2 csh2[2][8];
static void REGPARM(3) *sh2_drc_log_entry(void *block, SH2 *sh2, u32 sr)
{
  if (block != NULL) {
    dbg(8, "= %csh2 enter %08x %p, c=%d", sh2->is_slave ? 's' : 'm',
      sh2->pc, block, (signed int)sr >> 12);
#if defined PDB
    pdb_step(sh2, sh2->pc);
#elif (DRC_DEBUG & 256)
  {
    static FILE *trace[2];
    int idx = sh2->is_slave;
    if (!trace[0]) {
      truncate("pico.trace", 0);
      trace[0] = fopen("pico.trace0", "wb");
      trace[1] = fopen("pico.trace1", "wb");
    }
    if (csh2[idx][0].pc != sh2->pc) {
      fwrite(sh2, offsetof(SH2, read8_map), 1, trace[idx]);
      fwrite(&sh2->pdb_io_csum, sizeof(sh2->pdb_io_csum), 1, trace[idx]);
      memcpy(&csh2[idx][0], sh2, offsetof(SH2, poll_cnt)+4);
      csh2[idx][0].is_slave = idx;
    }
  }
#elif (DRC_DEBUG & 512)
  {
    static FILE *trace[2];
    static SH2 fsh2;
    int idx = sh2->is_slave;
    if (!trace[0]) {
      trace[0] = fopen("pico.trace0", "rb");
      trace[1] = fopen("pico.trace1", "rb");
    }
    if (csh2[idx][0].pc != sh2->pc) {
      if (!fread(&fsh2, offsetof(SH2, read8_map), 1, trace[idx]) ||
          !fread(&fsh2.pdb_io_csum, sizeof(sh2->pdb_io_csum), 1, trace[idx])) {
        printf("trace eof at %08lx\n",ftell(trace[idx]));
        exit(1);
      }
      fsh2.sr = (fsh2.sr & 0xfff) | (sh2->sr & ~0xfff);
      fsh2.is_slave = idx;
      if (memcmp(&fsh2, sh2, offsetof(SH2, read8_map)) ||
          0)//memcmp(&fsh2.pdb_io_csum, &sh2->pdb_io_csum, sizeof(sh2->pdb_io_csum)))
      {
        printf("difference at %08lx!\n",ftell(trace[idx]));
        SH2_DUMP(&fsh2, "file");
        SH2_DUMP(sh2, "current");
        SH2_DUMP(&csh2[idx][0], "previous");
        exit(1);
      }
      csh2[idx][0] = fsh2;
    }
  }
#elif (DRC_DEBUG & 1024)
  {
    int x = sh2->is_slave, i;
    for (i = 0; i < ARRAY_SIZE(csh2[x])-1; i++)
      memcpy(&csh2[x][i], &csh2[x][i+1], offsetof(SH2, poll_cnt)+4);
    memcpy(&csh2[x][ARRAY_SIZE(csh2[x])-1], sh2, offsetof(SH2, poll_cnt)+4);
    csh2[x][0].is_slave = x;
  }
#endif
  }
  return block;
}
#endif
// } debug

#define TCACHE_BUFFERS 3

// we have 3 translation cache buffers, split from one drc/cmn buffer.
// BIOS shares tcache with data array because it's only used for init
// and can be discarded early
// XXX: need to tune sizes
static const int tcache_sizes[TCACHE_BUFFERS] = {
  DRC_TCACHE_SIZE * 14 / 16, // ROM (rarely used), DRAM
  DRC_TCACHE_SIZE / 16, // BIOS, data array in master sh2
  DRC_TCACHE_SIZE / 16, // ... slave
};

static u8 *tcache_bases[TCACHE_BUFFERS];
static u8 *tcache_ptrs[TCACHE_BUFFERS];
static u8 *tcache_limit[TCACHE_BUFFERS];

// ptr for code emiters
static u8 *tcache_ptr;

#define MAX_BLOCK_ENTRIES (BLOCK_INSN_LIMIT / 8)

struct block_link {
  u32 target_pc;
  void *jump;                // insn address
  struct block_link *next;   // either in block_entry->links or unresolved
  struct block_link *o_next; //     ...in block_entry->o_links
  struct block_link *prev;
  struct block_link *o_prev;
  struct block_entry *target;// target block this is linked in (be->links)
  int tcache_id;
};

struct block_entry {
  u32 pc;
  u8 *tcache_ptr;            // translated block for above PC
  struct block_entry *next;  // chain in hash_table with same pc hash
  struct block_entry *prev;
  struct block_link *links;  // incoming links to this entry
  struct block_link *o_links;// outgoing links from this entry
#if (DRC_DEBUG & 2)
  struct block_desc *block;
#endif
#if (DRC_DEBUG & 32)
  int entry_count;
#endif
};

struct block_desc {
  u32 addr;                  // block start SH2 PC address
  u32 addr_lit;              // block start SH2 literal pool addr
  int size;                  // ..of recompiled insns
  int size_lit;              // ..of (insns+)literal pool
  u8 *tcache_ptr;            // start address of block in cache
  u16 crc;                   // crc of insns and literals
  u16 active;                // actively used or deactivated?
  struct block_list *list;
#if (DRC_DEBUG & 2)
  int refcount;
#endif
  int entry_count;
  struct block_entry entryp[MAX_BLOCK_ENTRIES];
};

static const int block_max_counts[TCACHE_BUFFERS] = {
  4*1024,
  256,
  256,
};
static struct block_desc *block_tables[TCACHE_BUFFERS];
static int block_counts[TCACHE_BUFFERS];
static int block_limit[TCACHE_BUFFERS];

// we have block_link_pool to avoid using mallocs
static const int block_link_pool_max_counts[TCACHE_BUFFERS] = {
  16*1024,
  4*256,
  4*256,
};
static struct block_link *block_link_pool[TCACHE_BUFFERS]; 
static int block_link_pool_counts[TCACHE_BUFFERS];
static struct block_link **unresolved_links[TCACHE_BUFFERS];
static struct block_link *blink_free[TCACHE_BUFFERS];

// used for invalidation
static const int ram_sizes[TCACHE_BUFFERS] = {
  0x40000,
  0x1000,
  0x1000,
};
#define INVAL_PAGE_SIZE 0x100

struct block_list {
  struct block_desc *block;
  struct block_list *next;
  struct block_list *prev;
  struct block_list **head;
  struct block_list *l_next;
};
struct block_list *blist_free;

static struct block_list *inactive_blocks[TCACHE_BUFFERS];

// array of pointers to block_lists for RAM and 2 data arrays
// each array has len: sizeof(mem) / INVAL_PAGE_SIZE 
static struct block_list **inval_lookup[TCACHE_BUFFERS];

static const int hash_table_sizes[TCACHE_BUFFERS] = {
  0x4000,
  0x100,
  0x100,
};
static struct block_entry **hash_tables[TCACHE_BUFFERS];

#define HASH_FUNC(hash_tab, addr, mask) \
  (hash_tab)[(((addr) >> 20) ^ ((addr) >> 2)) & (mask)]

#if (DRC_DEBUG & 128)
#if BRANCH_CACHE
int bchit, bcmiss;
#endif
#if CALL_STACK
int rchit, rcmiss;
#endif
#endif

// host register tracking
enum {
  HR_FREE,
  HR_STATIC, // vreg has a static mapping
  HR_CACHED, // vreg has sh2_reg_e
  HR_TEMP,   // reg used for temp storage
} cache_reg_type;

enum {
  HRF_DIRTY  = 1 << 0, // has "dirty" value to be written to ctx
  HRF_LOCKED = 1 << 1, // can't be evicted
  HRF_TEMP   = 1 << 2, // is for temps and args
  HRF_REG    = 1 << 3, // is for sh2 regs
} cache_reg_flags;

typedef struct {
  u8 hreg;      // "host" reg
  u8 flags:4;   // TEMP or REG?
  u8 type:4;
  u16 stamp;    // kind of a timestamp
  u32 gregs;    // "guest" reg mask
} cache_reg_t;

// guest register tracking
enum {
  GRF_DIRTY  = 1 << 0, // reg has "dirty" value to be written to ctx
  GRF_CONST  = 1 << 1, // reg has a constant
  GRF_CDIRTY = 1 << 2, // constant not yet written to ctx
  GRF_STATIC = 1 << 3, // reg has static mapping to vreg
} guest_reg_flags;

typedef struct {
  u8 flags;     // guest flags: is constant, is dirty?
  s8 sreg;      // cache reg for static mapping
  s8 vreg;      // cache_reg this is currently mapped to, -1 if not mapped
  s8 cnst;      // const index if this is constant
} guest_reg_t;


// note: cache_regs[] must have at least the amount of
// HRF_REG registers used by handlers in worst case (currently 4)
#ifdef __arm__
#include "../drc/emit_arm.c"

// register assigment goes by ABI convention. All caller save registers are TEMP
// the others are either static or REG. SR must be static, R0 very recommended
static guest_reg_t guest_regs[] = {
  // SHR_R0 .. SHR_SP
#ifndef __MACH__ // no r9..
  { GRF_STATIC, 8 }, { GRF_STATIC, 9 }, { 0 }            , { 0 }            ,
#else
  { GRF_STATIC, 8 }, { 0 }            , { 0 }            , { 0 }            ,
#endif
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  // SHR_PC,  SHR_PPC, SHR_PR,   SHR_SR,
  // SHR_GBR, SHR_VBR, SHR_MACH, SHR_MACL,
  { 0 }            , { 0 }            , { 0 }            , { GRF_STATIC, 10 },
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
};

// NB first TEMP, then REG. alloc/evict algorithm depends on this
static cache_reg_t cache_regs[] = {
  { 12, HRF_TEMP },
  { 14, HRF_TEMP },
  {  0, HRF_TEMP },
  {  1, HRF_TEMP },
  {  2, HRF_TEMP },
  {  3, HRF_TEMP },
  {  8, HRF_LOCKED },
#ifndef __MACH__ // no r9..
  {  9, HRF_LOCKED },
#endif
  { 10, HRF_LOCKED },
  {  4, HRF_REG },
  {  5, HRF_REG },
  {  6, HRF_REG },
  {  7, HRF_REG },
};

#elif defined(__i386__)
#include "../drc/emit_x86.c"

static guest_reg_t guest_regs[] = {
  // SHR_R0 .. SHR_SP
  {GRF_STATIC, xSI}, { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  // SHR_PC,  SHR_PPC, SHR_PR,   SHR_SR,
  // SHR_GBR, SHR_VBR, SHR_MACH, SHR_MACL,
  { 0 }            , { 0 }            , { 0 }            , {GRF_STATIC, xDI},
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
};

// ax, cx, dx are usually temporaries by convention
static cache_reg_t cache_regs[] = {
  { xBX, HRF_REG|HRF_TEMP },
  { xCX, HRF_REG|HRF_TEMP },
  { xDX, HRF_REG|HRF_TEMP },
  { xAX, HRF_REG|HRF_TEMP },
  { xSI, HRF_LOCKED },
  { xDI, HRF_LOCKED },
};

#elif defined(__x86_64__)
#include "../drc/emit_x86.c"

static guest_reg_t guest_regs[] = {
  // SHR_R0 .. SHR_SP
#ifndef _WIN32
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
#else
  {GRF_STATIC, xDI}, { 0 }            , { 0 }            , { 0 }            ,
#endif
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
  // SHR_PC,  SHR_PPC, SHR_PR,   SHR_SR,
  // SHR_GBR, SHR_VBR, SHR_MACH, SHR_MACL,
  { 0 }            , { 0 }            , { 0 }            , {GRF_STATIC, xBX},
  { 0 }            , { 0 }            , { 0 }            , { 0 }            ,
};

// ax, cx, dx are usually temporaries by convention
static cache_reg_t cache_regs[] = {
  { xCX, HRF_REG|HRF_TEMP },
  { xDX, HRF_REG|HRF_TEMP },
  { xAX, HRF_REG|HRF_TEMP },
  { xSI, HRF_REG|HRF_TEMP },
#ifndef _WIN32
  { xDI, HRF_REG|HRF_TEMP },
#else
  { xDI, HRF_LOCKED },
#endif
  { xBX, HRF_LOCKED },
};

#else
#error unsupported arch
#endif

static signed char reg_map_host[HOST_REGS];

static void REGPARM(1) (*sh2_drc_entry)(SH2 *sh2);
static void REGPARM(1) (*sh2_drc_dispatcher)(u32 pc);
#if CALL_STACK
static void REGPARM(2) (*sh2_drc_dispatcher_call)(u32 pc, uptr host_pr);
static void REGPARM(1) (*sh2_drc_dispatcher_return)(u32 pc);
#endif
static void REGPARM(1) (*sh2_drc_exit)(u32 pc);
static void            (*sh2_drc_test_irq)(void);

static u32  REGPARM(1) (*sh2_drc_read8)(u32 a);
static u32  REGPARM(1) (*sh2_drc_read16)(u32 a);
static u32  REGPARM(1) (*sh2_drc_read32)(u32 a);
static u32  REGPARM(1) (*sh2_drc_read8_poll)(u32 a);
static u32  REGPARM(1) (*sh2_drc_read16_poll)(u32 a);
static u32  REGPARM(1) (*sh2_drc_read32_poll)(u32 a);
static void REGPARM(2) (*sh2_drc_write8)(u32 a, u32 d);
static void REGPARM(2) (*sh2_drc_write16)(u32 a, u32 d);
static void REGPARM(2) (*sh2_drc_write32)(u32 a, u32 d);

// flags for memory access
#define MF_SIZEMASK 0x03        // size of access
#define MF_POSTINCR 0x10        // post increment (for read_rr)
#define MF_PREDECR  MF_POSTINCR // pre decrement (for write_rr)
#define MF_POLLING  0x20	// include polling check in read

// address space stuff
static int dr_is_rom(u32 a)
{
  // tweak for WWF Raw which writes data to some high ROM addresses
  return (a & 0xc6000000) == 0x02000000 && (a & 0x3f0000) < 0x3e0000;
}

static int dr_ctx_get_mem_ptr(SH2 *sh2, u32 a, u32 *mask)
{
  void *memptr;
  int poffs = -1;

  // check if region is mapped memory
  memptr = p32x_sh2_get_mem_ptr(a, mask, sh2);
  if (memptr == NULL /*|| (a & ((1 << SH2_READ_SHIFT)-1) & ~*mask) != 0*/)
    return poffs;

  if (memptr == sh2->p_bios)        // BIOS
    poffs = offsetof(SH2, p_bios);
  else if (memptr == sh2->p_da)     // data array
    // FIXME: access sh2->data_array instead
    poffs = offsetof(SH2, p_da);
  else if (memptr == sh2->p_sdram)  // SDRAM
    poffs = offsetof(SH2, p_sdram);
  else if (memptr == sh2->p_rom)    // ROM
    poffs = offsetof(SH2, p_rom);

  return poffs;
}

static struct block_entry *dr_get_entry(u32 pc, int is_slave, int *tcache_id)
{
  struct block_entry *be;
  u32 tcid = 0, mask;

  // data arrays have their own caches
  if ((pc & 0xe0000000) == 0xc0000000 || (pc & ~0xfff) == 0)
    tcid = 1 + is_slave;

  *tcache_id = tcid;

  mask = hash_table_sizes[tcid] - 1;
  be = HASH_FUNC(hash_tables[tcid], pc, mask);
  for (; be != NULL; be = be->next)
    if (be->pc == pc)
      return be;

  return NULL;
}

// ---------------------------------------------------------------

// block management
static void add_to_block_list(struct block_list **blist, struct block_desc *block)
{
  struct block_list *added;

  if (blist_free) {
    added = blist_free;
    blist_free = added->next;
  } else {
    added = malloc(sizeof(*added));
  }
  if (!added) {
    elprintf(EL_ANOMALY, "drc OOM (1)");
    return;
  }
  added->block = block;
  added->l_next = block->list;
  block->list = added;
  added->head = blist;

  added->prev = NULL;
  if (*blist)
    (*blist)->prev = added;
  added->next = *blist;
  *blist = added;
}

static void rm_from_block_lists(struct block_desc *block)
{
  struct block_list *entry;

  entry = block->list;
  while (entry != NULL) {
    if (entry->prev != NULL)
      entry->prev->next = entry->next;
    else
      *(entry->head) = entry->next;
    if (entry->next != NULL)
      entry->next->prev = entry->prev;

    entry->next = blist_free;
    blist_free = entry;

    entry = entry->l_next;
  }
  block->list = NULL;
}

static void rm_block_list(struct block_list **blist)
{
  struct block_list *next, *current = *blist;
  while (current != NULL) {
    next = current->next;
    current->next = blist_free;
    blist_free = current;
    current = next;
  }
  *blist = NULL;
}

static void REGPARM(1) flush_tcache(int tcid)
{
  int i;
#if (DRC_DEBUG & 1)
  int tc_used, bl_used;

  tc_used = tcache_sizes[tcid] - (tcache_limit[tcid] - tcache_ptrs[tcid]);
  bl_used = block_max_counts[tcid] - (block_limit[tcid] - block_counts[tcid]);
  elprintf(EL_STATUS, "tcache #%d flush! (%d/%d, bds %d/%d)", tcid, tc_used,
    tcache_sizes[tcid], bl_used, block_max_counts[tcid]);
#endif

  block_counts[tcid] = 0;
  block_limit[tcid] = block_max_counts[tcid] - 1;
  block_link_pool_counts[tcid] = 0;
  blink_free[tcid] = NULL;
  memset(unresolved_links[tcid], 0, sizeof(*unresolved_links[0]) * hash_table_sizes[tcid]);
  memset(hash_tables[tcid], 0, sizeof(*hash_tables[0]) * hash_table_sizes[tcid]);
  tcache_ptrs[tcid] = tcache_bases[tcid];
  tcache_limit[tcid] = tcache_bases[tcid] + tcache_sizes[tcid];
  if (Pico32xMem->sdram != NULL) {
    if (tcid == 0) { // ROM, RAM
      memset(Pico32xMem->drcblk_ram, 0, sizeof(Pico32xMem->drcblk_ram));
      memset(Pico32xMem->drclit_ram, 0, sizeof(Pico32xMem->drclit_ram));
      memset(sh2s[0].branch_cache, -1, sizeof(sh2s[0].branch_cache));
      memset(sh2s[1].branch_cache, -1, sizeof(sh2s[1].branch_cache));
      memset(sh2s[0].rts_cache, -1, sizeof(sh2s[0].rts_cache));
      memset(sh2s[1].rts_cache, -1, sizeof(sh2s[1].rts_cache));
      sh2s[0].rts_cache_idx = sh2s[1].rts_cache_idx = 0;
    } else {
      memset(Pico32xMem->drcblk_ram, 0, sizeof(Pico32xMem->drcblk_ram));
      memset(Pico32xMem->drclit_ram, 0, sizeof(Pico32xMem->drclit_ram));
      memset(Pico32xMem->drcblk_da[tcid - 1], 0, sizeof(Pico32xMem->drcblk_da[tcid - 1]));
      memset(Pico32xMem->drclit_da[tcid - 1], 0, sizeof(Pico32xMem->drclit_da[tcid - 1]));
      memset(sh2s[tcid - 1].branch_cache, -1, sizeof(sh2s[0].branch_cache));
      memset(sh2s[tcid - 1].rts_cache, -1, sizeof(sh2s[0].rts_cache));
      sh2s[tcid - 1].rts_cache_idx = 0;
    }
  }
#if (DRC_DEBUG & 4)
  tcache_dsm_ptrs[tcid] = tcache_bases[tcid];
#endif

  for (i = 0; i < ram_sizes[tcid] / INVAL_PAGE_SIZE; i++)
    rm_block_list(&inval_lookup[tcid][i]);
  rm_block_list(&inactive_blocks[tcid]);
}

static void add_to_hashlist(struct block_entry *be, int tcache_id)
{
  u32 tcmask = hash_table_sizes[tcache_id] - 1;
  struct block_entry **head = &HASH_FUNC(hash_tables[tcache_id], be->pc, tcmask);

  be->prev = NULL;
  if (*head)
    (*head)->prev = be;
  be->next = *head;
  *head = be;

#if (DRC_DEBUG & 2)
  if (be->next != NULL) {
    printf(" %08x: entry hash collision with %08x\n",
      be->pc, be->next->pc);
    hash_collisions++;
  }
#endif
}

static void rm_from_hashlist(struct block_entry *be, int tcache_id)
{
  u32 tcmask = hash_table_sizes[tcache_id] - 1;
  struct block_entry **head = &HASH_FUNC(hash_tables[tcache_id], be->pc, tcmask);

#if DRC_DEBUG & 1
  struct block_entry *current = be;
  while (current->prev != NULL)
    current = current->prev;
  if (current != *head)
    dbg(1, "rm_from_hashlist @%p: be %p %08x missing?", head, be, be->pc);
#endif

  if (be->prev != NULL)
    be->prev->next = be->next;
  else
    *head = be->next;
  if (be->next != NULL)
    be->next->prev = be->prev;
}


static void add_to_hashlist_unresolved(struct block_link *bl, int tcache_id)
{
  u32 tcmask = hash_table_sizes[tcache_id] - 1;
  struct block_link **head = &HASH_FUNC(unresolved_links[tcache_id], bl->target_pc, tcmask);

#if DRC_DEBUG & 1
  struct block_link *current = *head;
  while (current != NULL && current != bl)
    current = current->next;
  if (current == bl)
    dbg(1, "add_to_hashlist_unresolved @%p: bl %p %p %08x already in?", head, bl, bl->target, bl->target_pc);
#endif

  bl->target = NULL; // marker for not resolved
  bl->prev = NULL;
  if (*head)
    (*head)->prev = bl;
  bl->next = *head;
  *head = bl;
}

static void rm_from_hashlist_unresolved(struct block_link *bl, int tcache_id)
{
  u32 tcmask = hash_table_sizes[tcache_id] - 1;
  struct block_link **head = &HASH_FUNC(unresolved_links[tcache_id], bl->target_pc, tcmask);

#if DRC_DEBUG & 1
  struct block_link *current = bl;
  while (current->prev != NULL)
    current = current->prev;
  if (current != *head)
    dbg(1, "rm_from_hashlist_unresolved @%p: bl %p %p %08x missing?", head, bl, bl->target, bl->target_pc);
#endif

  if (bl->prev != NULL)
    bl->prev->next = bl->next;
  else
    *head = bl->next;
  if (bl->next != NULL)
    bl->next->prev = bl->prev;
}

static void sh2_smc_rm_block_entry(struct block_desc *bd, int tcache_id, u32 nolit, int free);
static void dr_free_oldest_block(int tcache_id)
{
  struct block_desc *bd;

  if (block_limit[tcache_id] >= block_max_counts[tcache_id]) {
    // block desc wrap around
    block_limit[tcache_id] = 0;
  }
  bd = &block_tables[tcache_id][block_limit[tcache_id]];

  if (bd->tcache_ptr && bd->tcache_ptr < tcache_ptrs[tcache_id]) {
    // cache wrap around
    tcache_ptrs[tcache_id] = bd->tcache_ptr;
  }

  if (bd->addr && bd->entry_count)
    sh2_smc_rm_block_entry(bd, tcache_id, 0, 1);

  block_limit[tcache_id]++;
  if (block_limit[tcache_id] >= block_max_counts[tcache_id])
    block_limit[tcache_id] = 0;
  bd = &block_tables[tcache_id][block_limit[tcache_id]];
  if (bd->tcache_ptr >= tcache_ptrs[tcache_id])
    tcache_limit[tcache_id] = bd->tcache_ptr;
  else
    tcache_limit[tcache_id] = tcache_bases[tcache_id] + tcache_sizes[tcache_id];
}

static u8 *dr_prepare_cache(int tcache_id, int insn_count)
{
  u8 *limit = tcache_limit[tcache_id];

  // if no block desc available
  if (block_counts[tcache_id] == block_limit[tcache_id])
    dr_free_oldest_block(tcache_id);

  // while not enough cache space left (limit - tcache_ptr < max space needed)
  while (tcache_limit[tcache_id] - tcache_ptrs[tcache_id] < insn_count * 128)
    dr_free_oldest_block(tcache_id);

  if (limit != tcache_limit[tcache_id]) {
#if BRANCH_CACHE
    if (tcache_id)
      memset32(sh2s[tcache_id-1].branch_cache, -1, sizeof(sh2s[0].branch_cache)/4);
    else {
      memset32(sh2s[0].branch_cache, -1, sizeof(sh2s[0].branch_cache)/4);
      memset32(sh2s[1].branch_cache, -1, sizeof(sh2s[1].branch_cache)/4);
    }
#endif
#if CALL_STACK
    if (tcache_id) {
      memset32(sh2s[tcache_id-1].rts_cache, -1, sizeof(sh2s[0].rts_cache)/4);
      sh2s[tcache_id-1].rts_cache_idx = 0;
    } else {
      memset32(sh2s[0].rts_cache, -1, sizeof(sh2s[0].rts_cache)/4);
      memset32(sh2s[1].rts_cache, -1, sizeof(sh2s[1].rts_cache)/4);
      sh2s[0].rts_cache_idx = sh2s[1].rts_cache_idx = 0;
    }
#endif
  }
  return (u8 *)tcache_ptrs[tcache_id];
}

static void dr_mark_memory(int mark, struct block_desc *block, int tcache_id, u32 nolit)
{
  u8 *drc_ram_blk = NULL, *lit_ram_blk = NULL;
  u32 addr, end, mask = 0, shift = 0, idx;

  // mark memory blocks as containing compiled code
  if ((block->addr & 0xc7fc0000) == 0x06000000
      || (block->addr & 0xfffff000) == 0xc0000000)
  {
    if (tcache_id != 0) {
      // data array
      drc_ram_blk = Pico32xMem->drcblk_da[tcache_id-1];
      lit_ram_blk = Pico32xMem->drclit_da[tcache_id-1];
      shift = SH2_DRCBLK_DA_SHIFT;
    }
    else {
      // SDRAM
      drc_ram_blk = Pico32xMem->drcblk_ram;
      lit_ram_blk = Pico32xMem->drclit_ram;
      shift = SH2_DRCBLK_RAM_SHIFT;
    }
    mask = ram_sizes[tcache_id] - 1;

    // mark recompiled insns
    addr = block->addr & ~((1 << shift) - 1);
    end = block->addr + block->size;
    for (idx = (addr & mask) >> shift; addr < end; addr += (1 << shift))
      drc_ram_blk[idx++] += mark;

    // mark literal pool
    if (addr < (block->addr_lit & ~((1 << shift) - 1)))
      addr = block->addr_lit & ~((1 << shift) - 1);
    end = block->addr_lit + block->size_lit;
    for (idx = (addr & mask) >> shift; addr < end; addr += (1 << shift))
      drc_ram_blk[idx++] += mark;

    // mark for literals disabled
    if (nolit) {
      addr = nolit & ~((1 << shift) - 1);
      end = block->addr_lit + block->size_lit;
      for (idx = (addr & mask) >> shift; addr < end; addr += (1 << shift))
        lit_ram_blk[idx++] = 1;
    }

    if (mark < 0)
      rm_from_block_lists(block);
    else {
      // add to invalidation lookup lists
      addr = block->addr & ~(INVAL_PAGE_SIZE - 1);
      end = block->addr + block->size;
      for (idx = (addr & mask) / INVAL_PAGE_SIZE; addr < end; addr += INVAL_PAGE_SIZE)
        add_to_block_list(&inval_lookup[tcache_id][idx++], block);

      if (addr < (block->addr_lit & ~(INVAL_PAGE_SIZE - 1)))
        addr = block->addr_lit & ~(INVAL_PAGE_SIZE - 1);
      end = block->addr_lit + block->size_lit;
      for (idx = (addr & mask) / INVAL_PAGE_SIZE; addr < end; addr += INVAL_PAGE_SIZE)
        add_to_block_list(&inval_lookup[tcache_id][idx++], block);
    }
  }
}

static u32 dr_check_nolit(u32 start, u32 end, int tcache_id)
{
  u8 *lit_ram_blk = NULL;
  u32 mask = 0, shift = 0, addr, idx;

  if ((start & 0xc7fc0000) == 0x06000000
      || (start & 0xfffff000) == 0xc0000000)
  {
    if (tcache_id != 0) {
      // data array
      lit_ram_blk = Pico32xMem->drclit_da[tcache_id-1];
      shift = SH2_DRCBLK_DA_SHIFT;
    }
    else {
      // SDRAM
      lit_ram_blk = Pico32xMem->drclit_ram;
      shift = SH2_DRCBLK_RAM_SHIFT;
    }
    mask = ram_sizes[tcache_id] - 1;

    addr = start & ~((1 << shift) - 1);
    for (idx = (addr & mask) >> shift; addr < end; addr += (1 << shift))
      if (lit_ram_blk[idx++])
        break;

    return (addr < start ? start : addr > end ? end : addr);
  }

  return end;
}

static struct block_desc *dr_find_inactive_block(int tcache_id, u16 crc,
  u32 addr, int size, u32 addr_lit, int size_lit)
{
  struct block_list **head = &inactive_blocks[tcache_id];
  struct block_list *current;

  for (current = *head; current != NULL; current = current->next) {
    struct block_desc *block = current->block;
    if (block->crc == crc && block->addr == addr && block->size == size &&
        block->addr_lit == addr_lit && block->size_lit == size_lit)
    {
      rm_from_block_lists(block);
      return block;
    }
  }
  return NULL;
}

static struct block_desc *dr_add_block(u32 addr, int size,
  u32 addr_lit, int size_lit, u16 crc, int is_slave, int *blk_id)
{
  struct block_entry *be;
  struct block_desc *bd;
  int tcache_id;
  int *bcount;

  // do a lookup to get tcache_id and override check
  be = dr_get_entry(addr, is_slave, &tcache_id);
  if (be != NULL)
    dbg(1, "block override for %08x", addr);

  bcount = &block_counts[tcache_id];
  if (*bcount == block_limit[tcache_id]) {
    dbg(1, "bd overflow for tcache %d", tcache_id);
    return NULL;
  }

  bd = &block_tables[tcache_id][*bcount];
  bd->addr = addr;
  bd->size = size;
  bd->addr_lit = addr_lit;
  bd->size_lit = size_lit;
  bd->tcache_ptr = tcache_ptr;
  bd->crc = crc;
  bd->active = 1;

  bd->entry_count = 1;
  bd->entryp[0].pc = addr;
  bd->entryp[0].tcache_ptr = tcache_ptr;
  bd->entryp[0].links = bd->entryp[0].o_links = NULL;
#if (DRC_DEBUG & 2)
  bd->entryp[0].block = bd;
  bd->refcount = 0;
#endif
  add_to_hashlist(&bd->entryp[0], tcache_id);

  *blk_id = *bcount;
  (*bcount)++;
  if (*bcount >= block_max_counts[tcache_id])
    *bcount = 0;

  return bd;
}

static void REGPARM(3) *dr_lookup_block(u32 pc, int is_slave, int *tcache_id)
{
  struct block_entry *be = NULL;
  void *block = NULL;

  be = dr_get_entry(pc, is_slave, tcache_id);
  if (be != NULL)
    block = be->tcache_ptr;

#if (DRC_DEBUG & 2)
  if (be != NULL)
    be->block->refcount++;
#endif
  return block;
}

static void *dr_failure(void)
{
  lprintf("recompilation failed\n");
  exit(1);
}

#if LINK_BRANCHES
static void dr_block_link(struct block_entry *be, struct block_link *bl, int emit_jump)
{
  dbg(2, "- %slink from %p to pc %08x entry %p", emit_jump ? "":"early ",
    bl->jump, bl->target_pc, be->tcache_ptr);

  if (emit_jump)
    emith_jump_patch(bl->jump, be->tcache_ptr);
    // could sync arm caches here, but that's unnecessary

  // move bl to block_entry
  bl->target = be;
  bl->prev = NULL;
  if (be->links)
    be->links->prev = bl;
  bl->next = be->links;
  be->links = bl;
}

static void dr_block_unlink(struct block_link *bl, int emit_jump)
{
  dbg(2,"- unlink from %p to pc %08x", bl->jump, bl->target_pc);

  if (bl->target) {
    if (emit_jump) {
      emith_jump_patch(bl->jump, sh2_drc_dispatcher);
      // update cpu caches since the previous jump target doesn't exist anymore
      host_instructions_updated(bl->jump, bl->jump+4);
    }

    if (bl->prev)
      bl->prev->next = bl->next;
    else
      bl->target->links = bl->next;
    if (bl->next)
      bl->next->prev = bl->prev;
    bl->target = NULL;
  }
}
#endif

static void *dr_prepare_ext_branch(struct block_entry *owner, u32 pc, int is_slave, int tcache_id)
{
#if LINK_BRANCHES
  struct block_link *bl = block_link_pool[tcache_id];
  int cnt = block_link_pool_counts[tcache_id];
  struct block_entry *be = NULL;
  int target_tcache_id;

  // get the target block entry
  be = dr_get_entry(pc, is_slave, &target_tcache_id);
  if (target_tcache_id && target_tcache_id != tcache_id)
    return sh2_drc_dispatcher;

  // get a block link
  if (blink_free[tcache_id] != NULL) {
    bl = blink_free[tcache_id];
    blink_free[tcache_id] = bl->next;
  } else if (cnt >= block_link_pool_max_counts[tcache_id]) {
    dbg(1, "bl overflow for tcache %d", tcache_id);
    return sh2_drc_dispatcher;
  } else {
    bl += cnt;
    block_link_pool_counts[tcache_id] = cnt+1;
  }

  // prepare link and add to ougoing list of owner
  bl->tcache_id = tcache_id;
  bl->target_pc = pc;
  bl->jump = tcache_ptr;
  bl->o_next = owner->o_links;
  owner->o_links = bl;

  if (be != NULL) {
    dr_block_link(be, bl, 0); // jump not yet emitted by translate()
    return be->tcache_ptr;
  }
  else {
    add_to_hashlist_unresolved(bl, tcache_id);
    return sh2_drc_dispatcher;
  }
#else
  return sh2_drc_dispatcher;
#endif
}

static void dr_link_blocks(struct block_entry *be, int tcache_id)
{
#if LINK_BRANCHES
  u32 tcmask = hash_table_sizes[tcache_id] - 1;
  u32 pc = be->pc;
  struct block_link **head = &HASH_FUNC(unresolved_links[tcache_id], pc, tcmask);
  struct block_link *bl = *head, *next;

  while (bl != NULL) {
    next = bl->next;
    if (bl->target_pc == pc && (!bl->tcache_id || bl->tcache_id == tcache_id)) {
      rm_from_hashlist_unresolved(bl, bl->tcache_id);
      dr_block_link(be, bl, 1);
    }
    bl = next;
  }
#endif
}

static void dr_link_outgoing(struct block_entry *be, int tcache_id, int is_slave)
{
#if LINK_BRANCHES
  struct block_link *bl;
  int target_tcache_id;

  for (bl = be->o_links; bl; bl = bl->o_next) {
    if (bl->target == NULL) {
      be = dr_get_entry(bl->target_pc, is_slave, &target_tcache_id);
      if (be != NULL && (!target_tcache_id || target_tcache_id == tcache_id)) {
        // remove bl from unresolved_links (must've been since target was NULL)
        rm_from_hashlist_unresolved(bl, bl->tcache_id);
        dr_block_link(be, bl, 1);
      }
    }
  }
#endif
}

#define ADD_TO_ARRAY(array, count, item, failcode) { \
  if (count >= ARRAY_SIZE(array)) { \
    dbg(1, "warning: " #array " overflow"); \
    failcode; \
  } else \
    array[count++] = item; \
}

static int find_in_array(u32 *array, size_t size, u32 what)
{
  size_t i;
  for (i = 0; i < size; i++)
    if (what == array[i])
      return i;

  return -1;
}

// ---------------------------------------------------------------

// NB rcache allocation dependencies:
// - get_reg_arg/get_tmp_arg first (might evict other regs just allocated)
// - get_reg(..., NULL) before get_reg(..., &hr) if it might get the same reg
// - get_reg(..., RC_GR_READ/RMW, ...) before WRITE (might evict needed reg)

// register cache / constant propagation stuff
typedef enum {
  RC_GR_READ,
  RC_GR_WRITE,
  RC_GR_RMW,
} rc_gr_mode;

typedef struct {
  u32 gregs;
  u32 val;
} gconst_t;

gconst_t gconsts[ARRAY_SIZE(guest_regs)];

static int rcache_get_reg_(sh2_reg_e r, rc_gr_mode mode, int do_locking, int *hr);
static void rcache_add_vreg_alias(int x, sh2_reg_e r);
static void rcache_remove_vreg_alias(int x, sh2_reg_e r);

#define RCACHE_DUMP(msg) { \
  cache_reg_t *cp; \
  guest_reg_t *gp; \
  int i; \
  printf("cache dump %s:\n",msg); \
  printf("cache_regs:\n"); \
  for (i = 0; i < ARRAY_SIZE(cache_regs); i++) { \
    cp = &cache_regs[i]; \
    if (cp->type != HR_FREE || cp->gregs) \
      printf("%d: hr=%d t=%d f=%x m=%x\n", i, cp->hreg, cp->type, cp->flags, cp->gregs); \
  } \
  printf("guest_regs:\n"); \
  for (i = 0; i < ARRAY_SIZE(guest_regs); i++) { \
    gp = &guest_regs[i]; \
    if (gp->vreg != -1 || gp->sreg >= 0) \
      printf("%d: v=%d f=%x s=%d\n", i, gp->vreg, gp->flags, gp->sreg); \
  } \
}

// binary search approach, since we don't have CLZ on ARM920T
#define FOR_ALL_BITS_SET_DO(mask, bit, code) { \
  u32 __mask = mask; \
  for (bit = 31; bit >= 0 && mask; bit--, __mask <<= 1) { \
    if (!(__mask & (0xffff << 16))) \
      bit -= 16, __mask <<= 16; \
    if (!(__mask & (0xff << 24))) \
      bit -= 8, __mask <<= 8; \
    if (!(__mask & (0xf << 28))) \
      bit -= 4, __mask <<= 4; \
    if (!(__mask & (0x3 << 30))) \
      bit -= 2, __mask <<= 2; \
    if (!(__mask & (0x1 << 31))) \
      bit -= 1, __mask <<= 1; \
    if (__mask & (0x1 << 31)) { \
      code; \
    } \
  } \
}

#if PROPAGATE_CONSTANTS
static inline int gconst_alloc(sh2_reg_e r)
{
  int i, n = -1;

  for (i = 0; i < ARRAY_SIZE(gconsts); i++) {
    if (gconsts[i].gregs & (1 << r))
      gconsts[i].gregs &= ~(1 << r);
    if (gconsts[i].gregs == 0 && n < 0)
      n = i;
  }
  if (n >= 0)
    gconsts[n].gregs = (1 << r);
  else
    exit(1); // cannot happen - more constants than guest regs?
  return n;
}

static void gconst_set(sh2_reg_e r, u32 val)
{
  int i = gconst_alloc(r);

  guest_regs[r].flags |= GRF_CONST;
  guest_regs[r].cnst = i;
  gconsts[i].val = val;
}

static void gconst_new(sh2_reg_e r, u32 val)
{
  gconst_set(r, val);
  guest_regs[r].flags |= GRF_CDIRTY;

  // throw away old r that we might have cached
  if (guest_regs[r].vreg >= 0)
    rcache_remove_vreg_alias(guest_regs[r].vreg, r);
}

static void gconst_copy(sh2_reg_e rd, sh2_reg_e rs)
{
  if (guest_regs[rd].flags & GRF_CONST) {
    guest_regs[rd].flags &= ~(GRF_CONST|GRF_CDIRTY);
    gconsts[guest_regs[rd].cnst].gregs &= ~(1 << rd);
  }
  if (guest_regs[rs].flags & GRF_CONST) {
    guest_regs[rd].flags |= GRF_CONST;
    guest_regs[rd].cnst = guest_regs[rs].cnst;
    gconsts[guest_regs[rd].cnst].gregs |= (1 << rd);
  }
}
#endif

static int gconst_get(sh2_reg_e r, u32 *val)
{
  if (guest_regs[r].flags & GRF_CONST) {
    *val = gconsts[guest_regs[r].cnst].val;
    return 1;
  }
  return 0;
}

static int gconst_check(sh2_reg_e r)
{
  if (guest_regs[r].flags & (GRF_CONST|GRF_CDIRTY))
    return 1;
  return 0;
}

// update hr if dirty, else do nothing
static int gconst_try_read(int vreg, sh2_reg_e r)
{
  int i, x;
  if (guest_regs[r].flags & GRF_CDIRTY) {
    x = guest_regs[r].cnst;
    emith_move_r_imm(cache_regs[vreg].hreg, gconsts[x].val);
    FOR_ALL_BITS_SET_DO(gconsts[x].gregs, i,
      {
        if (guest_regs[i].vreg >= 0 && i != r)
          rcache_remove_vreg_alias(guest_regs[i].vreg, i);
        rcache_add_vreg_alias(vreg, i);
        guest_regs[i].flags &= ~GRF_CDIRTY;
        guest_regs[i].flags |= GRF_DIRTY;
      });
    return 1;
  }
  return 0;
}

static u32 gconst_dirty_mask(void)
{
  u32 mask = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++)
    if (guest_regs[i].flags & GRF_CDIRTY)
      mask |= (1 << i);
  return mask;
}

static void gconst_kill(sh2_reg_e r)
{
  if (guest_regs[r].flags &= ~(GRF_CONST|GRF_CDIRTY))
    gconsts[guest_regs[r].cnst].gregs &= ~(1 << r);
  guest_regs[r].flags &= ~(GRF_CONST|GRF_CDIRTY);
}

static void gconst_clean(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++)
    if (guest_regs[i].flags & GRF_CDIRTY) {
      // using RC_GR_READ here: it will call gconst_try_read,
      // cache the reg and mark it dirty.
      rcache_get_reg_(i, RC_GR_READ, 0, NULL);
    }
}

static void gconst_invalidate(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++) {
    if (guest_regs[i].flags & (GRF_CONST|GRF_CDIRTY))
      gconsts[guest_regs[i].cnst].gregs &= ~(1 << i);
    guest_regs[i].flags &= ~(GRF_CONST|GRF_CDIRTY);
  }
}

static u16 rcache_counter;
static u32 rcache_static;
static u32 rcache_locked;
static u32 rcache_hint_soon;
static u32 rcache_hint_late;
static u32 rcache_hint_write;
static u32 rcache_hint_clean;
#define rcache_hint (rcache_hint_soon|rcache_hint_late)

static void rcache_unmap_vreg(int x)
{
  int i;

  FOR_ALL_BITS_SET_DO(cache_regs[x].gregs, i,
      guest_regs[i].vreg = -1);
  if (cache_regs[x].type != HR_STATIC)
    cache_regs[x].type = HR_FREE;
  cache_regs[x].gregs = 0;
  cache_regs[x].flags &= (HRF_REG|HRF_TEMP);
}

static void rcache_clean_vreg(int x)
{
  int r;

  if (cache_regs[x].flags & HRF_DIRTY) { // writeback
    cache_regs[x].flags &= ~HRF_DIRTY;
    FOR_ALL_BITS_SET_DO(cache_regs[x].gregs, r,
        if (guest_regs[r].flags & GRF_DIRTY) {
          if (guest_regs[r].flags & GRF_STATIC) {
            if (guest_regs[r].vreg != guest_regs[r].sreg) {
              if (!(cache_regs[guest_regs[r].sreg].flags & HRF_LOCKED)) {
                // statically mapped reg not in its sreg. move back to sreg
                rcache_clean_vreg(guest_regs[r].sreg);
                rcache_unmap_vreg(guest_regs[r].sreg);
                emith_move_r_r(cache_regs[guest_regs[r].sreg].hreg, cache_regs[guest_regs[r].vreg].hreg);
                rcache_remove_vreg_alias(x, r);
                rcache_add_vreg_alias(guest_regs[r].sreg, r);
                cache_regs[guest_regs[r].sreg].flags |= HRF_DIRTY;
              } else {
                // must evict since sreg is locked
                emith_ctx_write(cache_regs[x].hreg, r * 4);
                guest_regs[r].flags &= ~GRF_DIRTY;
                guest_regs[r].vreg = -1;
              }
            }
          } else if (~rcache_hint_write & (1 << r)) {
            emith_ctx_write(cache_regs[x].hreg, r * 4);
            guest_regs[r].flags &= ~GRF_DIRTY;
          }
        })
  }
}

static void rcache_add_vreg_alias(int x, sh2_reg_e r)
{
  cache_regs[x].gregs |= (1 << r);
  guest_regs[r].vreg = x;
}

static void rcache_remove_vreg_alias(int x, sh2_reg_e r)
{
  cache_regs[x].gregs &= ~(1 << r);
  if (!cache_regs[x].gregs) {
    // no reg mapped -> free vreg
    if (cache_regs[x].type != HR_STATIC)
      cache_regs[x].type = HR_FREE;
    cache_regs[x].flags &= (HRF_REG|HRF_TEMP);
  }
  guest_regs[r].vreg = -1;
}

static void rcache_evict_vreg(int x)
{
  rcache_clean_vreg(x);
  rcache_unmap_vreg(x);
}

static void rcache_evict_vreg_aliases(int x, sh2_reg_e r)
{
  cache_regs[x].gregs &= ~(1 << r);
  rcache_evict_vreg(x);
  cache_regs[x].gregs = (1 << r);
  if (cache_regs[x].type != HR_STATIC)
    cache_regs[x].type = HR_CACHED;
  if (guest_regs[r].flags & GRF_DIRTY)
    cache_regs[x].flags |= HRF_DIRTY;
}

static cache_reg_t *rcache_evict(void)
{
  // evict reg with oldest stamp (only for HRF_REG, no temps)
  int i, i_prio, oldest = -1, prio = 0;
  u16 min_stamp = (u16)-1;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++) {
    // consider only unlocked REG
    if (!(cache_regs[i].flags & HRF_REG) || (cache_regs[i].flags & HRF_LOCKED))
      continue;
    if (cache_regs[i].type == HR_FREE || (cache_regs[i].type == HR_TEMP)) {
      oldest = i;
      break;
    }
    if (cache_regs[i].type == HR_CACHED) {
      if (rcache_locked & cache_regs[i].gregs)
        // REGs needed for the current insn
        i_prio = 1;
      else if (rcache_hint_soon & cache_regs[i].gregs)
        // REGs needed in some future insn
        i_prio = 2;
      else if (rcache_hint_late & cache_regs[i].gregs)
        // REGs needed in some future insn
        i_prio = 3;
      else if ((rcache_hint_write & cache_regs[i].gregs) != cache_regs[i].gregs)
        // REGs not needed soon
        i_prio = 4;
      else
        // REGs soon overwritten anyway
        i_prio = 5;

      if (prio < i_prio || (prio == i_prio && cache_regs[i].stamp < min_stamp)) {
        min_stamp = cache_regs[i].stamp;
        oldest = i;
        prio = i_prio;
      }
    }
  }

  if (oldest == -1) {
    printf("no registers to evict, aborting\n");
    exit(1);
  }

  if (cache_regs[oldest].type == HR_CACHED)
    rcache_evict_vreg(oldest);
  cache_regs[oldest].type = HR_FREE;
  cache_regs[oldest].flags &= (HRF_TEMP|HRF_REG);
  cache_regs[oldest].gregs = 0;

  return &cache_regs[oldest];
}

#if REMAP_REGISTER
// maps a host register to a REG
static int rcache_map_reg(sh2_reg_e r, int hr, int mode)
{
  int i;

  gconst_kill(r);

  // lookup the TEMP hr maps to
  i = reg_map_host[hr];
  if (i < 0) {
    // must not happen
    printf("invalid host register %d\n", hr);
    exit(1);
  }

  // deal with statically mapped regs
  if (mode == RC_GR_RMW && (guest_regs[r].flags & GRF_STATIC)) {
    if (guest_regs[r].vreg == guest_regs[r].sreg) { 
      // STATIC in its sreg with no aliases, and some processing pending
      if (cache_regs[guest_regs[r].vreg].gregs == 1 << r)
        return cache_regs[guest_regs[r].vreg].hreg;
    } else if (!cache_regs[guest_regs[r].sreg].gregs)
      // STATIC not in its sreg, with sreg available -> move it
      i = guest_regs[r].sreg;
  }

  // remove old mappings of r and i if one exists
  if (guest_regs[r].vreg >= 0)
    rcache_remove_vreg_alias(guest_regs[r].vreg, r);
  if (cache_regs[i].type == HR_CACHED)
    rcache_unmap_vreg(i);
  // set new mappping
  if (cache_regs[i].type != HR_STATIC)
    cache_regs[i].type = HR_CACHED;
  cache_regs[i].gregs = 1 << r;
  cache_regs[i].flags &= (HRF_TEMP|HRF_REG);
  cache_regs[i].stamp = ++rcache_counter;
  cache_regs[i].flags |= HRF_DIRTY|HRF_LOCKED;
  guest_regs[r].flags |= GRF_DIRTY;
  guest_regs[r].vreg = i;
  return cache_regs[i].hreg;
}

// remap vreg from a TEMP to a REG if it is hinted (upcoming TEMP invalidation)
static void rcache_remap_vreg(int r)
{
  int i, j, free = -1, cached = -1, hinted = -1;
  u16 min_stamp_cached = (u16)-1, min_stamp_hinted = -1;

  // r must be a vreg
  if (cache_regs[r].type != HR_CACHED)
    return;
  // if r is already a REG or isn't used, clean here to avoid data loss on inval
  if ((cache_regs[r].flags & HRF_REG) || !(rcache_hint & cache_regs[r].gregs)) {
    rcache_clean_vreg(r);
    return;
  }

  // find REG, either free or unused temp or oldest cached
  for (i = 0; i < ARRAY_SIZE(cache_regs) && free < 0; i++) {
    if ((cache_regs[i].flags & HRF_TEMP) || (cache_regs[i].flags & HRF_LOCKED))
      continue;
    if (cache_regs[i].type == HR_FREE || cache_regs[i].type == HR_TEMP)
      free = i;
    if (cache_regs[i].type == HR_CACHED && !(rcache_hint & cache_regs[i].gregs)) {
      if (cache_regs[i].stamp < min_stamp_cached) {
        min_stamp_cached = cache_regs[i].stamp;
        cached = i;
      }
    }
    if (cache_regs[i].type == HR_CACHED && !(rcache_hint_soon & cache_regs[i].gregs)
                                      && (rcache_hint_soon & cache_regs[r].gregs))
      if (cache_regs[i].stamp < min_stamp_hinted) {
        min_stamp_hinted = cache_regs[i].stamp;
        hinted = i;
      }
  }

  if (free >= 0) {
    i = free;
  } else if (cached >= 0 && cached != r) {
    i = cached;
    rcache_evict_vreg(i);
  } else if (hinted >= 0 && hinted != r) {
    i = hinted;
    rcache_evict_vreg(i);
  } else {
    rcache_clean_vreg(r);
    return;
  }

  // set new mapping and remove old one
  cache_regs[i].type = HR_CACHED;
  cache_regs[i].gregs = cache_regs[r].gregs;
  cache_regs[i].flags &= (HRF_TEMP|HRF_REG);
  cache_regs[i].flags |= cache_regs[r].flags & ~(HRF_TEMP|HRF_REG);
  cache_regs[i].stamp = cache_regs[r].stamp;
  emith_move_r_r(cache_regs[i].hreg, cache_regs[r].hreg);
  for (j = 0; j < ARRAY_SIZE(guest_regs); j++)
    if (guest_regs[j].vreg == r)
      guest_regs[j].vreg = i;
  cache_regs[r].type = HR_FREE;
  cache_regs[r].flags &= (HRF_TEMP|HRF_REG);
  cache_regs[r].gregs = 0;
}
#endif

// note: must not be called when doing conditional code
static int rcache_get_reg_(sh2_reg_e r, rc_gr_mode mode, int do_locking, int *hr)
{
  cache_reg_t *tr = NULL;
  int i, h, split = -1;

  rcache_counter++;

  // maybe already cached?
  // if so, prefer against gconst (they must be in sync)
  i = guest_regs[r].vreg;
  if ((guest_regs[r].flags & GRF_STATIC) && i != guest_regs[r].sreg &&
      !(cache_regs[guest_regs[r].sreg].flags & HRF_LOCKED) &&
      (i < 0 || mode != RC_GR_READ) &&
      !((rcache_hint_soon|rcache_locked) & cache_regs[guest_regs[r].sreg].gregs)) {
    // good opportunity to relocate a remapped STATIC
    h = guest_regs[r].sreg;
    rcache_evict_vreg(h);
    tr = &cache_regs[h];
    tr->gregs = 1 << r;
    if (i >= 0) {
      if (mode != RC_GR_WRITE) {
        if (hr)
          *hr = cache_regs[i].hreg;
        else
          emith_move_r_r(cache_regs[h].hreg, cache_regs[i].hreg);
        hr = NULL;
      }
      rcache_remove_vreg_alias(guest_regs[r].vreg, r);
    } else if (mode != RC_GR_WRITE) {
      if (gconst_try_read(h, r)) {
        tr->flags |= HRF_DIRTY;
        guest_regs[r].flags |= GRF_DIRTY;
      } else
        emith_ctx_read(tr->hreg, r * 4);
    }
    guest_regs[r].vreg = guest_regs[r].sreg;
    goto end;
  } else if (i >= 0) {
    if (mode == RC_GR_READ || !(cache_regs[i].gregs & ~(1 << r))) {
      // either only reading, or no multiple mapping
      tr = &cache_regs[i];
      goto end;
    }
    // split if aliases needed rsn, or already locked, or r is STATIC in sreg
    if (((rcache_hint|rcache_locked) & cache_regs[i].gregs & ~(1 << r)) ||
        (cache_regs[i].flags & HRF_LOCKED) ||
        (cache_regs[i].type == HR_STATIC && !(guest_regs[r].flags & GRF_STATIC))) {
      // need to split up. take reg out here to avoid unnecessary writebacks
      rcache_remove_vreg_alias(i, r);
      split = i;
    } else {
      // aliases not needed anytime soon, remove them
      // XXX split aliases away if writing and static and not locked and hinted?
      rcache_evict_vreg_aliases(i, r);
      tr = &cache_regs[i];
      goto end;
    }
  }

  // get a free reg, but use temps only if r is not needed soon
  for (i = ARRAY_SIZE(cache_regs) - 1; i >= 0; i--) {
    if ((cache_regs[i].type == HR_FREE ||
        (cache_regs[i].type == HR_TEMP && !(cache_regs[i].flags & HRF_LOCKED))) &&
        (!(rcache_hint & (1 << r)) || (cache_regs[i].flags & HRF_REG))) {
      tr = &cache_regs[i];
      break;
    }
  }

  if (!tr)
    tr = rcache_evict();

  tr->type = HR_CACHED;
  tr->gregs = 1 << r;
  guest_regs[r].vreg = tr - cache_regs;

  if (mode != RC_GR_WRITE) {
    if (gconst_try_read(guest_regs[r].vreg, r)) {
      tr->flags |= HRF_DIRTY;
      guest_regs[r].flags |= GRF_DIRTY;
    } else if (split >= 0) {
      if (hr) {
        cache_regs[split].flags |= HRF_LOCKED;
        *hr = cache_regs[split].hreg;
        hr = NULL;
      } else if (tr->hreg != cache_regs[split].hreg)
        emith_move_r_r(tr->hreg, cache_regs[split].hreg);
    } else
      emith_ctx_read(tr->hreg, r * 4);
  }

end:
  if (hr)
    *hr = tr->hreg;
  if (do_locking)
    tr->flags |= HRF_LOCKED;
  tr->stamp = rcache_counter;
  if (mode != RC_GR_READ) {
    tr->flags |= HRF_DIRTY;
    guest_regs[r].flags |= GRF_DIRTY;
    gconst_kill(r);
  }

  return tr->hreg;
}

static int rcache_get_reg(sh2_reg_e r, rc_gr_mode mode, int *hr)
{
  return rcache_get_reg_(r, mode, 1, hr);
}

static int rcache_get_tmp(void)
{
  cache_reg_t *tr = NULL;
  int i;

  // use any free reg, but prefer TEMP regs
  for (i = 0; i < ARRAY_SIZE(cache_regs); i++) {
    if (cache_regs[i].type == HR_FREE ||
        (cache_regs[i].type == HR_TEMP && !(cache_regs[i].flags & HRF_LOCKED))) {
      tr = &cache_regs[i];
      break;
    }
  }

  if (!tr)
    tr = rcache_evict();

  tr->type = HR_TEMP;
  tr->flags |= HRF_LOCKED;
  return tr->hreg;
}

static int rcache_get_hr_id(int hr)
{
  int i;

  i = reg_map_host[hr];
  if (i < 0) // can't happen
    exit(1);

#if REMAP_REGISTER
  if (cache_regs[i].type == HR_CACHED)
    rcache_remap_vreg(i);
#endif
  if (cache_regs[i].type == HR_CACHED)
    rcache_evict_vreg(i);
  else if (cache_regs[i].type == HR_TEMP && (cache_regs[i].flags & HRF_LOCKED)) {
    printf("host reg %d already used, aborting\n", hr);
    exit(1);
  }

  return i;
}

static int rcache_get_arg_id(int arg)
{
  int hr = 0;

  host_arg2reg(hr, arg);
  return rcache_get_hr_id(hr);
}

// get a reg to be used as function arg
static int rcache_get_tmp_arg(int arg)
{
  int id = rcache_get_arg_id(arg);
  cache_regs[id].type = HR_TEMP;
  cache_regs[id].flags |= HRF_LOCKED;

  return cache_regs[id].hreg;
}

// ... as return value after a call
static int rcache_get_tmp_ret(void)
{
  int id = rcache_get_hr_id(RET_REG);
  cache_regs[id].type = HR_TEMP;
  cache_regs[id].flags |= HRF_LOCKED;

  return cache_regs[id].hreg;
}

// same but caches a reg if access is readonly (announced by hr being NULL)
static int rcache_get_reg_arg(int arg, sh2_reg_e r, int *hr)
{
  int i, srcr, dstr, dstid;
  int dirty = 0, src_dirty = 0, is_const = 0, is_cached = 0;
  u32 val;
  host_arg2reg(dstr, arg);

  i = guest_regs[r].vreg;
  if (i >= 0 && cache_regs[i].type == HR_CACHED && cache_regs[i].hreg == dstr)
    // r is already in arg
    dstid = i;
  else
    dstid = rcache_get_arg_id(arg);
  dstr = cache_regs[dstid].hreg;

  if (rcache_hint & (1 << r)) {
    // r is needed later on anyway
    srcr = rcache_get_reg_(r, RC_GR_READ, 0, NULL);
    is_cached = (cache_regs[reg_map_host[srcr]].type == HR_CACHED);
  } else if (!(rcache_hint_clean & (1 << r)) &&
             (guest_regs[r].flags & GRF_CDIRTY) && gconst_get(r, &val)) {
    // r has an uncomitted const - load into arg, but keep constant uncomitted
    srcr = dstr;
    is_const = 1;
  } else if ((i = guest_regs[r].vreg) >= 0) {
    // maybe already cached?
    srcr = cache_regs[i].hreg;
    is_cached = (cache_regs[reg_map_host[srcr]].type == HR_CACHED);
  } else {
    // must read either const or from ctx
    srcr = dstr;
    if (rcache_static & (1 << r))
      srcr = rcache_get_reg_(r, RC_GR_READ, 0, NULL);
    else if (gconst_try_read(dstid, r))
      dirty = 1;
    else
      emith_ctx_read(srcr, r * 4);
  }

  if (is_cached) {
    i = reg_map_host[srcr];
    if (srcr == dstr) { // evict aliases here since it is reallocated below
      if (guest_regs[r].flags & GRF_STATIC) // move STATIC back to its sreg
        rcache_clean_vreg(guest_regs[r].vreg);
#if REMAP_REGISTER
      rcache_remap_vreg(i);
#endif
      if (cache_regs[i].type == HR_CACHED)
        rcache_evict_vreg(i);
    }
    else if (hr != NULL)     // must lock srcr if not copied here
      cache_regs[i].flags |= HRF_LOCKED;
    if (guest_regs[r].flags & GRF_DIRTY)
      src_dirty = 1;
  }

  cache_regs[dstid].type = HR_TEMP;
  if (is_const) {
    // uncomitted constant
    emith_move_r_imm(srcr, val);
  } else if (dstr != srcr) {
    // arg is a copy of cached r
    if (hr == NULL)
      emith_move_r_r(dstr, srcr);
  } else if (hr != NULL) {
    // caller will modify arg, so it will soon be out of sync with r
    if (dirty || src_dirty) {
      if (~rcache_hint_write & (1 << r)) {
        emith_ctx_write(dstr, r * 4); // must clean since arg will be modified
        guest_regs[r].flags &= ~GRF_DIRTY;
      }
    }
  } else {
    // keep arg as vreg for r
    cache_regs[dstid].type = HR_CACHED;
    if (guest_regs[r].vreg < 0) {
      cache_regs[dstid].gregs = 1 << r;
      guest_regs[r].vreg = dstid;
    }
    if (dirty || src_dirty) { // mark as modifed for cleaning later on
      cache_regs[dstid].flags |= HRF_DIRTY;
      guest_regs[r].flags |= GRF_DIRTY;
    }
  }

  if (hr)
    *hr = srcr;

  cache_regs[dstid].stamp = ++rcache_counter;
  cache_regs[dstid].flags |= HRF_LOCKED;
  return dstr;
}

static void rcache_free_tmp(int hr)
{
  int i = reg_map_host[hr];
  if (i < 0 || cache_regs[i].type != HR_TEMP) {
    printf("rcache_free_tmp fail: #%i hr %d, type %d\n", i, hr, cache_regs[i].type);
    return;
  }

  cache_regs[i].type = HR_FREE;
  cache_regs[i].flags &= (HRF_REG|HRF_TEMP);
}

// saves temporary result either in REG or in drctmp
static int rcache_save_tmp(int hr)
{
  int i, free = -1, cached = -1;
  u16 min_stamp = (u16)-1;

  // find REG, either free or unlocked temp or oldest non-hinted cached
  for (i = 0; i < ARRAY_SIZE(cache_regs) && free < 0; i++) {
    if ((cache_regs[i].flags & HRF_TEMP) || (cache_regs[i].flags & HRF_LOCKED))
      continue;
    if (cache_regs[i].type == HR_FREE || cache_regs[i].type == HR_TEMP)
      free = i;
    if (cache_regs[i].type == HR_CACHED &&
          !((rcache_hint | rcache_locked) & cache_regs[i].gregs)) {
      if (cache_regs[i].stamp < min_stamp) {
        min_stamp = cache_regs[i].stamp;
        cached = i;
      }
    }
  }

  if (free >= 0)
    i = free;
  else if (cached >= 0) {
    i = cached;
    rcache_evict_vreg(i);
  } else {
    // if none is available, store in drctmp
    emith_ctx_write(hr, offsetof(SH2, drc_tmp));
    rcache_free_tmp(hr);
    return -1;
  }

  cache_regs[i].type = HR_CACHED;
  cache_regs[i].gregs = 0; // not storing any guest register
  cache_regs[i].flags &= (HRF_TEMP|HRF_REG);
  cache_regs[i].flags |= HRF_LOCKED;
  cache_regs[i].stamp = ++rcache_counter;
  emith_move_r_r(cache_regs[i].hreg, hr);
  rcache_free_tmp(hr);
  return i;
}

static int rcache_restore_tmp(int r)
{
  int hr;

  // find REG with tmp store: cached but with no gregs
  if (r >= 0) {
    if (cache_regs[r].type != HR_CACHED || cache_regs[r].gregs) {
      printf("invalid tmp storage %d\n", r);
      exit(1);
    }
    // found, transform to a TEMP
    cache_regs[r].type = HR_TEMP;
    cache_regs[r].flags |= HRF_LOCKED;
    return cache_regs[r].hreg;
  }
 
  // if not available, create a TEMP store and fetch from drctmp
  hr = rcache_get_tmp();
  emith_ctx_read(hr, offsetof(SH2, drc_tmp));

  return hr;
}

static void rcache_unlock(int hr)
{
  if (hr >= 0) {
    cache_regs[hr].flags &= ~HRF_LOCKED;
    rcache_locked &= ~cache_regs[hr].gregs;
  }
}

static void rcache_unlock_all(void)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
    cache_regs[i].flags &= ~HRF_LOCKED;
}

static inline void rcache_set_locked(u32 mask)
{
  rcache_locked = mask & ~rcache_static;
}

static inline void rcache_set_hint_soon(u32 mask)
{
  rcache_hint_soon = mask & ~rcache_static;
}

static inline void rcache_set_hint_late(u32 mask)
{
  rcache_hint_late = mask & ~rcache_static;
}

static inline void rcache_set_hint_write(u32 mask)
{
  rcache_hint_write = mask & ~rcache_static;
}

static inline int rcache_is_hinted(sh2_reg_e r)
{
  // consider static REGs as always hinted, since they are always there
  return ((rcache_hint | rcache_static) & (1 << r));
}

static inline int rcache_is_cached(sh2_reg_e r)
{
  // consider static REGs as always hinted, since they are always there
  return (guest_regs[r].vreg >= 0);
}

static inline u32 rcache_used_hreg_mask(void)
{
  u32 mask = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
    if (cache_regs[i].type != HR_FREE)
      mask |= 1 << cache_regs[i].hreg;

  return mask & ~rcache_static;
}

static inline u32 rcache_dirty_mask(void)
{
  u32 mask = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++)
    if (guest_regs[i].flags & GRF_DIRTY)
      mask |= 1 << i;
  mask |= gconst_dirty_mask();

  return mask;
}

static inline u32 rcache_reg_mask(void)
{
  u32 mask = 0;
  int i;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
    if (cache_regs[i].type == HR_CACHED)
      mask |= cache_regs[i].gregs;

  return mask;
}

static void rcache_clean_tmp(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
    if (cache_regs[i].type == HR_CACHED && (cache_regs[i].flags & HRF_TEMP))
#if REMAP_REGISTER
      rcache_remap_vreg(i);
#else
      rcache_clean_vreg(i);
#endif
}

static void rcache_clean_mask(u32 mask)
{
  int i;

  if (!(mask &= ~rcache_static))
    return;
  rcache_hint_clean |= mask;

  // clean only vregs where all aliases are covered by the mask
  for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
    if (cache_regs[i].type == HR_CACHED &&
        (cache_regs[i].gregs & mask) && !(cache_regs[i].gregs & ~mask))
      rcache_clean_vreg(i);
}

static void rcache_clean(void)
{
  int i;
  gconst_clean();

  for (i = ARRAY_SIZE(cache_regs)-1; i >= 0; i--)
    if (cache_regs[i].type == HR_CACHED || cache_regs[i].type == HR_STATIC)
      rcache_clean_vreg(i);
}

static void rcache_invalidate_tmp(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++) {
    if (cache_regs[i].flags & HRF_TEMP) {
      if (cache_regs[i].type == HR_CACHED)
        rcache_unmap_vreg(i);
      cache_regs[i].type = HR_FREE;
      cache_regs[i].flags &= (HRF_TEMP|HRF_REG);
      cache_regs[i].gregs = 0;
    }
  }
}

static void rcache_invalidate(void)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(cache_regs); i++) {
    cache_regs[i].flags &= (HRF_TEMP|HRF_REG);
    if (cache_regs[i].type != HR_STATIC)
      cache_regs[i].type = HR_FREE;
    cache_regs[i].gregs = 0;
  }

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++) {
    guest_regs[i].flags &= GRF_STATIC;
    if (!(guest_regs[i].flags & GRF_STATIC))
      guest_regs[i].vreg = -1;
    else {
      if (guest_regs[i].vreg < 0)
        emith_ctx_read(cache_regs[guest_regs[i].sreg].hreg, i*4);
      else if (guest_regs[i].vreg != guest_regs[i].sreg)
        emith_move_r_r(cache_regs[guest_regs[i].sreg].hreg,
                        cache_regs[guest_regs[i].vreg].hreg);
      cache_regs[guest_regs[i].sreg].gregs = 1 << i;
      guest_regs[i].vreg = guest_regs[i].sreg;
    }
  }

  rcache_counter = 0;
  rcache_hint_soon = rcache_hint_late = rcache_hint_write = rcache_hint_clean = 0;

  gconst_invalidate();
}

static void rcache_flush(void)
{
  rcache_unlock_all();
  rcache_clean();
  rcache_invalidate();
}

static void rcache_init(void)
{
  static int once = 1;
  int i;

  // init is executed on every rom load, but this must only be executed once...
  if (once) {
    memset(reg_map_host, -1, sizeof(reg_map_host));
    for (i = 0; i < ARRAY_SIZE(cache_regs); i++)
      reg_map_host[cache_regs[i].hreg] = i;

    for (i = 0; i < ARRAY_SIZE(guest_regs); i++)
      if (guest_regs[i].flags & GRF_STATIC) {
        rcache_static |= (1 << i);
        guest_regs[i].sreg = reg_map_host[guest_regs[i].sreg];
        cache_regs[guest_regs[i].sreg].type = HR_STATIC;
      } else
        guest_regs[i].sreg = -1;
    once = 0;
  }

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++)
    if (guest_regs[i].flags & GRF_STATIC) {
      guest_regs[i].vreg = guest_regs[i].sreg;
      cache_regs[guest_regs[i].sreg].gregs = (1 << i);
    }

  rcache_invalidate();
}

// ---------------------------------------------------------------

// NB may return either REG or TEMP
static int emit_get_rbase_and_offs(SH2 *sh2, sh2_reg_e r, int rmod, u32 *offs)
{
  uptr omask = 0xff; // offset mask, XXX: ARM oriented..
  u32 mask = 0;
  u32 a;
  int poffs;
  int hr, hr2;
  uptr la;

  // is r constant and points to a memory region?
  if (! gconst_get(r, &a))
    return -1;
  poffs = dr_ctx_get_mem_ptr(sh2, a, &mask);
  if (poffs == -1)
    return -1;

  if (mask < 0x1000) {
    // data array or BIOS, can't safely access directly since translated code
    // may run on both SH2s
    hr = rcache_get_tmp();
    emith_ctx_read_ptr(hr, poffs);
    a += *offs;
    if (a & mask & ~omask)
      emith_add_r_r_ptr_imm(hr, hr, a & mask & ~omask);
    *offs = a & omask;
    return hr;
  }

  la = (uptr)*(void **)((char *)sh2 + poffs);
  // accessing ROM or SDRAM, code location doesn't matter. The host address
  // for these should be mmapped to be equal to the SH2 address.
  // if r is in rcache or needed soon anyway, and offs is relative to region
  // use rcached const to avoid loading a literal on ARM
  if ((guest_regs[r].vreg >= 0 || ((guest_regs[r].flags & GRF_CDIRTY) &&
      ((rcache_hint_soon|rcache_hint_clean) & (1 << r)))) && !(*offs & ~mask)) {
    u32 odd = a & 1; // need to fix odd address for correct byte addressing
    la -= (s32)((a & ~mask) - *offs - odd); // diff between reg and memory
    // if reg is modified later on, allocate it RMW to remove aliases here
    // else the aliases vreg stays locked and a vreg shortage may occur.
    hr = hr2 = rcache_get_reg(r, rmod ? RC_GR_RMW : RC_GR_READ, NULL);
    if ((la & ~omask) - odd) {
      hr = rcache_get_tmp();
      emith_add_r_r_ptr_imm(hr, hr2, (la & ~omask) - odd);
    }
    *offs = (la & omask);
  } else {
    // known fixed host address
    la += (a + *offs) & mask;
    hr = rcache_get_tmp();
    emith_move_r_ptr_imm(hr, la & ~omask);
    *offs = la & omask;
  }
  return hr;
}

// read const data from const ROM address
static int emit_get_rom_data(SH2 *sh2, sh2_reg_e r, u32 offs, int size, u32 *val)
{
  u32 a, mask;

  *val = 0;
  if (gconst_get(r, &a)) {
    a += offs;
    // check if rom is memory mapped (not bank switched), and address is in rom
    if (dr_is_rom(a) && p32x_sh2_get_mem_ptr(a, &mask, sh2)) {
      switch (size & MF_SIZEMASK) {
      case 0:   *val = (s8)p32x_sh2_read8(a, sh2s);   break;  // 8
      case 1:   *val = (s16)p32x_sh2_read16(a, sh2s); break;  // 16
      case 2:   *val = p32x_sh2_read32(a, sh2s);      break;  // 32
      }
      return 1;
    }
  }
  return 0;
}

static void emit_move_r_imm32(sh2_reg_e dst, u32 imm)
{
#if PROPAGATE_CONSTANTS
  gconst_new(dst, imm);
#else
  int hr = rcache_get_reg(dst, RC_GR_WRITE, NULL);
  emith_move_r_imm(hr, imm);
#endif
}

static void emit_move_r_r(sh2_reg_e dst, sh2_reg_e src)
{
  int hr_d, hr_s;

  if (guest_regs[src].vreg >= 0 || gconst_check(src) || rcache_is_hinted(src)) {
    hr_s = rcache_get_reg(src, RC_GR_READ, NULL);
#if ALIAS_REGISTERS
    // check for aliasing
    int i = guest_regs[src].vreg;
    if (guest_regs[dst].vreg != i) {
      // remove possible old mapping of dst
      if (guest_regs[dst].vreg >= 0)
        rcache_remove_vreg_alias(guest_regs[dst].vreg, dst);
      // make dst an alias of src
      rcache_add_vreg_alias(i, dst);
      cache_regs[i].flags |= HRF_DIRTY;
      guest_regs[dst].flags |= GRF_DIRTY;
      gconst_kill(dst);
#if PROPAGATE_CONSTANTS
      gconst_copy(dst, src);
#endif
      return;
    }
#endif
    hr_d = rcache_get_reg(dst, RC_GR_WRITE, NULL);
    emith_move_r_r(hr_d, hr_s);
#if PROPAGATE_CONSTANTS
    gconst_copy(dst, src);
#endif
  } else {
    hr_d = rcache_get_reg(dst, RC_GR_WRITE, NULL);
    emith_ctx_read(hr_d, src * 4);
  }
}

static void emit_sync_t_to_sr(void)
{
  // avoid reloading SR from context if there's nothing to do
  if (emith_get_t_cond() >= 0) {
    int sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
    emith_sync_t(sr);
  }
}

// rd = @(arg0)
static int emit_memhandler_read(int size)
{
  emit_sync_t_to_sr();
  rcache_clean_tmp();
#ifndef DRC_SR_REG
  // must writeback cycles for poll detection stuff
  if (guest_regs[SHR_SR].vreg != -1)
    rcache_evict_vreg(guest_regs[SHR_SR].vreg);
#endif

  if (size & MF_POLLING)
    switch (size & MF_SIZEMASK) {
    case 0:   emith_call(sh2_drc_read8_poll);   break; // 8
    case 1:   emith_call(sh2_drc_read16_poll);  break; // 16
    case 2:   emith_call(sh2_drc_read32_poll);  break; // 32
    }
  else
    switch (size & MF_SIZEMASK) {
    case 0:   emith_call(sh2_drc_read8);        break; // 8
    case 1:   emith_call(sh2_drc_read16);       break; // 16
    case 2:   emith_call(sh2_drc_read32);       break; // 32
    }

  rcache_invalidate_tmp();
  return rcache_get_tmp_ret();
}

// @(arg0) = arg1
static void emit_memhandler_write(int size)
{
  emit_sync_t_to_sr();
  rcache_clean_tmp();
#ifndef DRC_SR_REG
  if (guest_regs[SHR_SR].vreg != -1)
    rcache_evict_vreg(guest_regs[SHR_SR].vreg);
#endif

  switch (size & MF_SIZEMASK) {
  case 0:   emith_call(sh2_drc_write8);     break;  // 8
  case 1:   emith_call(sh2_drc_write16);    break;  // 16
  case 2:   emith_call(sh2_drc_write32);    break;  // 32
  }

  rcache_invalidate_tmp();
}

// rd = @(Rs,#offs); rd < 0 -> return a temp
static int emit_memhandler_read_rr(SH2 *sh2, sh2_reg_e rd, sh2_reg_e rs, u32 offs, int size)
{
  int hr, hr2;
  u32 val;

#if PROPAGATE_CONSTANTS
  if (emit_get_rom_data(sh2, rs, offs, size, &val)) {
    if (rd == SHR_TMP) {
      hr2 = rcache_get_tmp();
      emith_move_r_imm(hr2, val);
    } else {
      emit_move_r_imm32(rd, val);
      hr2 = rcache_get_reg(rd, RC_GR_RMW, NULL);
    }
    if ((size & MF_POSTINCR) && gconst_get(rs, &val))
      gconst_new(rs, val + (1 << (size & MF_SIZEMASK)));
    return hr2;
  }

  hr = emit_get_rbase_and_offs(sh2, rs, size & MF_POSTINCR, &offs);
  if (hr != -1) {
    if (rd == SHR_TMP)
      hr2 = rcache_get_tmp();
    else
      hr2 = rcache_get_reg(rd, RC_GR_WRITE, NULL);
    switch (size & MF_SIZEMASK) {
    case 0: emith_read8s_r_r_offs(hr2, hr, offs ^ 1);  break; // 8
    case 1: emith_read16s_r_r_offs(hr2, hr, offs);     break; // 16
    case 2: emith_read_r_r_offs(hr2, hr, offs); emith_ror(hr2, hr2, 16); break;
    }
    if (cache_regs[reg_map_host[hr]].type == HR_TEMP) // may also return REG
      rcache_free_tmp(hr);
    if (size & MF_POSTINCR) {
      int isgc = gconst_get(rs, &val);
      if (!isgc || guest_regs[rs].vreg >= 0) {
        // already loaded
        hr = rcache_get_reg(rs, RC_GR_RMW, NULL);
        emith_add_r_r_imm(hr, hr, 1 << (size & MF_SIZEMASK));
        if (isgc)
          gconst_set(rs, val + (1 << (size & MF_SIZEMASK)));
      } else
        gconst_new(rs, val + (1 << (size & MF_SIZEMASK)));
    }
    return hr2;
  }
#endif

  if (gconst_get(rs, &val) && guest_regs[rs].vreg < 0 && !(rcache_hint_soon & (1 << rs))) {
    hr = rcache_get_tmp_arg(0);
    emith_move_r_imm(hr, val + offs);
    if (size & MF_POSTINCR)
      gconst_new(rs, val + (1 << (size & MF_SIZEMASK)));
  } else if (size & MF_POSTINCR) {
    hr = rcache_get_tmp_arg(0);
    hr2 = rcache_get_reg(rs, RC_GR_RMW, NULL);
    emith_add_r_r_imm(hr, hr2, offs);
    emith_add_r_imm(hr2, 1 << (size & MF_SIZEMASK));
  } else {
    hr = rcache_get_reg_arg(0, rs, &hr2);
    if (offs || hr != hr2)
      emith_add_r_r_imm(hr, hr2, offs);
  }
  hr = emit_memhandler_read(size);

  size &= MF_SIZEMASK;
  if (rd == SHR_TMP)
    hr2 = hr;
  else
#if REMAP_REGISTER
    hr2 = rcache_map_reg(rd, hr, RC_GR_WRITE);
#else
    hr2 = rcache_get_reg(rd, RC_GR_WRITE, NULL);
#endif

  if (hr != hr2) {
    emith_move_r_r(hr2, hr);
    rcache_free_tmp(hr);
  }
  return hr2;
}

// @(Rs,#offs) = rd; rd < 0 -> write arg1
static void emit_memhandler_write_rr(SH2 *sh2, sh2_reg_e rd, sh2_reg_e rs, u32 offs, int size)
{
  int hr, hr2;
  u32 val;

  if (rd == SHR_TMP) {
    host_arg2reg(hr2, 1);
  } else if ((size & MF_PREDECR) && rd == rs) { // must avoid caching rd in arg1
    hr2 = rcache_get_reg_arg(1, rd, &hr);
    if (hr != hr2) emith_move_r_r(hr2, hr);
  } else
    hr2 = rcache_get_reg_arg(1, rd, NULL);

  if (gconst_get(rs, &val) && guest_regs[rs].vreg < 0 && !(rcache_hint_soon & (1 << rs))) {
    if (size & MF_PREDECR) {
      val -= 1 << (size & MF_SIZEMASK);
      gconst_new(rs, val);
    }
    hr = rcache_get_tmp_arg(0);
    emith_move_r_imm(hr, val + offs);
  } else if (offs || (size & MF_PREDECR)) {
    if (size & MF_PREDECR) {
      hr = rcache_get_reg(rs, RC_GR_RMW, &hr2);
      emith_sub_r_r_imm(hr, hr2, 1 << (size & MF_SIZEMASK));
    }
    hr = rcache_get_reg_arg(0, rs, &hr2);
    if (offs || hr != hr2)
      emith_add_r_r_imm(hr, hr2, offs);
  } else
    rcache_get_reg_arg(0, rs, NULL);

  emit_memhandler_write(size);
}

// rd = @(Rx,Ry); rd < 0 -> return a temp
static int emit_indirect_indexed_read(SH2 *sh2, sh2_reg_e rd, sh2_reg_e rx, sh2_reg_e ry, int size)
{
  int hr, hr2;
  int tx, ty;
#if PROPAGATE_CONSTANTS
  u32 offs;

  if (gconst_get(ry, &offs))
    return emit_memhandler_read_rr(sh2, rd, rx, offs, size);
  if (gconst_get(rx, &offs))
    return emit_memhandler_read_rr(sh2, rd, ry, offs, size);
#endif
  hr = rcache_get_reg_arg(0, rx, &tx);
  ty = rcache_get_reg(ry, RC_GR_READ, NULL);
  emith_add_r_r_r(hr, tx, ty);
  hr = emit_memhandler_read(size);

  size &= MF_SIZEMASK;
  if (rd == SHR_TMP)
    hr2 = hr;
  else
#if REMAP_REGISTER
    hr2 = rcache_map_reg(rd, hr, RC_GR_WRITE);
#else
    hr2 = rcache_get_reg(rd, RC_GR_WRITE, NULL);
#endif

  if (hr != hr2) {
    emith_move_r_r(hr2, hr);
    rcache_free_tmp(hr);
  }
  return hr2;
}

// @(Rx,Ry) = rd; rd < 0 -> write arg1
static void emit_indirect_indexed_write(SH2 *sh2, sh2_reg_e rd, sh2_reg_e rx, sh2_reg_e ry, int size)
{
  int hr, tx, ty;
#if PROPAGATE_CONSTANTS
  u32 offs;

  if (gconst_get(ry, &offs))
    return emit_memhandler_write_rr(sh2, rd, rx, offs, size);
  if (gconst_get(rx, &offs))
    return emit_memhandler_write_rr(sh2, rd, ry, offs, size);
#endif
  if (rd != SHR_TMP)
    rcache_get_reg_arg(1, rd, NULL);
  hr = rcache_get_reg_arg(0, rx, &tx);
  ty = rcache_get_reg(ry, RC_GR_READ, NULL);
  emith_add_r_r_r(hr, tx, ty);
  emit_memhandler_write(size);
}

// @Rn+,@Rm+
static void emit_indirect_read_double(SH2 *sh2, int *rnr, int *rmr, sh2_reg_e rn, sh2_reg_e rm, int size)
{
  int tmp;

  // unlock rn, rm here to avoid REG shortage in MAC operation
  tmp = emit_memhandler_read_rr(sh2, SHR_TMP, rn, 0, size | MF_POSTINCR);
  rcache_unlock(guest_regs[rn].vreg);
  tmp = rcache_save_tmp(tmp);
  *rmr = emit_memhandler_read_rr(sh2, SHR_TMP, rm, 0, size | MF_POSTINCR);
  rcache_unlock(guest_regs[rm].vreg);
  *rnr = rcache_restore_tmp(tmp);
}
 
static void emit_do_static_regs(int is_write, int tmpr)
{
  int i, r, count;

  for (i = 0; i < ARRAY_SIZE(guest_regs); i++) {
    if (guest_regs[i].flags & GRF_STATIC)
      r = cache_regs[guest_regs[i].vreg].hreg;
    else
      continue;

    for (count = 1; i < ARRAY_SIZE(guest_regs) - 1; i++, r++) {
      if ((guest_regs[i + 1].flags & GRF_STATIC) &&
          cache_regs[guest_regs[i + 1].vreg].hreg == r + 1)
        count++;
      else
        break;
    }

    if (count > 1) {
      // i, r point to last item
      if (is_write)
        emith_ctx_write_multiple(r - count + 1, (i - count + 1) * 4, count, tmpr);
      else
        emith_ctx_read_multiple(r - count + 1, (i - count + 1) * 4, count, tmpr);
    } else {
      if (is_write)
        emith_ctx_write(r, i * 4);
      else
        emith_ctx_read(r, i * 4);
    }
  }
}

/* just after lookup function, jump to address returned */
static void emit_block_entry(void)
{
  emith_tst_r_r_ptr(RET_REG, RET_REG);
  EMITH_SJMP_START(DCOND_EQ);
  emith_jump_reg_c(DCOND_NE, RET_REG);
  EMITH_SJMP_END(DCOND_EQ);
}

#define DELAY_SAVE_T(sr) { \
  emith_bic_r_imm(sr, T_save); \
  emith_tst_r_imm(sr, T);      \
  EMITH_SJMP_START(DCOND_EQ);  \
  emith_or_r_imm_c(DCOND_NE, sr, T_save); \
  EMITH_SJMP_END(DCOND_EQ);    \
}

#define FLUSH_CYCLES(sr) \
  if (cycles > 0) { \
    emith_sub_r_imm(sr, cycles << 12); \
    cycles = 0; \
  }

static void *dr_get_pc_base(u32 pc, SH2 *sh2);

static void REGPARM(2) *sh2_translate(SH2 *sh2, int tcache_id)
{
  u32 branch_target_pc[MAX_LOCAL_BRANCHES];
  void *branch_target_ptr[MAX_LOCAL_BRANCHES];
  int branch_target_count = 0;
  void *branch_patch_ptr[MAX_LOCAL_BRANCHES];
  u32 branch_patch_pc[MAX_LOCAL_BRANCHES];
  int branch_patch_count = 0;
  u8 op_flags[BLOCK_INSN_LIMIT];
  struct drcf {
    int delay_reg:8;
    u32 loop_type:8;
    u32 polling:8;
    u32 test_irq:1;
    u32 pending_branch_direct:1;
    u32 pending_branch_indirect:1;
  } drcf = { 0, };

  // PC of current, first, last SH2 insn
  u32 pc, base_pc, end_pc;
  u32 base_literals, end_literals;
  void *block_entry_ptr;
  struct block_desc *block;
  struct block_entry *entry;
  u16 *dr_pc_base;
  struct op_data *opd;
  int blkid_main = 0;
  int skip_op = 0;
  int tmp, tmp2;
  int cycles;
  int i, v;
  u32 u, m1, m2;
  int op;
  u16 crc;

  base_pc = sh2->pc;

  // get base/validate PC
  dr_pc_base = dr_get_pc_base(base_pc, sh2);
  if (dr_pc_base == (void *)-1) {
    printf("invalid PC, aborting: %08x\n", base_pc);
    // FIXME: be less destructive
    exit(1);
  }

  // initial passes to disassemble and analyze the block
  crc = scan_block(base_pc, sh2->is_slave, op_flags, &end_pc, &base_literals, &end_literals);
  end_literals = dr_check_nolit(base_literals, end_literals, tcache_id);
  if (base_literals == end_literals) // map empty lit section to end of code
    base_literals = end_literals = end_pc;

  // if there is already a translated but inactive block, reuse it
  block = dr_find_inactive_block(tcache_id, crc, base_pc, end_pc - base_pc,
    base_literals, end_literals - base_literals);

  if (block) {
    // connect branches
    dbg(2, "== %csh2 reuse block %08x-%08x,%08x-%08x -> %p", sh2->is_slave ? 's' : 'm',
      base_pc, end_pc, base_literals, end_literals, block->entryp->tcache_ptr);
    for (i = 0; i < block->entry_count; i++) {
      entry = &block->entryp[i];
      add_to_hashlist(entry, tcache_id);
#if LINK_BRANCHES
      // incoming branches
      dr_link_blocks(entry, tcache_id);
      if (!tcache_id)
        dr_link_blocks(entry, sh2->is_slave?2:1);
      // outgoing branches
      dr_link_outgoing(entry, tcache_id, sh2->is_slave);
#endif
    }
    // mark memory for overwrite detection
    dr_mark_memory(1, block, tcache_id, 0);
    block->active = 1;
    return block->entryp[0].tcache_ptr;
  }

  // collect branch_targets that don't land on delay slots
  m1 = m2 = v = op = 0;
  for (pc = base_pc, i = 0; pc < end_pc; i++, pc += 2) {
    if (op_flags[i] & OF_DELAY_OP)
      op_flags[i] &= ~OF_BTARGET;
    if (op_flags[i] & OF_BTARGET)
      ADD_TO_ARRAY(branch_target_pc, branch_target_count, pc, );
    if (ops[i].op == OP_LDC && (ops[i].dest & BITMASK1(SHR_SR)) && pc+2 < end_pc)
      op_flags[i+1] |= OF_BTARGET; // RTE entrypoint in case of SR(IMASK) change
#if LOOP_DETECTION
    // loop types detected:
    // 1. target: ... BRA target -> idle loop
    // 2. target: ... delay insn ... BF target -> delay loop
    // 3. target: ... poll  insn ... BF/BT target -> poll loop
    // 4. target: ... poll  insn ... BF/BT exit ... BRA target, exit: -> poll
    // conditions:
    // a. no further branch targets between target and back jump.
    // b. no unconditional branch insn inside the loop.
    // c. exactly one poll or delay insn is allowed inside a delay/poll loop
    // (scan_block marks loops only if they meet conditions a through c)
    // d. idle loops do not modify anything but PC,SR and contain no branches
    // e. delay/poll loops do not modify anything but the concerned reg,PC,SR
    // f. loading constants into registers inside the loop is allowed
    // g. a delay/poll loop must have a conditional branch somewhere
    // h. an idle loop must not have a conditional branch
    if (op_flags[i] & OF_BTARGET) {
      // possible loop entry point
      drcf.loop_type = op_flags[i] & OF_LOOP;
      drcf.pending_branch_direct = drcf.pending_branch_indirect = 0;
      op = OF_IDLE_LOOP; // loop type
      v = i;
      m1 = m2 = 0;
    }
    if (drcf.loop_type) {
      // detect loop type, and store poll/delay register
      if (op_flags[i] & OF_POLL_INSN) {
        op = OF_POLL_LOOP;
        m1 |= ops[i].dest;   // loop poll/delay regs
      } else if (op_flags[i] & OF_DELAY_INSN) {
        op = OF_DELAY_LOOP;
        m1 |= ops[i].dest;
      } else if (ops[i].op != OP_LOAD_POOL && ops[i].op != OP_LOAD_CONST
              && (ops[i].op != OP_MOVE || op != OF_POLL_LOOP)) {
        // not (MOV @(PC) or MOV # or (MOV reg and poll)),   condition f
        m2 |= ops[i].dest;   // regs modified by other insns
      }
      // branch detector
      if (OP_ISBRAIMM(ops[i].op) && ops[i].imm == base_pc + 2*v)
        drcf.pending_branch_direct = 1;         // backward branch detected
      if (OP_ISBRACND(ops[i].op))
        drcf.pending_branch_indirect = 1;       // conditions g,h - cond.branch
      // poll/idle loops terminate with their backwards branch to the loop start
      if (drcf.pending_branch_direct && !(op_flags[i+1] & OF_DELAY_OP)) {
        m2 &= ~(m1 | BITMASK2(SHR_PC, SHR_SR)); // conditions d,e + g,h
        if (m2 || ((op == OF_IDLE_LOOP) == (drcf.pending_branch_indirect)))
          op = 0;                               // conditions not met
        op_flags[v] = (op_flags[v] & ~OF_LOOP) | op; // set loop type
        drcf.loop_type = 0;
      }
    }
#endif
  }

  if (branch_target_count > 0) {
    memset(branch_target_ptr, 0, sizeof(branch_target_ptr[0]) * branch_target_count);
  }

  tcache_ptr = dr_prepare_cache(tcache_id, (end_pc - base_pc) / 2);
#if (DRC_DEBUG & 4)
  tcache_dsm_ptrs[tcache_id] = tcache_ptr;
#endif

  block = dr_add_block(base_pc, end_pc - base_pc, base_literals,
    end_literals - base_literals, crc, sh2->is_slave, &blkid_main);
  if (block == NULL)
    return NULL;

  block_entry_ptr = tcache_ptr;
  dbg(2, "== %csh2 block #%d,%d %08x-%08x,%08x-%08x -> %p", sh2->is_slave ? 's' : 'm',
    tcache_id, blkid_main, base_pc, end_pc, base_literals, end_literals, block_entry_ptr);


  // clear stale state after compile errors
  rcache_invalidate();
  emith_invalidate_t();
  drcf = (struct drcf) { 0 };

  // -------------------------------------------------
  // 3rd pass: actual compilation
  pc = base_pc;
  cycles = 0;
  for (i = 0; pc < end_pc; i++)
  {
    u32 delay_dep_fw = 0, delay_dep_bk = 0;
    int tmp3, tmp4;
    int sr;

    opd = &ops[i];
    op = FETCH_OP(pc);

#if (DRC_DEBUG & 2)
    insns_compiled++;
#endif
#if (DRC_DEBUG & 4)
    DasmSH2(sh2dasm_buff, pc, op);
    if (op_flags[i] & OF_BTARGET) {
      if ((op_flags[i] & OF_LOOP) == OF_DELAY_LOOP)     tmp3 = '+';
      else if ((op_flags[i] & OF_LOOP) == OF_POLL_LOOP) tmp3 = '=';
      else if ((op_flags[i] & OF_LOOP) == OF_IDLE_LOOP) tmp3 = '~';
      else                                              tmp3 = '*';
    } else if (drcf.loop_type)                          tmp3 = '.';
    else                                                tmp3 = ' ';
    printf("%c%08x %04x %s\n", tmp3, pc, op, sh2dasm_buff);
#endif

    if (op_flags[i] & OF_BTARGET)
    {
      if (pc != base_pc)
      {
        sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        FLUSH_CYCLES(sr);
        emith_sync_t(sr);
        rcache_flush();
        emith_flush();

        // make block entry
        v = block->entry_count;
        entry = &block->entryp[v];
        if (v < ARRAY_SIZE(block->entryp))
        {
          entry = &block->entryp[v];
          entry->pc = pc;
          entry->tcache_ptr = tcache_ptr;
          entry->links = entry->o_links = NULL;
#if (DRC_DEBUG & 2)
          entry->block = block;
#endif
          add_to_hashlist(entry, tcache_id);
          block->entry_count++;

          dbg(2, "-- %csh2 block #%d,%d entry %08x -> %p",
            sh2->is_slave ? 's' : 'm', tcache_id, blkid_main,
            pc, tcache_ptr);
        }
        else {
          dbg(1, "too many entryp for block #%d,%d pc=%08x",
            tcache_id, blkid_main, pc);
          break;
        }
      } else {
        entry = block->entryp;
      }

      // since we made a block entry, link any other blocks that jump to it
      dr_link_blocks(entry, tcache_id);
      if (!tcache_id) // can safely link from cpu-local to global memory
        dr_link_blocks(entry, sh2->is_slave?2:1);

      v = find_in_array(branch_target_pc, branch_target_count, pc);
      if (v >= 0)
        branch_target_ptr[v] = tcache_ptr;
#if LOOP_DETECTION
      drcf.loop_type = op_flags[i] & OF_LOOP;
      drcf.delay_reg = -1;
      drcf.polling = (drcf.loop_type == OF_POLL_LOOP ? MF_POLLING : 0);
#endif

#if DRC_DEBUG
      // must update PC
      emit_move_r_imm32(SHR_PC, pc);
#endif
      rcache_clean();

#if (DRC_DEBUG & 0x10)
      rcache_get_reg_arg(0, SHR_PC, NULL);
      tmp = emit_memhandler_read(1);
      tmp2 = rcache_get_tmp();
      tmp3 = rcache_get_tmp();
      emith_move_r_imm(tmp2, (s16)FETCH_OP(pc));
      emith_move_r_imm(tmp3, 0);
      emith_cmp_r_r(tmp, tmp2);
      EMITH_SJMP_START(DCOND_EQ);
      emith_read_r_r_offs_c(DCOND_NE, tmp3, tmp3, 0); // crash
      EMITH_SJMP_END(DCOND_EQ);
      rcache_free_tmp(tmp);
      rcache_free_tmp(tmp2);
      rcache_free_tmp(tmp3);
#endif

      // check cycles
      tmp = rcache_get_tmp_arg(0);
      sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
      emith_cmp_r_imm(sr, 0);
      emith_move_r_imm(tmp, pc);
      emith_jump_cond(DCOND_LE, sh2_drc_exit);
      rcache_free_tmp(tmp);

#if (DRC_DEBUG & 32)
      // block hit counter
      tmp  = rcache_get_tmp_arg(0);
      tmp2 = rcache_get_tmp_arg(1);
      emith_move_r_ptr_imm(tmp, (uptr)entry);
      emith_read_r_r_offs(tmp2, tmp, offsetof(struct block_entry, entry_count));
      emith_add_r_imm(tmp2, 1);
      emith_write_r_r_offs(tmp2, tmp, offsetof(struct block_entry, entry_count));
      rcache_free_tmp(tmp);
      rcache_free_tmp(tmp2);
#endif

#if (DRC_DEBUG & (8|256|512|1024))
      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      emith_sync_t(sr);
      rcache_clean();
      tmp = rcache_used_hreg_mask();
      emith_save_caller_regs(tmp);
      emit_do_static_regs(1, 0);
      rcache_get_reg_arg(2, SHR_SR, NULL);
      tmp2 = rcache_get_tmp_arg(0);
      tmp3 = rcache_get_tmp_arg(1);
      emith_move_r_ptr_imm(tmp2, tcache_ptr);
      emith_move_r_r_ptr(tmp3,CONTEXT_REG);
      emith_call(sh2_drc_log_entry);
      emith_restore_caller_regs(tmp);
      rcache_invalidate_tmp();
#endif

      do_host_disasm(tcache_id);
      rcache_unlock_all();
    }

#ifdef DRC_CMP
    if (!(op_flags[i] & OF_DELAY_OP)) {
      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      FLUSH_CYCLES(sr);
      emith_sync_t(sr);
      rcache_clean();

      tmp = rcache_used_hreg_mask();
      emith_save_caller_regs(tmp);
      emit_do_static_regs(1, 0);
      emith_pass_arg_r(0, CONTEXT_REG);
      emith_call(do_sh2_cmp);
      emith_restore_caller_regs(tmp);
    }
#endif

    emith_pool_check();
    pc += 2;

    if (skip_op > 0) {
      skip_op--;
      continue;
    }

    if (op_flags[i] & OF_DELAY_OP)
    {
      // handle delay slot dependencies
      delay_dep_fw = opd->dest & ops[i-1].source;
      delay_dep_bk = opd->source & ops[i-1].dest;
      if (delay_dep_fw & BITMASK1(SHR_T)) {
        sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_sync_t(sr);
        DELAY_SAVE_T(sr);
      }
      if (delay_dep_bk & BITMASK1(SHR_PC)) {
        if (opd->op != OP_LOAD_POOL && opd->op != OP_MOVA) {
          // can only be those 2 really..
          elprintf_sh2(sh2, EL_ANOMALY,
            "drc: illegal slot insn %04x @ %08x?", op, pc - 2);
        }
        // store PC for MOVA/MOV @PC address calculation
        if (opd->imm != 0)
          ; // case OP_BRANCH - addr already resolved in scan_block
        else {
          switch (ops[i-1].op) {
          case OP_BRANCH:
            emit_move_r_imm32(SHR_PC, ops[i-1].imm);
            break;
          case OP_BRANCH_CT:
          case OP_BRANCH_CF:
            sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
            tmp = rcache_get_reg(SHR_PC, RC_GR_WRITE, NULL);
            emith_move_r_imm(tmp, pc);
            tmp2 = emith_tst_t(sr, (ops[i-1].op == OP_BRANCH_CT));
            tmp3 = emith_invert_cond(tmp2);
            EMITH_SJMP_START(tmp3);
            emith_move_r_imm_c(tmp2, tmp, ops[i-1].imm);
            EMITH_SJMP_END(tmp3);
            break;
          case OP_BRANCH_N: // BT/BF known not to be taken
            // XXX could modify opd->imm instead?
            emit_move_r_imm32(SHR_PC, pc);
            break;
          // case OP_BRANCH_R OP_BRANCH_RF - PC already loaded
          }
        }
      }
      //if (delay_dep_fw & ~BITMASK1(SHR_T))
      //  dbg(1, "unhandled delay_dep_fw: %x", delay_dep_fw & ~BITMASK1(SHR_T));
      if (delay_dep_bk & ~BITMASK2(SHR_PC, SHR_PR))
        dbg(1, "unhandled delay_dep_bk: %x", delay_dep_bk);
      rcache_set_hint_soon(0);
      rcache_set_hint_late(0);
      rcache_set_hint_write(0);
    }
    else
    {
      // inform cache about future register usage
      u32 late = 0;             // regs read by future ops
      u32 write = 0;            // regs written to (to detect write before read)
      u32 soon = 0;             // regs read soon
      tmp = (OP_ISBRANCH(opd[0].op) || opd[0].op == OP_RTE || // branching insns
              opd[0].op == OP_TRAPA || opd[0].op == OP_UNDEFINED);
      for (v = 1; v <= 9; v++) {
        // no sense in looking any further than the next rcache flush
        if (pc + 2*v < end_pc && !(op_flags[i+v] & OF_BTARGET) &&
              (!tmp || (op_flags[i+v] & OF_DELAY_OP))) {
          late |= opd[v].source & ~write;
          // ignore source regs after they have been written to
          write |= opd[v].dest;
        } else {
          // upcoming rcache_flush, start writing back unused dirty stuff
          tmp2 = write|opd[0].source|opd[0].dest; // insn may change reg aliases
          rcache_clean_mask(rcache_dirty_mask() & ~tmp2);
          break;
        }
        tmp |= (OP_ISBRANCH(opd[v].op) || opd[v].op == OP_RTE ||
                opd[v].op == OP_TRAPA || opd[v].op == OP_UNDEFINED);
        // regs needed in the next few instructions
        if (v <= 4)
          soon = late;
      }
      rcache_set_hint_soon(late);           // insns 1-3
      rcache_set_hint_late(late & ~soon);   // insns 4-9
      rcache_set_hint_write(write & ~(late|soon) & ~opd[0].source);
                                            // overwritten without being used
    }
    rcache_set_locked(opd[0].source); // try not to evict src regs for this op

    switch (opd->op)
    {
    case OP_BRANCH_N:
      // never taken, just use up cycles
      goto end_op;
    case OP_BRANCH:
    case OP_BRANCH_CT:
    case OP_BRANCH_CF:
      if (opd->dest & BITMASK1(SHR_PR))
        emit_move_r_imm32(SHR_PR, pc + 2);
      drcf.pending_branch_direct = 1;
      goto end_op;

    case OP_BRANCH_R:
      if (opd->dest & BITMASK1(SHR_PR))
        emit_move_r_imm32(SHR_PR, pc + 2);
      emit_move_r_r(SHR_PC, opd->rm);
      drcf.pending_branch_indirect = 1;
      goto end_op;

    case OP_BRANCH_RF:
      tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
      tmp  = rcache_get_reg(SHR_PC, RC_GR_WRITE, NULL);
      emith_move_r_imm(tmp, pc + 2);
      if (opd->dest & BITMASK1(SHR_PR)) {
        tmp3 = rcache_get_reg(SHR_PR, RC_GR_WRITE, NULL);
        emith_move_r_r(tmp3, tmp);
      }
      emith_add_r_r(tmp, tmp2);
      if (gconst_get(GET_Rn(), &u))
        gconst_set(SHR_PC, pc + 2 + u);
      drcf.pending_branch_indirect = 1;
      goto end_op;

    case OP_SLEEP: // SLEEP      0000000000011011
      printf("TODO sleep\n");
      goto end_op;

    case OP_RTE: // RTE        0000000000101011
      emith_invalidate_t();
      // pop PC
      emit_memhandler_read_rr(sh2, SHR_PC, SHR_SP, 0, 2 | MF_POSTINCR);
      // pop SR
      tmp = emit_memhandler_read_rr(sh2, SHR_TMP, SHR_SP, 0, 2 | MF_POSTINCR);
      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      emith_write_sr(sr, tmp);
      rcache_free_tmp(tmp);
      drcf.test_irq = 1;
      drcf.pending_branch_indirect = 1;
      goto end_op;

    case OP_UNDEFINED:
      elprintf_sh2(sh2, EL_ANOMALY, "drc: unhandled op %04x @ %08x", op, pc-2);
      opd->imm = (op_flags[i] & OF_B_IN_DS) ? 6 : 4;
      // fallthrough
    case OP_TRAPA: // TRAPA #imm      11000011iiiiiiii
      // push SR
      tmp  = rcache_get_reg_arg(1, SHR_SR, &tmp2);
      emith_sync_t(tmp2);
      emith_clear_msb(tmp, tmp2, 22);
      emit_memhandler_write_rr(sh2, SHR_TMP, SHR_SP, 0, 2 | MF_PREDECR);
      // push PC
      if (op == OP_TRAPA) {
        tmp = rcache_get_tmp_arg(1);
        emith_move_r_imm(tmp, pc);
      } else if (drcf.pending_branch_indirect) {
        tmp = rcache_get_reg_arg(1, SHR_PC, NULL);
      } else {
        tmp = rcache_get_tmp_arg(1);
        emith_move_r_imm(tmp, pc - 2);
      }
      emith_move_r_imm(tmp, pc);
      emit_memhandler_write_rr(sh2, SHR_TMP, SHR_SP, 0, 2 | MF_PREDECR);
      // obtain new PC
      emit_memhandler_read_rr(sh2, SHR_PC, SHR_VBR, opd->imm * 4, 2);
      // indirect jump -> back to dispatcher
      drcf.pending_branch_indirect = 1;
      goto end_op;

    case OP_LOAD_POOL:
#if PROPAGATE_CONSTANTS
      if ((opd->imm && opd->imm >= base_pc && opd->imm < end_literals) ||
          dr_is_rom(opd->imm))
      {
        if (opd->size == 2)
          u = FETCH32(opd->imm);
        else
          u = (s16)FETCH_OP(opd->imm);
        // tweak for Blackthorne: avoid stack overwriting
        if (GET_Rn() == SHR_SP && u == 0x0603f800) u = 0x0603f880;
        gconst_new(GET_Rn(), u);
      }
      else
#endif
      {
        if (opd->imm != 0) {
          tmp = rcache_get_tmp_arg(0);
          emith_move_r_imm(tmp, opd->imm);
        } else {
          // have to calculate read addr from PC for delay slot
          tmp = rcache_get_reg_arg(0, SHR_PC, &tmp2);
          if (opd->size == 2) {
            emith_add_r_r_imm(tmp, tmp2, 2 + (op & 0xff) * 4);
            emith_bic_r_imm(tmp, 3);
          }
          else
            emith_add_r_r_imm(tmp, tmp2, 2 + (op & 0xff) * 2);
        }
        tmp2 = emit_memhandler_read(opd->size);
#if REMAP_REGISTER
        tmp3 = rcache_map_reg(GET_Rn(), tmp2, RC_GR_WRITE);
#else
        tmp3 = rcache_get_reg(GET_Rn(), RC_GR_WRITE, NULL);
#endif
        if (tmp3 != tmp2) {
          emith_move_r_r(tmp3, tmp2);
          rcache_free_tmp(tmp2);
        }
      }
      goto end_op;

    case OP_MOVA: // MOVA @(disp,PC),R0    11000111dddddddd
      if (opd->imm != 0)
        emit_move_r_imm32(SHR_R0, opd->imm);
      else {
        // have to calculate addr from PC for delay slot
        tmp2 = rcache_get_reg(SHR_PC, RC_GR_READ, NULL);
        tmp = rcache_get_reg(SHR_R0, RC_GR_WRITE, NULL);
        emith_add_r_r_imm(tmp, tmp2, 2 + (op & 0xff) * 4);
        emith_bic_r_imm(tmp, 3);
      }
      goto end_op;
    }

    switch ((op >> 12) & 0x0f)
    {
    /////////////////////////////////////////////
    case 0x00:
      switch (op & 0x0f)
      {
      case 0x02:
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
        if (tmp2 == SHR_SR) {
          sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
          emith_sync_t(sr);
          tmp = rcache_get_reg(GET_Rn(), RC_GR_WRITE, NULL);
          emith_clear_msb(tmp, sr, 22); // reserved bits defined by ISA as 0
        } else
          emit_move_r_r(GET_Rn(), tmp2);
        goto end_op;
      case 0x04: // MOV.B Rm,@(R0,Rn)   0000nnnnmmmm0100
      case 0x05: // MOV.W Rm,@(R0,Rn)   0000nnnnmmmm0101
      case 0x06: // MOV.L Rm,@(R0,Rn)   0000nnnnmmmm0110
        emit_indirect_indexed_write(sh2, GET_Rm(), SHR_R0, GET_Rn(), op & 3);
        goto end_op;
      case 0x07: // MUL.L     Rm,Rn      0000nnnnmmmm0111
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE, NULL);
        emith_mul(tmp3, tmp2, tmp);
        goto end_op;
      case 0x08:
        switch (GET_Fx())
        {
        case 0: // CLRT               0000000000001000
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_set_t(sr, 0);
          break;
        case 1: // SETT               0000000000011000
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_set_t(sr, 1);
          break;
        case 2: // CLRMAC             0000000000101000
          emit_move_r_imm32(SHR_MACL, 0);
          emit_move_r_imm32(SHR_MACH, 0);
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x09:
        switch (GET_Fx())
        {
        case 0: // NOP        0000000000001001
          break;
        case 1: // DIV0U      0000000000011001
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_invalidate_t();
          emith_bic_r_imm(sr, M|Q|T);
          break;
        case 2: // MOVT Rn    0000nnnn00101001
          sr   = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
          emith_sync_t(sr);
          tmp2 = rcache_get_reg(GET_Rn(), RC_GR_WRITE, NULL);
          emith_clear_msb(tmp2, sr, 31);
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x0a:
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
        emit_move_r_r(GET_Rn(), tmp2);
        goto end_op;
      case 0x0c: // MOV.B    @(R0,Rm),Rn      0000nnnnmmmm1100
      case 0x0d: // MOV.W    @(R0,Rm),Rn      0000nnnnmmmm1101
      case 0x0e: // MOV.L    @(R0,Rm),Rn      0000nnnnmmmm1110
        emit_indirect_indexed_read(sh2, GET_Rn(), SHR_R0, GET_Rm(), (op & 3) | drcf.polling);
        goto end_op;
      case 0x0f: // MAC.L   @Rm+,@Rn+  0000nnnnmmmm1111
        emit_indirect_read_double(sh2, &tmp, &tmp2, GET_Rn(), GET_Rm(), 2);
        sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_RMW, NULL);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_RMW, NULL);
        emith_sh2_macl(tmp3, tmp4, tmp, tmp2, sr);
        rcache_free_tmp(tmp2);
        rcache_free_tmp(tmp);
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x01: // MOV.L Rm,@(disp,Rn) 0001nnnnmmmmdddd
      emit_memhandler_write_rr(sh2, GET_Rm(), GET_Rn(), (op & 0x0f) * 4, 2);
      goto end_op;

    case 0x02:
      switch (op & 0x0f)
      {
      case 0x00: // MOV.B Rm,@Rn        0010nnnnmmmm0000
      case 0x01: // MOV.W Rm,@Rn        0010nnnnmmmm0001
      case 0x02: // MOV.L Rm,@Rn        0010nnnnmmmm0010
        emit_memhandler_write_rr(sh2, GET_Rm(), GET_Rn(), 0, op & 3);
        goto end_op;
      case 0x04: // MOV.B Rm,@-Rn       0010nnnnmmmm0100
      case 0x05: // MOV.W Rm,@-Rn       0010nnnnmmmm0101
      case 0x06: // MOV.L Rm,@-Rn       0010nnnnmmmm0110
        emit_memhandler_write_rr(sh2, GET_Rm(), GET_Rn(), 0, (op & 3) | MF_PREDECR);
        goto end_op;
      case 0x07: // DIV0S Rm,Rn         0010nnnnmmmm0111
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        emith_invalidate_t();
        emith_bic_r_imm(sr, M|Q|T);
        emith_tst_r_imm(tmp2, (1<<31));
        EMITH_SJMP_START(DCOND_EQ);
        emith_or_r_imm_c(DCOND_NE, sr, Q);
        EMITH_SJMP_END(DCOND_EQ);
        emith_tst_r_imm(tmp3, (1<<31));
        EMITH_SJMP_START(DCOND_EQ);
        emith_or_r_imm_c(DCOND_NE, sr, M);
        EMITH_SJMP_END(DCOND_EQ);
        emith_teq_r_r(tmp2, tmp3);
        EMITH_SJMP_START(DCOND_PL);
        emith_or_r_imm_c(DCOND_MI, sr, T);
        EMITH_SJMP_END(DCOND_PL);
        goto end_op;
      case 0x08: // TST Rm,Rn           0010nnnnmmmm1000
        sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        emith_clr_t_cond(sr);
        emith_tst_r_r(tmp2, tmp3);
        emith_set_t_cond(sr, DCOND_EQ);
        goto end_op;
      case 0x09: // AND Rm,Rn           0010nnnnmmmm1001
        if (GET_Rm() != GET_Rn()) {
          tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
          emith_and_r_r_r(tmp, tmp3, tmp2);
        }
        goto end_op;
      case 0x0a: // XOR Rm,Rn           0010nnnnmmmm1010
#if PROPAGATE_CONSTANTS
        if (GET_Rn() == GET_Rm()) {
          gconst_new(GET_Rn(), 0);
          goto end_op;
        }
#endif
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        emith_eor_r_r_r(tmp, tmp3, tmp2);
        goto end_op;
      case 0x0b: // OR  Rm,Rn           0010nnnnmmmm1011
        if (GET_Rm() != GET_Rn()) {
          tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
          tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
          emith_or_r_r_r(tmp, tmp3, tmp2);
        }
        goto end_op;
      case 0x0c: // CMP/STR Rm,Rn       0010nnnnmmmm1100
        tmp  = rcache_get_tmp();
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        emith_eor_r_r_r(tmp, tmp2, tmp3);
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_clr_t_cond(sr);
        emith_tst_r_imm(tmp, 0x000000ff);
        EMITH_SJMP_START(DCOND_EQ);
        emith_tst_r_imm_c(DCOND_NE, tmp, 0x0000ff00);
        EMITH_SJMP_START(DCOND_EQ);
        emith_tst_r_imm_c(DCOND_NE, tmp, 0x00ff0000);
        EMITH_SJMP_START(DCOND_EQ);
        emith_tst_r_imm_c(DCOND_NE, tmp, 0xff000000);
        EMITH_SJMP_END(DCOND_EQ);
        EMITH_SJMP_END(DCOND_EQ);
        EMITH_SJMP_END(DCOND_EQ);
        emith_set_t_cond(sr, DCOND_EQ);
        rcache_free_tmp(tmp);
        goto end_op;
      case 0x0d: // XTRCT  Rm,Rn        0010nnnnmmmm1101
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        emith_lsr(tmp, tmp3, 16);
        emith_or_r_r_lsl(tmp, tmp2, 16);
        goto end_op;
      case 0x0e: // MULU.W Rm,Rn        0010nnnnmmmm1110
      case 0x0f: // MULS.W Rm,Rn        0010nnnnmmmm1111
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(SHR_MACL, RC_GR_WRITE, NULL);
        if (op & 1) {
          emith_sext(tmp, tmp2, 16);
        } else
          emith_clear_msb(tmp, tmp2, 16);
        tmp2 = rcache_get_tmp();
        if (op & 1) {
          emith_sext(tmp2, tmp3, 16);
        } else
          emith_clear_msb(tmp2, tmp3, 16);
        emith_mul(tmp, tmp, tmp2);
        rcache_free_tmp(tmp2);
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
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        emith_clr_t_cond(sr);
        emith_cmp_r_r(tmp2, tmp3);
        switch (op & 0x07)
        {
        case 0x00: // CMP/EQ
          emith_set_t_cond(sr, DCOND_EQ);
          break;
        case 0x02: // CMP/HS
          emith_set_t_cond(sr, DCOND_HS);
          break;
        case 0x03: // CMP/GE
          emith_set_t_cond(sr, DCOND_GE);
          break;
        case 0x06: // CMP/HI
          emith_set_t_cond(sr, DCOND_HI);
          break;
        case 0x07: // CMP/GT
          emith_set_t_cond(sr, DCOND_GT);
          break;
        }
        goto end_op;
      case 0x04: // DIV1    Rm,Rn       0011nnnnmmmm0100
        // Q1 = carry(Rn = (Rn << 1) | T)
        // if Q ^ M
        //   Q2 = carry(Rn += Rm)
        // else
        //   Q2 = carry(Rn -= Rm)
        // Q = M ^ Q1 ^ Q2
        // T = (Q == M) = !(Q ^ M) = !(Q1 ^ Q2)
        tmp3 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp);
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_sync_t(sr);
        emith_tpop_carry(sr, 0);
        emith_adcf_r_r_r(tmp2, tmp, tmp);
        emith_tpush_carry(sr, 0);            // keep Q1 in T for now
        tmp4 = rcache_get_tmp();
        emith_and_r_r_imm(tmp4, sr, M);
        emith_eor_r_r_lsr(sr, tmp4, M_SHIFT - Q_SHIFT); // Q ^= M
        rcache_free_tmp(tmp4);
        // add or sub, invert T if carry to get Q1 ^ Q2
        // in: (Q ^ M) passed in Q, Q1 in T
        emith_sh2_div1_step(tmp2, tmp3, sr);
        emith_bic_r_imm(sr, Q);
        emith_tst_r_imm(sr, M);
        EMITH_SJMP_START(DCOND_EQ);
        emith_or_r_imm_c(DCOND_NE, sr, Q);  // Q = M
        EMITH_SJMP_END(DCOND_EQ);
        emith_tst_r_imm(sr, T);
        EMITH_SJMP_START(DCOND_EQ);
        emith_eor_r_imm_c(DCOND_NE, sr, Q); // Q = M ^ Q1 ^ Q2
        EMITH_SJMP_END(DCOND_EQ);
        emith_eor_r_imm(sr, T);             // T = !(Q1 ^ Q2)
        goto end_op;
      case 0x05: // DMULU.L Rm,Rn       0011nnnnmmmm0101
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE, NULL);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_WRITE, NULL);
        emith_mul_u64(tmp3, tmp4, tmp, tmp2);
        goto end_op;
      case 0x08: // SUB     Rm,Rn       0011nnnnmmmm1000
#if PROPAGATE_CONSTANTS
        if (GET_Rn() == GET_Rm()) {
          gconst_new(GET_Rn(), 0);
          goto end_op;
        }
#endif
      case 0x0c: // ADD     Rm,Rn       0011nnnnmmmm1100
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        if (op & 4) {
          emith_add_r_r_r(tmp, tmp3, tmp2);
        } else
          emith_sub_r_r_r(tmp, tmp3, tmp2);
        goto end_op;
      case 0x0a: // SUBC    Rm,Rn       0011nnnnmmmm1010
      case 0x0e: // ADDC    Rm,Rn       0011nnnnmmmm1110
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_sync_t(sr);
        if (op & 4) { // adc
          emith_tpop_carry(sr, 0);
          emith_adcf_r_r_r(tmp, tmp3, tmp2);
          emith_tpush_carry(sr, 0);
        } else {
          emith_tpop_carry(sr, 1);
          emith_sbcf_r_r_r(tmp, tmp3, tmp2);
          emith_tpush_carry(sr, 1);
        }
        goto end_op;
      case 0x0b: // SUBV    Rm,Rn       0011nnnnmmmm1011
      case 0x0f: // ADDV    Rm,Rn       0011nnnnmmmm1111
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_clr_t_cond(sr);
        if (op & 4) {
          emith_addf_r_r_r(tmp, tmp3, tmp2);
        } else
          emith_subf_r_r_r(tmp, tmp3, tmp2);
        emith_set_t_cond(sr, DCOND_VS);
        goto end_op;
      case 0x0d: // DMULS.L Rm,Rn       0011nnnnmmmm1101
        tmp  = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
        tmp2 = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_WRITE, NULL);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_WRITE, NULL);
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
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp2);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_sync_t(sr);
          emith_tpop_carry(sr, 0); // dummy
          emith_lslf(tmp, tmp2, 1);
          emith_tpush_carry(sr, 0);
          goto end_op;
        case 1: // DT Rn      0100nnnn00010000
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
#if LOOP_DETECTION
          if (drcf.loop_type == OF_DELAY_LOOP) {
            if (drcf.delay_reg == -1)
              drcf.delay_reg = GET_Rn();
            else
              drcf.polling = drcf.loop_type = 0;
          }
#endif
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp2);
          emith_clr_t_cond(sr);
          emith_subf_r_r_imm(tmp, tmp2, 1);
          emith_set_t_cond(sr, DCOND_EQ);
          goto end_op;
        }
        goto default_;
      case 0x01:
        switch (GET_Fx())
        {
        case 0: // SHLR Rn    0100nnnn00000001
        case 2: // SHAR Rn    0100nnnn00100001
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp2);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_sync_t(sr);
          emith_tpop_carry(sr, 0); // dummy
          if (op & 0x20) {
            emith_asrf(tmp, tmp2, 1);
          } else
            emith_lsrf(tmp, tmp2, 1);
          emith_tpush_carry(sr, 0);
          goto end_op;
        case 1: // CMP/PZ Rn  0100nnnn00010001
          tmp = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_clr_t_cond(sr);
          emith_cmp_r_imm(tmp, 0);
          emith_set_t_cond(sr, DCOND_GE);
          goto end_op;
        }
        goto default_;
      case 0x02:
      case 0x03:
        switch (op & 0x3f)
        {
        case 0x02: // STS.L    MACH,@-Rn 0100nnnn00000010
          tmp = SHR_MACH;
          break;
        case 0x12: // STS.L    MACL,@-Rn 0100nnnn00010010
          tmp = SHR_MACL;
          break;
        case 0x22: // STS.L    PR,@-Rn   0100nnnn00100010
          tmp = SHR_PR;
          break;
        case 0x03: // STC.L    SR,@-Rn   0100nnnn00000011
          tmp = SHR_SR;
          break;
        case 0x13: // STC.L    GBR,@-Rn  0100nnnn00010011
          tmp = SHR_GBR;
          break;
        case 0x23: // STC.L    VBR,@-Rn  0100nnnn00100011
          tmp = SHR_VBR;
          break;
        default:
          goto default_;
        }
        tmp3 = rcache_get_reg_arg(1, tmp, &tmp4);
        if (tmp == SHR_SR) {
          emith_sync_t(tmp4);
          emith_clear_msb(tmp3, tmp4, 22); // reserved bits defined by ISA as 0
        } else if (tmp3 != tmp4)
          emith_move_r_r(tmp3, tmp4);
        emit_memhandler_write_rr(sh2, SHR_TMP, GET_Rn(), 0, 2 | MF_PREDECR);
        goto end_op;
      case 0x04:
      case 0x05:
        switch (op & 0x3f)
        {
        case 0x04: // ROTL   Rn          0100nnnn00000100
        case 0x05: // ROTR   Rn          0100nnnn00000101
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp2);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_sync_t(sr);
          emith_tpop_carry(sr, 0); // dummy
          if (op & 1) {
            emith_rorf(tmp, tmp2, 1);
          } else
            emith_rolf(tmp, tmp2, 1);
          emith_tpush_carry(sr, 0);
          goto end_op;
        case 0x24: // ROTCL  Rn          0100nnnn00100100
        case 0x25: // ROTCR  Rn          0100nnnn00100101
          tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, NULL);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_sync_t(sr);
          emith_tpop_carry(sr, 0);
          if (op & 1) {
            emith_rorcf(tmp);
          } else
            emith_rolcf(tmp);
          emith_tpush_carry(sr, 0);
          goto end_op;
        case 0x15: // CMP/PL Rn          0100nnnn00010101
          tmp = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_clr_t_cond(sr);
          emith_cmp_r_imm(tmp, 0);
          emith_set_t_cond(sr, DCOND_GT);
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
        if (tmp == SHR_SR) {
          emith_invalidate_t();
          tmp2 = emit_memhandler_read_rr(sh2, SHR_TMP, GET_Rn(), 0, 2 | MF_POSTINCR);
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_write_sr(sr, tmp2);
          rcache_free_tmp(tmp2);
          drcf.test_irq = 1;
        } else
          emit_memhandler_read_rr(sh2, tmp, GET_Rn(), 0, 2 | MF_POSTINCR);
        goto end_op;
      case 0x08:
      case 0x09:
        switch (GET_Fx())
        {
        case 0: // SHLL2 Rn        0100nnnn00001000
                // SHLR2 Rn        0100nnnn00001001
          tmp = 2;
          break;
        case 1: // SHLL8 Rn        0100nnnn00011000
                // SHLR8 Rn        0100nnnn00011001
          tmp = 8;
          break;
        case 2: // SHLL16 Rn       0100nnnn00101000
                // SHLR16 Rn       0100nnnn00101001
          tmp = 16;
          break;
        default:
          goto default_;
        }
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp3);
        if (op & 1) {
          emith_lsr(tmp2, tmp3, tmp);
        } else
          emith_lsl(tmp2, tmp3, tmp);
        goto end_op;
      case 0x0a:
        switch (GET_Fx())
        {
        case 0: // LDS      Rm,MACH   0100mmmm00001010
          tmp2 = SHR_MACH;
          break;
        case 1: // LDS      Rm,MACL   0100mmmm00011010
          tmp2 = SHR_MACL;
          break;
        case 2: // LDS      Rm,PR     0100mmmm00101010
          tmp2 = SHR_PR;
          break;
        default:
          goto default_;
        }
        emit_move_r_r(tmp2, GET_Rn());
        goto end_op;
      case 0x0b:
        switch (GET_Fx())
        {
        case 1: // TAS.B @Rn  0100nnnn00011011
          // XXX: is TAS working on 32X?
          rcache_get_reg_arg(0, GET_Rn(), NULL);
          tmp = emit_memhandler_read(0);
          sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_clr_t_cond(sr);
          emith_cmp_r_imm(tmp, 0);
          emith_set_t_cond(sr, DCOND_EQ);
          emith_or_r_imm(tmp, 0x80);
          tmp2 = rcache_get_tmp_arg(1); // assuming it differs to tmp
          emith_move_r_r(tmp2, tmp);
          rcache_free_tmp(tmp);
          rcache_get_reg_arg(0, GET_Rn(), NULL);
          emit_memhandler_write(0);
          break;
        default:
          goto default_;
        }
        goto end_op;
      case 0x0e:
        switch (GET_Fx())
        {
        case 0: // LDC Rm,SR   0100mmmm00001110
          tmp2 = SHR_SR;
          break;
        case 1: // LDC Rm,GBR  0100mmmm00011110
          tmp2 = SHR_GBR;
          break;
        case 2: // LDC Rm,VBR  0100mmmm00101110
          tmp2 = SHR_VBR;
          break;
        default:
          goto default_;
        }
        if (tmp2 == SHR_SR) {
          emith_invalidate_t();
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          tmp = rcache_get_reg(GET_Rn(), RC_GR_READ, NULL);
          emith_write_sr(sr, tmp);
          drcf.test_irq = 1;
        } else
          emit_move_r_r(tmp2, GET_Rn());
        goto end_op;
      case 0x0f: // MAC.W @Rm+,@Rn+  0100nnnnmmmm1111
        emit_indirect_read_double(sh2, &tmp, &tmp2, GET_Rn(), GET_Rm(), 1);
        sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
        tmp3 = rcache_get_reg(SHR_MACL, RC_GR_RMW, NULL);
        tmp4 = rcache_get_reg(SHR_MACH, RC_GR_RMW, NULL);
        emith_sh2_macw(tmp3, tmp4, tmp, tmp2, sr);
        rcache_free_tmp(tmp2);
        rcache_free_tmp(tmp);
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x05: // MOV.L @(disp,Rm),Rn 0101nnnnmmmmdddd
      emit_memhandler_read_rr(sh2, GET_Rn(), GET_Rm(), (op & 0x0f) * 4, 2 | drcf.polling);
      goto end_op;

    /////////////////////////////////////////////
    case 0x06:
      switch (op & 0x0f)
      {
      case 0x00: // MOV.B @Rm,Rn        0110nnnnmmmm0000
      case 0x01: // MOV.W @Rm,Rn        0110nnnnmmmm0001
      case 0x02: // MOV.L @Rm,Rn        0110nnnnmmmm0010
      case 0x04: // MOV.B @Rm+,Rn       0110nnnnmmmm0100
      case 0x05: // MOV.W @Rm+,Rn       0110nnnnmmmm0101
      case 0x06: // MOV.L @Rm+,Rn       0110nnnnmmmm0110
        tmp = ((op & 7) >= 4 && GET_Rn() != GET_Rm()) ? MF_POSTINCR : drcf.polling;
        emit_memhandler_read_rr(sh2, GET_Rn(), GET_Rm(), 0, (op & 3) | tmp);
        goto end_op;
      case 0x03: // MOV    Rm,Rn        0110nnnnmmmm0011
        emit_move_r_r(GET_Rn(), GET_Rm());
        goto end_op;
      case 0x07 ... 0x0f:
        tmp  = rcache_get_reg(GET_Rm(), RC_GR_READ, NULL);
        tmp2 = rcache_get_reg(GET_Rn(), RC_GR_WRITE, NULL);
        switch (op & 0x0f)
        {
        case 0x07: // NOT    Rm,Rn        0110nnnnmmmm0111
          emith_mvn_r_r(tmp2, tmp);
          break;
        case 0x08: // SWAP.B Rm,Rn        0110nnnnmmmm1000
          tmp3 = tmp2;
          if (tmp == tmp2)
            tmp3 = rcache_get_tmp();
          tmp4 = rcache_get_tmp();
          emith_lsr(tmp3, tmp, 16);
          emith_or_r_r_lsl(tmp3, tmp, 24);
          emith_and_r_r_imm(tmp4, tmp, 0xff00);
          emith_or_r_r_lsl(tmp3, tmp4, 8);
          emith_rol(tmp2, tmp3, 16);
          rcache_free_tmp(tmp4);
          if (tmp == tmp2)
            rcache_free_tmp(tmp3);
          break;
        case 0x09: // SWAP.W Rm,Rn        0110nnnnmmmm1001
          emith_rol(tmp2, tmp, 16);
          break;
        case 0x0a: // NEGC   Rm,Rn        0110nnnnmmmm1010
          sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
          emith_sync_t(sr);
          emith_tpop_carry(sr, 1);
          emith_negcf_r_r(tmp2, tmp);
          emith_tpush_carry(sr, 1);
          break;
        case 0x0b: // NEG    Rm,Rn        0110nnnnmmmm1011
          emith_neg_r_r(tmp2, tmp);
          break;
        case 0x0c: // EXTU.B Rm,Rn        0110nnnnmmmm1100
          emith_clear_msb(tmp2, tmp, 24);
          break;
        case 0x0d: // EXTU.W Rm,Rn        0110nnnnmmmm1101
          emith_clear_msb(tmp2, tmp, 16);
          break;
        case 0x0e: // EXTS.B Rm,Rn        0110nnnnmmmm1110
          emith_sext(tmp2, tmp, 8);
          break;
        case 0x0f: // EXTS.W Rm,Rn        0110nnnnmmmm1111
          emith_sext(tmp2, tmp, 16);
          break;
        }
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x07: // ADD #imm,Rn  0111nnnniiiiiiii
      tmp = rcache_get_reg(GET_Rn(), RC_GR_RMW, &tmp2);
      if (op & 0x80) { // adding negative
        emith_sub_r_r_imm(tmp, tmp2, -op & 0xff);
      } else
        emith_add_r_r_imm(tmp, tmp2, op & 0xff);
      goto end_op;

    /////////////////////////////////////////////
    case 0x08:
      switch (op & 0x0f00)
      {
      case 0x0000: // MOV.B R0,@(disp,Rn)  10000000nnnndddd
      case 0x0100: // MOV.W R0,@(disp,Rn)  10000001nnnndddd
        tmp = (op & 0x100) >> 8;
        emit_memhandler_write_rr(sh2, SHR_R0, GET_Rm(), (op & 0x0f) << tmp, tmp);
        goto end_op;
      case 0x0400: // MOV.B @(disp,Rm),R0  10000100mmmmdddd
      case 0x0500: // MOV.W @(disp,Rm),R0  10000101mmmmdddd
        tmp = (op & 0x100) >> 8;
        emit_memhandler_read_rr(sh2, SHR_R0, GET_Rm(), (op & 0x0f) << tmp, tmp | drcf.polling);
        goto end_op;
      case 0x0800: // CMP/EQ #imm,R0       10001000iiiiiiii
        tmp2 = rcache_get_reg(SHR_R0, RC_GR_READ, NULL);
        sr   = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_clr_t_cond(sr);
        emith_cmp_r_imm(tmp2, (s8)(op & 0xff));
        emith_set_t_cond(sr, DCOND_EQ);
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x0c:
      switch (op & 0x0f00)
      {
      case 0x0000: // MOV.B R0,@(disp,GBR)   11000000dddddddd
      case 0x0100: // MOV.W R0,@(disp,GBR)   11000001dddddddd
      case 0x0200: // MOV.L R0,@(disp,GBR)   11000010dddddddd
        tmp = (op & 0x300) >> 8;
        emit_memhandler_write_rr(sh2, SHR_R0, SHR_GBR, (op & 0xff) << tmp, tmp);
        goto end_op;
      case 0x0400: // MOV.B @(disp,GBR),R0   11000100dddddddd
      case 0x0500: // MOV.W @(disp,GBR),R0   11000101dddddddd
      case 0x0600: // MOV.L @(disp,GBR),R0   11000110dddddddd
        tmp = (op & 0x300) >> 8;
        emit_memhandler_read_rr(sh2, SHR_R0, SHR_GBR, (op & 0xff) << tmp, tmp | drcf.polling);
        goto end_op;
      case 0x0800: // TST #imm,R0           11001000iiiiiiii
        tmp = rcache_get_reg(SHR_R0, RC_GR_READ, NULL);
        sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_clr_t_cond(sr);
        emith_tst_r_imm(tmp, op & 0xff);
        emith_set_t_cond(sr, DCOND_EQ);
        goto end_op;
      case 0x0900: // AND #imm,R0           11001001iiiiiiii
        tmp = rcache_get_reg(SHR_R0, RC_GR_RMW, &tmp2);
        emith_and_r_r_imm(tmp, tmp2, (op & 0xff));
        goto end_op;
      case 0x0a00: // XOR #imm,R0           11001010iiiiiiii
        if (op & 0xff) {
          tmp = rcache_get_reg(SHR_R0, RC_GR_RMW, &tmp2);
          emith_eor_r_r_imm(tmp, tmp2, (op & 0xff));
        }
        goto end_op;
      case 0x0b00: // OR  #imm,R0           11001011iiiiiiii
        if (op & 0xff) {
          tmp = rcache_get_reg(SHR_R0, RC_GR_RMW, &tmp2);
          emith_or_r_r_imm(tmp, tmp2, (op & 0xff));
        }
        goto end_op;
      case 0x0c00: // TST.B #imm,@(R0,GBR)  11001100iiiiiiii
        tmp = emit_indirect_indexed_read(sh2, SHR_TMP, SHR_R0, SHR_GBR, 0 | drcf.polling);
        sr  = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
        emith_clr_t_cond(sr);
        emith_tst_r_imm(tmp, op & 0xff);
        emith_set_t_cond(sr, DCOND_EQ);
        rcache_free_tmp(tmp);
        goto end_op;
      case 0x0d00: // AND.B #imm,@(R0,GBR)  11001101iiiiiiii
        tmp = emit_indirect_indexed_read(sh2, SHR_TMP, SHR_R0, SHR_GBR, 0);
        tmp2 = rcache_get_tmp_arg(1);
        emith_and_r_r_imm(tmp2, tmp, (op & 0xff));
        goto end_rmw_op;
      case 0x0e00: // XOR.B #imm,@(R0,GBR)  11001110iiiiiiii
        tmp = emit_indirect_indexed_read(sh2, SHR_TMP, SHR_R0, SHR_GBR, 0);
        tmp2 = rcache_get_tmp_arg(1);
        emith_eor_r_r_imm(tmp2, tmp, (op & 0xff));
        goto end_rmw_op;
      case 0x0f00: // OR.B  #imm,@(R0,GBR)  11001111iiiiiiii
        tmp = emit_indirect_indexed_read(sh2, SHR_TMP, SHR_R0, SHR_GBR, 0);
        tmp2 = rcache_get_tmp_arg(1);
        emith_or_r_r_imm(tmp2, tmp, (op & 0xff));
      end_rmw_op:
        rcache_free_tmp(tmp);
        emit_indirect_indexed_write(sh2, SHR_TMP, SHR_R0, SHR_GBR, 0);
        goto end_op;
      }
      goto default_;

    /////////////////////////////////////////////
    case 0x0e: // MOV #imm,Rn   1110nnnniiiiiiii
      emit_move_r_imm32(GET_Rn(), (s8)op);
      goto end_op;

    default:
    default_:
      if (!(op_flags[i] & OF_B_IN_DS)) {
        elprintf_sh2(sh2, EL_ANOMALY,
          "drc: illegal op %04x @ %08x", op, pc - 2);
        exit(1);
      }
    }

end_op:
    rcache_unlock_all();

    cycles += opd->cycles;

    if (op_flags[i+1] & OF_DELAY_OP) {
      do_host_disasm(tcache_id);
      continue;
    }

    // test irq?
    if (drcf.test_irq && !drcf.pending_branch_direct) {
      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      FLUSH_CYCLES(sr);
      emith_sync_t(sr);
      if (!drcf.pending_branch_indirect)
        emit_move_r_imm32(SHR_PC, pc);
      rcache_flush();
      emith_call(sh2_drc_test_irq);
      drcf.test_irq = 0;
    }

    // branch handling
    if (drcf.pending_branch_direct)
    {
      struct op_data *opd_b = (op_flags[i] & OF_DELAY_OP) ? opd-1 : opd;
      u32 target_pc = opd_b->imm;
      int cond = -1;
      void *target = NULL;
      int ctaken = 0;

      if (OP_ISBRACND(opd_b->op))
        ctaken = (op_flags[i] & OF_DELAY_OP) ? 1 : 2;
      cycles += ctaken; // assume branch taken
#if LOOP_DETECTION
      if ((drcf.loop_type == OF_IDLE_LOOP ||
          (drcf.loop_type == OF_DELAY_LOOP && drcf.delay_reg >= 0)))
      {
        // idle or delay loop
        emit_sync_t_to_sr();
        emith_sh2_delay_loop(cycles, drcf.delay_reg);
        drcf.polling = drcf.loop_type = 0;
      }
#endif

      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      FLUSH_CYCLES(sr);
      rcache_clean();

      // emit condition test for conditional branch
      if (OP_ISBRACND(opd_b->op)) {
        cond = (opd_b->op == OP_BRANCH_CF) ? DCOND_EQ : DCOND_NE;
        if (delay_dep_fw & BITMASK1(SHR_T)) {
          emith_sync_t(sr);
          emith_tst_r_imm(sr, T_save);
        } else {
          cond = emith_tst_t(sr, (opd_b->op == OP_BRANCH_CT));
          if (emith_get_t_cond() >= 0) {
            if (opd_b->op == OP_BRANCH_CT)
              emith_or_r_imm_c(cond, sr, T);
            else
              emith_bic_r_imm_c(cond, sr, T);
          }
        }
      } else
        emith_sync_t(sr);
      // no modification of host status/flags between here and branching!

#if LINK_BRANCHES
      v = find_in_array(branch_target_pc, branch_target_count, target_pc);
      if (v >= 0)
      {
        // local branch
        if (branch_target_ptr[v]) {
          // jumps back can be linked here since host PC is already known
          target = branch_target_ptr[v];
        } else if (branch_patch_count < MAX_LOCAL_BRANCHES) {
          target = tcache_ptr;
          branch_patch_pc[branch_patch_count] = target_pc;
          branch_patch_ptr[branch_patch_count] = target;
          branch_patch_count++;
        }
        else
          dbg(1, "warning: too many local branches");
      }
#endif
      if (target == NULL)
      {
        // can't resolve branch locally, make a block exit
        rcache_clean();
        tmp = rcache_get_tmp_arg(0);
        emith_move_r_imm(tmp, target_pc);
        rcache_free_tmp(tmp);

#if CALL_STACK
        if ((opd_b->dest & BITMASK1(SHR_PR)) && pc+2 < end_pc) {
          // BSR
          tmp = rcache_get_tmp_arg(1);
          emith_call_link(tmp, sh2_drc_dispatcher_call);
          rcache_free_tmp(tmp);
        } else
#endif
          target = dr_prepare_ext_branch(block->entryp, target_pc, sh2->is_slave, tcache_id);
      }

      if (cond != -1) {
        emith_jump_cond_patchable(cond, target);
      }
      else if (target != NULL) {
        emith_jump_patchable(target);
        rcache_invalidate();
      }

      // branch not taken, correct cycle count
      if (ctaken)
        emith_add_r_imm(sr, ctaken << 12);
      // set T bit to reflect branch not taken for OP_BRANCH_CT/CF
      if (emith_get_t_cond() >= 0) // T is synced for all other cases
        emith_set_t(sr, opd_b->op == OP_BRANCH_CF);

      drcf.pending_branch_direct = 0;
      if (target_pc >= base_pc && target_pc < pc)
        drcf.polling = drcf.loop_type = 0;
    }
    else if (drcf.pending_branch_indirect) {
      void *target;
      u32 target_pc;

      sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
      FLUSH_CYCLES(sr);
      emith_sync_t(sr);
      rcache_clean();
      tmp = rcache_get_reg_arg(0, SHR_PC, NULL);
#if CALL_STACK
      struct op_data *opd_b = (op_flags[i] & OF_DELAY_OP) ? opd-1 : opd;
      if (opd_b->rm == SHR_PR) {
        // RTS
        emith_jump(sh2_drc_dispatcher_return);
      } else if ((opd_b->dest & BITMASK1(SHR_PR)) && pc+2 < end_pc) {
        // JSR/BSRF
        tmp = rcache_get_tmp_arg(1);
        emith_call_link(tmp, sh2_drc_dispatcher_call);
      } else
#endif
      if (gconst_get(SHR_PC, &target_pc)) {
        // JMP const, treat like unconditional direct branch
        target = dr_prepare_ext_branch(block->entryp, target_pc, sh2->is_slave, tcache_id);
        emith_jump_patchable(target);
      } else {
        // JMP
        emith_jump(sh2_drc_dispatcher);
      }
      rcache_invalidate();
      drcf.pending_branch_indirect = 0;
      drcf.polling = drcf.loop_type = 0;
    }

    do_host_disasm(tcache_id);
  }

  // check the last op
  if (op_flags[i-1] & OF_DELAY_OP)
    opd = &ops[i-2];
  else
    opd = &ops[i-1];

  if (! OP_ISBRAUC(opd->op))
  {
    void *target;

    s32 tmp = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
    FLUSH_CYCLES(tmp);
    emith_sync_t(tmp);

    rcache_clean();
    tmp = rcache_get_tmp_arg(0);
    emith_move_r_imm(tmp, pc);

    target = dr_prepare_ext_branch(block->entryp, pc, sh2->is_slave, tcache_id);
    if (target == NULL)
      return NULL;
    emith_jump_patchable(target);
    rcache_invalidate();
  } else
    rcache_flush();
  emith_flush();

  // link local branches
  for (i = 0; i < branch_patch_count; i++) {
    void *target;
    int t;
    t = find_in_array(branch_target_pc, branch_target_count, branch_patch_pc[i]);
    target = branch_target_ptr[t];
    if (target == NULL) {
      // flush pc and go back to dispatcher (this should no longer happen)
      dbg(1, "stray branch to %08x %p", branch_patch_pc[i], tcache_ptr);
      target = tcache_ptr;
      tmp = rcache_get_tmp_arg(0);
      emith_move_r_imm(tmp, branch_patch_pc[i]);
      rcache_flush();
      emith_jump(sh2_drc_dispatcher);
    }
    emith_jump_patch(branch_patch_ptr[i], target);
  }

  emith_pool_commit(0);

  dr_mark_memory(1, block, tcache_id, 0);

  tcache_ptrs[tcache_id] = tcache_ptr;

  host_instructions_updated(block_entry_ptr, tcache_ptr);

  do_host_disasm(tcache_id);

  dbg(2, " block #%d,%d -> %p tcache %d/%d, insns %d -> %d %.3f",
    tcache_id, blkid_main, tcache_ptr,
    tcache_ptr - tcache_bases[tcache_id], tcache_sizes[tcache_id],
    insns_compiled, host_insn_count, (float)host_insn_count / insns_compiled);
  if ((sh2->pc & 0xc6000000) == 0x02000000) { // ROM
    dbg(2, "  hash collisions %d/%d", hash_collisions, block_counts[tcache_id]);
    Pico32x.emu_flags |= P32XF_DRC_ROM_C;
  }
/*
 printf("~~~\n");
 tcache_dsm_ptrs[tcache_id] = block_entry_ptr;
 do_host_disasm(tcache_id);
 printf("~~~\n");
*/

#if (DRC_DEBUG)
  fflush(stdout);
#endif

  return block_entry_ptr;
}

static void sh2_generate_utils(void)
{
  int arg0, arg1, arg2, arg3, sr, tmp;

  host_arg2reg(arg0, 0);
  host_arg2reg(arg1, 1);
  host_arg2reg(arg2, 2);
  host_arg2reg(arg3, 3);
  emith_move_r_r(arg0, arg0); // nop
  emith_move_r_r(arg1, arg1); // nop
  emith_move_r_r(arg2, arg2); // nop
  emith_move_r_r(arg3, arg3); // nop
  emith_flush();

  // sh2_drc_write8(u32 a, u32 d)
  sh2_drc_write8 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg2, offsetof(SH2, write8_tab));
  emith_sh2_wcall(arg0, arg1, arg2, arg3);
  emith_flush();

  // sh2_drc_write16(u32 a, u32 d)
  sh2_drc_write16 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg2, offsetof(SH2, write16_tab));
  emith_sh2_wcall(arg0, arg1, arg2, arg3);
  emith_flush();

  // sh2_drc_write32(u32 a, u32 d)
  sh2_drc_write32 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg2, offsetof(SH2, write32_tab));
  emith_sh2_wcall(arg0, arg1, arg2, arg3);
  emith_flush();

  // d = sh2_drc_read8(u32 a)
  sh2_drc_read8 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read8_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CS);
  emith_and_r_r_c(DCOND_CC, arg0, arg3);
  emith_eor_r_imm_c(DCOND_CC, arg0, 1);
  emith_read8s_r_r_r_c(DCOND_CC, RET_REG, arg0, arg2);
  emith_ret_c(DCOND_CC);
  EMITH_SJMP_END(DCOND_CS);
  emith_move_r_r_ptr(arg1, CONTEXT_REG);
  emith_jump_reg(arg2);
  emith_flush();

  // d = sh2_drc_read16(u32 a)
  sh2_drc_read16 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read16_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CS);
  emith_and_r_r_c(DCOND_CC, arg0, arg3);
  emith_read16s_r_r_r_c(DCOND_CC, RET_REG, arg0, arg2);
  emith_ret_c(DCOND_CC);
  EMITH_SJMP_END(DCOND_CS);
  emith_move_r_r_ptr(arg1, CONTEXT_REG);
  emith_jump_reg(arg2);
  emith_flush();

  // d = sh2_drc_read32(u32 a)
  sh2_drc_read32 = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read32_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CS);
  emith_and_r_r_c(DCOND_CC, arg0, arg3);
  emith_read_r_r_r_c(DCOND_CC, RET_REG, arg0, arg2);
  emith_ror_c(DCOND_CC, RET_REG, RET_REG, 16);
  emith_ret_c(DCOND_CC);
  EMITH_SJMP_END(DCOND_CS);
  emith_move_r_r_ptr(arg1, CONTEXT_REG);
  emith_jump_reg(arg2);
  emith_flush();

  // d = sh2_drc_read8_poll(u32 a)
  sh2_drc_read8_poll = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read8_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CC);
  emith_move_r_r_ptr_c(DCOND_CS, arg1, CONTEXT_REG);
  emith_jump_reg_c(DCOND_CS, arg2);
  EMITH_SJMP_END(DCOND_CC);
  emith_and_r_r_r(arg1, arg0, arg3);
  emith_eor_r_imm(arg1, 1);
  emith_read8s_r_r_r(arg1, arg1, arg2);
  emith_push_ret(arg1);
  emith_move_r_r_ptr(arg2, CONTEXT_REG);
  emith_call(p32x_sh2_poll_memory8);
  emith_pop_and_ret(arg1);
  emith_flush();

  // d = sh2_drc_read16_poll(u32 a)
  sh2_drc_read16_poll = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read16_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CC);
  emith_move_r_r_ptr_c(DCOND_CS, arg1, CONTEXT_REG);
  emith_jump_reg_c(DCOND_CS, arg2);
  EMITH_SJMP_END(DCOND_CC);
  emith_and_r_r_r(arg1, arg0, arg3);
  emith_read16s_r_r_r(arg1, arg1, arg2);
  emith_push_ret(arg1);
  emith_move_r_r_ptr(arg2, CONTEXT_REG);
  emith_call(p32x_sh2_poll_memory16);
  emith_pop_and_ret(arg1);
  emith_flush();

  // d = sh2_drc_read32_poll(u32 a)
  sh2_drc_read32_poll = (void *)tcache_ptr;
  emith_ctx_read_ptr(arg1, offsetof(SH2, read32_map));
  emith_sh2_rcall(arg0, arg1, arg2, arg3);
  EMITH_SJMP_START(DCOND_CC);
  emith_move_r_r_ptr_c(DCOND_CS, arg1, CONTEXT_REG);
  emith_jump_reg_c(DCOND_CS, arg2);
  EMITH_SJMP_END(DCOND_CC);
  emith_and_r_r_r(arg1, arg0, arg3);
  emith_read_r_r_r(arg1, arg1, arg2);
  emith_ror(arg1, arg1, 16);
  emith_push_ret(arg1);
  emith_move_r_r_ptr(arg2, CONTEXT_REG);
  emith_call(p32x_sh2_poll_memory32);
  emith_pop_and_ret(arg1);
  emith_flush();

  // sh2_drc_exit(u32 pc)
  sh2_drc_exit = (void *)tcache_ptr;
  emith_ctx_write(arg0, SHR_PC * 4);
  emit_do_static_regs(1, arg2);
  emith_sh2_drc_exit();
  emith_flush();

#if CALL_STACK
  // sh2_drc_dispatcher_call(u32 pc, uptr host_pr)
  sh2_drc_dispatcher_call = (void *)tcache_ptr;
  emith_ctx_read(arg2, offsetof(SH2, rts_cache_idx));
  emith_add_r_imm(arg2, 2*sizeof(void *));
  emith_and_r_imm(arg2, (ARRAY_SIZE(sh2s->rts_cache)-1) * 2*sizeof(void *));
  emith_ctx_write(arg2, offsetof(SH2, rts_cache_idx));
  emith_add_r_r_ptr_imm(arg3, CONTEXT_REG, offsetof(SH2, rts_cache) + sizeof(void *));
  emith_write_r_r_r_ptr_wb(arg1, arg2, arg3);
  emith_ctx_read(arg3, SHR_PR * 4);
  emith_write_r_r_offs(arg3, arg2, (s8)-sizeof(void *));
  emith_flush();
  // FALLTHROUGH
#endif
  // sh2_drc_dispatcher(u32 pc)
  sh2_drc_dispatcher = (void *)tcache_ptr;
  emith_ctx_write(arg0, SHR_PC * 4);
#if BRANCH_CACHE
  // check if PC is in branch target cache
  emith_and_r_r_imm(arg1, arg0, (ARRAY_SIZE(sh2s->branch_cache)-1)*8);
  emith_add_r_r_r_lsl_ptr(arg1, CONTEXT_REG, arg1, sizeof(void *) == 8 ? 1 : 0);
  emith_read_r_r_offs(arg2, arg1, offsetof(SH2, branch_cache));
  emith_cmp_r_r(arg2, arg0);
  EMITH_SJMP_START(DCOND_NE);
#if (DRC_DEBUG & 128)
  emith_move_r_ptr_imm(arg2, (uptr)&bchit);
  emith_read_r_r_offs_c(DCOND_EQ, arg3, arg2, 0);
  emith_add_r_imm_c(DCOND_EQ, arg3, 1);
  emith_write_r_r_offs_c(DCOND_EQ, arg3, arg2, 0);
#endif
  emith_read_r_r_offs_ptr_c(DCOND_EQ, RET_REG, arg1, offsetof(SH2, branch_cache) + sizeof(void *));
  emith_jump_reg_c(DCOND_EQ, RET_REG);
  EMITH_SJMP_END(DCOND_NE);
#endif
  emith_ctx_read(arg1, offsetof(SH2, is_slave));
  emith_add_r_r_ptr_imm(arg2, CONTEXT_REG, offsetof(SH2, drc_tmp));
  emith_call(dr_lookup_block);
#if BRANCH_CACHE
  // store PC and block entry ptr (in arg0) in branch target cache
  emith_tst_r_r_ptr(RET_REG, RET_REG);
  EMITH_SJMP_START(DCOND_EQ);
#if (DRC_DEBUG & 128)
  emith_move_r_ptr_imm(arg2, (uptr)&bcmiss);
  emith_read_r_r_offs_c(DCOND_NE, arg3, arg2, 0);
  emith_add_r_imm_c(DCOND_NE, arg3, 1);
  emith_write_r_r_offs_c(DCOND_NE, arg3, arg2, 0);
#endif
  emith_ctx_read_c(DCOND_NE, arg2, SHR_PC * 4);
  emith_and_r_r_imm(arg1, arg2, (ARRAY_SIZE(sh2s->branch_cache)-1)*8);
  emith_add_r_r_r_lsl_ptr(arg1, CONTEXT_REG, arg1, sizeof(void *) == 8 ? 1 : 0);
  emith_write_r_r_offs_c(DCOND_NE, arg2, arg1, offsetof(SH2, branch_cache));
  emith_write_r_r_offs_ptr_c(DCOND_NE, RET_REG, arg1, offsetof(SH2, branch_cache) + sizeof(void *));
  EMITH_SJMP_END(DCOND_EQ);
#endif
  emit_block_entry();
  // lookup failed, call sh2_translate()
  emith_move_r_r_ptr(arg0, CONTEXT_REG);
  emith_ctx_read(arg1, offsetof(SH2, drc_tmp)); // tcache_id
  emith_call(sh2_translate);
  emit_block_entry();
  // XXX: can't translate, fail
  emith_call(dr_failure);
  emith_flush();

#if CALL_STACK
  // sh2_drc_dispatcher_return(u32 pc)
  sh2_drc_dispatcher_return = (void *)tcache_ptr;
  emith_ctx_read(arg2, offsetof(SH2, rts_cache_idx));
  emith_add_r_r_ptr_imm(arg1, CONTEXT_REG, offsetof(SH2, rts_cache));
  emith_read_r_r_r_wb(arg3, arg1, arg2);
  emith_cmp_r_r(arg0, arg3);
#if (DRC_DEBUG & 128)
  EMITH_SJMP_START(DCOND_EQ);
  emith_move_r_ptr_imm(arg2, (uptr)&rcmiss);
  emith_read_r_r_offs_c(DCOND_NE, arg1, arg2, 0);
  emith_add_r_imm_c(DCOND_NE, arg1, 1);
  emith_write_r_r_offs_c(DCOND_NE, arg1, arg2, 0);
  EMITH_SJMP_END(DCOND_EQ);
#endif
  emith_jump_cond(DCOND_NE, sh2_drc_dispatcher);
  emith_read_r_r_offs_ptr(arg0, arg1, sizeof(void *));
  emith_sub_r_imm(arg2, 2*sizeof(void *));
  emith_and_r_imm(arg2, (ARRAY_SIZE(sh2s->rts_cache)-1) * 2*sizeof(void *));
  emith_ctx_write(arg2, offsetof(SH2, rts_cache_idx));
#if (DRC_DEBUG & 128)
  emith_move_r_ptr_imm(arg2, (uptr)&rchit);
  emith_read_r_r_offs(arg1, arg2, 0);
  emith_add_r_imm(arg1, 1);
  emith_write_r_r_offs(arg1, arg2, 0);
#endif
  emith_jump_reg(arg0);
  emith_flush();
#endif
 
  // sh2_drc_test_irq(void)
  // assumes it's called from main function (may jump to dispatcher)
  sh2_drc_test_irq = (void *)tcache_ptr;
  emith_ctx_read(arg1, offsetof(SH2, pending_level));
  sr = rcache_get_reg(SHR_SR, RC_GR_READ, NULL);
  emith_lsr(arg0, sr, I_SHIFT);
  emith_and_r_imm(arg0, 0x0f);
  emith_cmp_r_r(arg1, arg0); // pending_level > ((sr >> 4) & 0x0f)?
  EMITH_SJMP_START(DCOND_GT);
  emith_ret_c(DCOND_LE);     // nope, return
  EMITH_SJMP_END(DCOND_GT);
  // adjust SP
  tmp = rcache_get_reg(SHR_SP, RC_GR_RMW, NULL);
  emith_sub_r_imm(tmp, 4*2);
  rcache_clean();
  // push SR
  tmp = rcache_get_reg_arg(0, SHR_SP, NULL);
  emith_add_r_imm(tmp, 4);
  tmp = rcache_get_reg_arg(1, SHR_SR, NULL);
  emith_clear_msb(tmp, tmp, 22);
  emith_move_r_r_ptr(arg2, CONTEXT_REG);
  emith_call(p32x_sh2_write32); // XXX: use sh2_drc_write32?
  rcache_invalidate();
  // push PC
  rcache_get_reg_arg(0, SHR_SP, NULL);
  emith_ctx_read(arg1, SHR_PC * 4);
  emith_move_r_r_ptr(arg2, CONTEXT_REG);
  emith_call(p32x_sh2_write32);
  rcache_invalidate();
  // update I, cycles, do callback
  emith_ctx_read(arg1, offsetof(SH2, pending_level));
  sr = rcache_get_reg(SHR_SR, RC_GR_RMW, NULL);
  emith_bic_r_imm(sr, I);
  emith_or_r_r_lsl(sr, arg1, I_SHIFT);
  emith_sub_r_imm(sr, 13 << 12); // at least 13 cycles
  rcache_flush();
  emith_move_r_r_ptr(arg0, CONTEXT_REG);
  emith_call_ctx(offsetof(SH2, irq_callback)); // vector = sh2->irq_callback(sh2, level);
  // obtain new PC
  emith_ctx_read(arg1, SHR_VBR * 4);
  emith_add_r_r_r_lsl(arg0, arg1, RET_REG, 2);
  emith_call(sh2_drc_read32);
  if (arg0 != RET_REG)
    emith_move_r_r(arg0, RET_REG);
#if defined(__i386__) || defined(__x86_64__)
  emith_add_r_r_ptr_imm(xSP, xSP, sizeof(void *)); // fix stack
#endif
  emith_jump(sh2_drc_dispatcher);
  rcache_invalidate();
  emith_flush();

  // sh2_drc_entry(SH2 *sh2)
  sh2_drc_entry = (void *)tcache_ptr;
  emith_sh2_drc_entry();
  emith_move_r_r_ptr(CONTEXT_REG, arg0); // move ctx, arg0
  emit_do_static_regs(0, arg2);
  emith_call(sh2_drc_test_irq);
  emith_ctx_read(arg0, SHR_PC * 4);
  emith_jump(sh2_drc_dispatcher);
  emith_flush();

#ifdef PDB_NET
  // debug
  #define MAKE_READ_WRAPPER(func) { \
    void *tmp = (void *)tcache_ptr; \
    emith_push_ret(); \
    emith_call(func); \
    emith_ctx_read(arg2, offsetof(SH2, pdb_io_csum[0]));  \
    emith_addf_r_r(arg2, arg0);                           \
    emith_ctx_write(arg2, offsetof(SH2, pdb_io_csum[0])); \
    emith_ctx_read(arg2, offsetof(SH2, pdb_io_csum[1]));  \
    emith_adc_r_imm(arg2, 0x01000000);                    \
    emith_ctx_write(arg2, offsetof(SH2, pdb_io_csum[1])); \
    emith_pop_and_ret(); \
    emith_flush(); \
    func = tmp; \
  }
  #define MAKE_WRITE_WRAPPER(func) { \
    void *tmp = (void *)tcache_ptr; \
    emith_ctx_read(arg2, offsetof(SH2, pdb_io_csum[0]));  \
    emith_addf_r_r(arg2, arg1);                           \
    emith_ctx_write(arg2, offsetof(SH2, pdb_io_csum[0])); \
    emith_ctx_read(arg2, offsetof(SH2, pdb_io_csum[1]));  \
    emith_adc_r_imm(arg2, 0x01000000);                    \
    emith_ctx_write(arg2, offsetof(SH2, pdb_io_csum[1])); \
    emith_move_r_r_ptr(arg2, CONTEXT_REG);                \
    emith_jump(func); \
    emith_flush(); \
    func = tmp; \
  }

  MAKE_READ_WRAPPER(sh2_drc_read8);
  MAKE_READ_WRAPPER(sh2_drc_read16);
  MAKE_READ_WRAPPER(sh2_drc_read32);
  MAKE_WRITE_WRAPPER(sh2_drc_write8);
  MAKE_WRITE_WRAPPER(sh2_drc_write16);
  MAKE_WRITE_WRAPPER(sh2_drc_write32);
  MAKE_READ_WRAPPER(sh2_drc_read8_poll);
  MAKE_READ_WRAPPER(sh2_drc_read16_poll);
  MAKE_READ_WRAPPER(sh2_drc_read32_poll);
#endif

  emith_pool_commit(0);
  rcache_invalidate();
#if (DRC_DEBUG & 4)
  host_dasm_new_symbol(sh2_drc_entry);
  host_dasm_new_symbol(sh2_drc_dispatcher);
#if CALL_STACK
  host_dasm_new_symbol(sh2_drc_dispatcher_call);
  host_dasm_new_symbol(sh2_drc_dispatcher_return);
#endif
  host_dasm_new_symbol(sh2_drc_exit);
  host_dasm_new_symbol(sh2_drc_test_irq);
  host_dasm_new_symbol(sh2_drc_write8);
  host_dasm_new_symbol(sh2_drc_write16);
  host_dasm_new_symbol(sh2_drc_write32);
  host_dasm_new_symbol(sh2_drc_read8);
  host_dasm_new_symbol(sh2_drc_read16);
  host_dasm_new_symbol(sh2_drc_read32);
  host_dasm_new_symbol(sh2_drc_read8_poll);
  host_dasm_new_symbol(sh2_drc_read16_poll);
  host_dasm_new_symbol(sh2_drc_read32_poll);
#endif
}

static void sh2_smc_rm_block_entry(struct block_desc *bd, int tcache_id, u32 nolit, int free)
{
  struct block_link *bl;
  u32 i;

  free = free || nolit; // block is invalid if literals are overwritten
  dbg(2,"  %sing block %08x-%08x,%08x-%08x, blkid %d,%d", free?"delet":"disabl",
    bd->addr, bd->addr + bd->size, bd->addr_lit, bd->addr_lit + bd->size_lit,
    tcache_id, bd - block_tables[tcache_id]);
  if (bd->addr == 0 || bd->entry_count == 0) {
    dbg(1, "  killing dead block!? %08x", bd->addr);
    return;
  }

  // remove from hash table, make incoming links unresolved
  if (bd->active) {
    for (i = 0; i < bd->entry_count; i++) {
      rm_from_hashlist(&bd->entryp[i], tcache_id);

      while ((bl = bd->entryp[i].links) != NULL) {
        dr_block_unlink(bl, 1);
        add_to_hashlist_unresolved(bl, tcache_id);
      }
    }

    dr_mark_memory(-1, bd, tcache_id, nolit);
    add_to_block_list(&inactive_blocks[tcache_id], bd);
  }
  bd->active = 0;

  if (free) {
    // revoke outgoing links
    for (bl = bd->entryp[0].o_links; bl != NULL; bl = bl->o_next) {
      if (bl->target)
        dr_block_unlink(bl, 0);
      else
        rm_from_hashlist_unresolved(bl, tcache_id);
      bl->jump = NULL;
      bl->next = blink_free[bl->tcache_id];
      blink_free[bl->tcache_id] = bl;
    }
    bd->entryp[0].o_links = NULL;
    // invalidate block
    rm_from_block_lists(bd);
    bd->addr = bd->size = bd->addr_lit = bd->size_lit = 0;
    bd->entry_count = 0;
  }
}

static void sh2_smc_rm_blocks(u32 a, int tcache_id, u32 shift)
{
  struct block_list **blist, *entry, *next;
  u32 mask = ram_sizes[tcache_id] - 1;
  u32 wtmask = ~0x20000000; // writethrough area mask
  u32 start_addr, end_addr;
  u32 start_lit, end_lit;
  struct block_desc *block;
#if (DRC_DEBUG & 2)
  int removed = 0;
#endif

  // ignore cache-through
  a &= wtmask;

  blist = &inval_lookup[tcache_id][(a & mask) / INVAL_PAGE_SIZE];
  entry = *blist;
  // go through the block list for this range
  while (entry != NULL) {
    next = entry->next;
    block = entry->block;
    start_addr = block->addr & wtmask;
    end_addr = start_addr + block->size;
    start_lit = block->addr_lit & wtmask;
    end_lit = start_lit + block->size_lit;
    // disable/delete block if it covers the modified address
    if ((start_addr <= a && a < end_addr) ||
        (start_lit <= a && a < end_lit))
    {
      dbg(2, "smc remove @%08x", a);
      end_addr = (start_lit <= a && block->size_lit ? a : 0);
      sh2_smc_rm_block_entry(block, tcache_id, end_addr, 0);
#if (DRC_DEBUG & 2)
      removed = 1;
#endif
    }
    entry = next;
  }
#if (DRC_DEBUG & 2)
  if (!removed)
    dbg(2, "rm_blocks called @%08x, no work?", a);
#endif
#if BRANCH_CACHE
  if (tcache_id)
    memset32(sh2s[tcache_id-1].branch_cache, -1, sizeof(sh2s[0].branch_cache)/4);
  else {
    memset32(sh2s[0].branch_cache, -1, sizeof(sh2s[0].branch_cache)/4);
    memset32(sh2s[1].branch_cache, -1, sizeof(sh2s[1].branch_cache)/4);
  }
#endif
#if CALL_STACK
  if (tcache_id) {
    memset32(sh2s[tcache_id-1].rts_cache, -1, sizeof(sh2s[0].rts_cache)/4);
    sh2s[tcache_id-1].rts_cache_idx = 0;
  } else {
    memset32(sh2s[0].rts_cache, -1, sizeof(sh2s[0].rts_cache)/4);
    memset32(sh2s[1].rts_cache, -1, sizeof(sh2s[1].rts_cache)/4);
    sh2s[0].rts_cache_idx = sh2s[1].rts_cache_idx = 0;
  }
#endif
}

void sh2_drc_wcheck_ram(unsigned int a, int val, SH2 *sh2)
{
  dbg(2, "%csh2 smc check @%08x v=%d", sh2->is_slave ? 's' : 'm', a, val);
  sh2_smc_rm_blocks(a, 0, SH2_DRCBLK_RAM_SHIFT);
}

void sh2_drc_wcheck_da(unsigned int a, int val, SH2 *sh2)
{
  int cpuid = sh2->is_slave;
  dbg(2, "%csh2 smc check @%08x v=%d", cpuid ? 's' : 'm', a, val);
  sh2_smc_rm_blocks(a, 1 + cpuid, SH2_DRCBLK_DA_SHIFT);
}

int sh2_execute_drc(SH2 *sh2c, int cycles)
{
  int ret_cycles;

  // cycles are kept in SHR_SR unused bits (upper 20)
  // bit11 contains T saved for delay slot
  // others are usual SH2 flags
  sh2c->sr &= 0x3f3;
  sh2c->sr |= cycles << 12;
  sh2_drc_entry(sh2c);

  // TODO: irq cycles
  ret_cycles = (signed int)sh2c->sr >> 12;
  if (ret_cycles > 0)
    dbg(1, "warning: drc returned with cycles: %d", ret_cycles);

  sh2c->sr &= 0x3f3;
  return ret_cycles;
}

static void block_stats(void)
{
#if (DRC_DEBUG & 2)
  int c, b, i;
  long total = 0;

  printf("block stats:\n");
  for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
    for (i = 0; i < block_counts[b]; i++)
      if (block_tables[b][i].addr != 0)
        total += block_tables[b][i].refcount;
    for (i = block_limit[b]; i < block_max_counts[b]; i++)
      if (block_tables[b][i].addr != 0)
        total += block_tables[b][i].refcount;
  }
  printf("total: %ld\n",total);

  for (c = 0; c < 20; c++) {
    struct block_desc *blk, *maxb = NULL;
    int max = 0;
    for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
      for (i = 0; i < block_counts[b]; i++) {
        blk = &block_tables[b][i];
        if (blk->addr != 0 && blk->refcount > max) {
          max = blk->refcount;
          maxb = blk;
        }
      }
      for (i = block_limit[b]; i < block_max_counts[b]; i++) {
        blk = &block_tables[b][i];
        if (blk->addr != 0 && blk->refcount > max) {
          max = blk->refcount;
          maxb = blk;
        }
      }
    }
    if (maxb == NULL)
      break;
    printf("%08x %p %9d %2.3f%%\n", maxb->addr, maxb->tcache_ptr, maxb->refcount,
      (double)maxb->refcount / total * 100.0);
    maxb->refcount = 0;
  }

  for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
    for (i = 0; i < block_counts[b]; i++)
      block_tables[b][i].refcount = 0;
    for (i = block_limit[b]; i < block_max_counts[b]; i++)
      block_tables[b][i].refcount = 0;
  }
#endif
}

void entry_stats(void)
{
#if (DRC_DEBUG & 32)
  int c, b, i, j;
  long total = 0;

  printf("block entry stats:\n");
  for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
    for (i = 0; i < block_counts[b]; i++)
      for (j = 0; j < block_tables[b][i].entry_count; j++)
        total += block_tables[b][i].entryp[j].entry_count;
    for (i = block_limit[b]; i < block_max_counts[b]; i++)
      for (j = 0; j < block_tables[b][i].entry_count; j++)
        total += block_tables[b][i].entryp[j].entry_count;
  }
  printf("total: %ld\n",total);

  for (c = 0; c < 20; c++) {
    struct block_desc *blk;
    struct block_entry *maxb = NULL;
    int max = 0;
    for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
      for (i = 0; i < block_counts[b]; i++) {
        blk = &block_tables[b][i];
        for (j = 0; j < blk->entry_count; j++)
          if (blk->entryp[j].entry_count > max) {
            max = blk->entryp[j].entry_count;
            maxb = &blk->entryp[j];
          }
      }
      for (i = block_limit[b]; i < block_max_counts[b]; i++) {
        blk = &block_tables[b][i];
        for (j = 0; j < blk->entry_count; j++)
          if (blk->entryp[j].entry_count > max) {
            max = blk->entryp[j].entry_count;
            maxb = &blk->entryp[j];
          }
      }
    }
    if (maxb == NULL)
      break;
    printf("%08x %p %9d %2.3f%%\n", maxb->pc, maxb->tcache_ptr, maxb->entry_count,
      (double)100 * maxb->entry_count / total);
    maxb->entry_count = 0;
  }

  for (b = 0; b < ARRAY_SIZE(block_tables); b++) {
    for (i = 0; i < block_counts[b]; i++)
      for (j = 0; j < block_tables[b][i].entry_count; j++)
        block_tables[b][i].entryp[j].entry_count = 0;
    for (i = block_limit[b]; i < block_max_counts[b]; i++)
      for (j = 0; j < block_tables[b][i].entry_count; j++)
        block_tables[b][i].entryp[j].entry_count = 0;
  }
#endif
}

static void backtrace(void)
{
#if (DRC_DEBUG & 1024)
  int i;
  printf("backtrace master:\n");
  for (i = 0; i < ARRAY_SIZE(csh2[0]); i++)
    SH2_DUMP(&csh2[0][i], "bt msh2");
  printf("backtrace slave:\n");
  for (i = 0; i < ARRAY_SIZE(csh2[1]); i++)
    SH2_DUMP(&csh2[1][i], "bt ssh2");
#endif
}

static void state_dump(void)
{
#if (DRC_DEBUG & 2048)
  int i;

  SH2_DUMP(&sh2s[0], "master");
  printf("VBR msh2: %x\n", sh2s[0].vbr);
  for (i = 0; i < 0x60; i++) {
    printf("%08x ",p32x_sh2_read32(sh2s[0].vbr + i*4, &sh2s[0]));
    if ((i+1) % 8 == 0) printf("\n");
  }
  printf("stack msh2: %x\n", sh2s[0].r[15]);
  for (i = -0x30; i < 0x30; i++) {
    printf("%08x ",p32x_sh2_read32(sh2s[0].r[15] + i*4, &sh2s[0]));
    if ((i+1) % 8 == 0) printf("\n");
  }
  SH2_DUMP(&sh2s[1], "slave");
  printf("VBR ssh2: %x\n", sh2s[1].vbr);
  for (i = 0; i < 0x60; i++) {
    printf("%08x ",p32x_sh2_read32(sh2s[1].vbr + i*4, &sh2s[1]));
    if ((i+1) % 8 == 0) printf("\n");
  }
  printf("stack ssh2: %x\n", sh2s[1].r[15]);
  for (i = -0x30; i < 0x30; i++) {
    printf("%08x ",p32x_sh2_read32(sh2s[1].r[15] + i*4, &sh2s[1]));
    if ((i+1) % 8 == 0) printf("\n");
  }
#endif
}

static void bcache_stats(void)
{
#if (DRC_DEBUG & 128)
  int i;
#if CALL_STACK
  for (i = 1; i < ARRAY_SIZE(sh2s->rts_cache); i++)
    if (sh2s[0].rts_cache[i].pc == -1 && sh2s[1].rts_cache[i].pc == -1) break;

  printf("return cache hits:%d misses:%d depth: %d\n", rchit, rcmiss, i);
#endif
#if BRANCH_CACHE
  printf("branch cache hits:%d misses:%d\n", bchit, bcmiss);
  printf("branch cache master:\n");
  for (i = 0; i < ARRAY_SIZE(sh2s[0].branch_cache); i++) {
    printf("%08x ",sh2s[0].branch_cache[i].pc);
    if ((i+1) % 8 == 0) printf("\n");
  }
  printf("branch cache slave:\n");
  for (i = 0; i < ARRAY_SIZE(sh2s[1].branch_cache); i++) {
    printf("%08x ",sh2s[1].branch_cache[i].pc);
    if ((i+1) % 8 == 0) printf("\n");
  }
#endif
#endif
}

void sh2_drc_flush_all(void)
{
  backtrace();
  state_dump();
  block_stats();
  entry_stats();
  bcache_stats();
  flush_tcache(0);
  flush_tcache(1);
  flush_tcache(2);
  Pico32x.emu_flags &= ~P32XF_DRC_ROM_C;
}

void sh2_drc_mem_setup(SH2 *sh2)
{
  // fill the convenience pointers
  sh2->p_bios = sh2->is_slave ? Pico32xMem->sh2_rom_s.w : Pico32xMem->sh2_rom_m.w;
  sh2->p_da = sh2->data_array;
  sh2->p_sdram = Pico32xMem->sdram;
  sh2->p_rom = Pico.rom;
  // sh2->p_dram filled in dram bank switching
  sh2->p_drcblk_da = Pico32xMem->drcblk_da[!!sh2->is_slave];
  sh2->p_drcblk_ram = Pico32xMem->drcblk_ram;
}

void sh2_drc_frame(void)
{
}

int sh2_drc_init(SH2 *sh2)
{
  int i;

  if (block_tables[0] == NULL)
  {
    for (i = 0; i < TCACHE_BUFFERS; i++) {
      block_tables[i] = calloc(block_max_counts[i], sizeof(*block_tables[0]));
      if (block_tables[i] == NULL)
        goto fail;
      // max 2 block links (exits) per block
      block_link_pool[i] = calloc(block_link_pool_max_counts[i],
                          sizeof(*block_link_pool[0]));
      if (block_link_pool[i] == NULL)
        goto fail;

      inval_lookup[i] = calloc(ram_sizes[i] / INVAL_PAGE_SIZE,
                               sizeof(inval_lookup[0]));
      if (inval_lookup[i] == NULL)
        goto fail;

      hash_tables[i] = calloc(hash_table_sizes[i], sizeof(*hash_tables[0]));
      if (hash_tables[i] == NULL)
        goto fail;

      unresolved_links[i] = calloc(hash_table_sizes[i], sizeof(*unresolved_links[0]));
      if (unresolved_links[i] == NULL)
        goto fail;
    }
    memset(block_counts, 0, sizeof(block_counts));
    for (i = 0; i < ARRAY_SIZE(block_counts); i++) {
      block_limit[i] = block_max_counts[i] - 1;
    }
    memset(block_link_pool_counts, 0, sizeof(block_link_pool_counts));
    for (i = 0; i < ARRAY_SIZE(blink_free); i++) {
      blink_free[i] = NULL;
    }

    drc_cmn_init();
    rcache_init();
    tcache_ptr = tcache;
    sh2_generate_utils();
    host_instructions_updated(tcache, tcache_ptr);

    tcache_bases[0] = tcache_ptrs[0] = tcache_ptr;
    tcache_limit[0] = tcache_bases[0] + tcache_sizes[0] - (tcache_ptr-tcache);
    for (i = 1; i < ARRAY_SIZE(tcache_bases); i++) {
      tcache_bases[i] = tcache_ptrs[i] = tcache_bases[i - 1] + tcache_sizes[i - 1];
      tcache_limit[i] = tcache_bases[i] + tcache_sizes[i];
    }

#if (DRC_DEBUG & 4)
    for (i = 0; i < ARRAY_SIZE(block_tables); i++)
      tcache_dsm_ptrs[i] = tcache_bases[i];
    // disasm the utils
    tcache_dsm_ptrs[0] = tcache;
    do_host_disasm(0);
    fflush(stdout);
#endif
#if (DRC_DEBUG & 1)
    hash_collisions = 0;
#endif
  }
  memset(sh2->branch_cache, -1, sizeof(sh2->branch_cache));
  memset(sh2->rts_cache, -1, sizeof(sh2->rts_cache));
  sh2->rts_cache_idx = 0;

  return 0;

fail:
  sh2_drc_finish(sh2);
  return -1;
}

void sh2_drc_finish(SH2 *sh2)
{
  struct block_list *bl, *bn;
  int i;

  if (block_tables[0] == NULL)
    return;

  sh2_drc_flush_all();

  for (i = 0; i < TCACHE_BUFFERS; i++) {
#if (DRC_DEBUG & 4)
    printf("~~~ tcache %d\n", i);
#if 0
    tcache_dsm_ptrs[i] = tcache_bases[i];
    tcache_ptr = tcache_ptrs[i];
    do_host_disasm(i);
    if (tcache_limit[i] < tcache_bases[i] + tcache_sizes[i]) {
      tcache_dsm_ptrs[i] = tcache_limit[i];
      tcache_ptr = tcache_bases[i] + tcache_sizes[i];
      do_host_disasm(i);
    }
#endif
    printf("max links: %d\n", block_link_pool_counts[i]);
#endif

    if (block_tables[i] != NULL)
      free(block_tables[i]);
    block_tables[i] = NULL;
    if (block_link_pool[i] != NULL)
      free(block_link_pool[i]);
    block_link_pool[i] = NULL;
    blink_free[i] = NULL;

    if (inval_lookup[i] != NULL)
      free(inval_lookup[i]);
    inval_lookup[i] = NULL;

    if (hash_tables[i] != NULL) {
      free(hash_tables[i]);
      hash_tables[i] = NULL;
    }
  }

  for (bl = blist_free; bl; bl = bn) {
    bn = bl->next;
    free(bl);
  }
  blist_free = NULL;

  drc_cmn_cleanup();
}

#endif /* DRC_SH2 */

static void *dr_get_pc_base(u32 pc, SH2 *sh2)
{
  void *ret = NULL;
  u32 mask = 0;

  ret = p32x_sh2_get_mem_ptr(pc, &mask, sh2);
  if (ret == NULL)
    return (void *)-1; // NULL is valid value

  return (char *)ret - (pc & ~mask);
}

u16 scan_block(u32 base_pc, int is_slave, u8 *op_flags, u32 *end_pc_out,
  u32 *base_literals_out, u32 *end_literals_out)
{
  u16 *dr_pc_base;
  u32 pc, op, tmp;
  u32 end_pc, end_literals = 0;
  u32 lowest_literal = 0;
  u32 lowest_mova = 0;
  struct op_data *opd;
  int next_is_delay = 0;
  int end_block = 0;
  int i, i_end;
  u32 crc = 0;
  // 2nd pass stuff
  int last_btarget; // loop detector 
  enum { T_UNKNOWN, T_CLEAR, T_SET } t; // T propagation state

  memset(op_flags, 0, sizeof(*op_flags) * BLOCK_INSN_LIMIT);
  op_flags[0] |= OF_BTARGET; // block start is always a target

  dr_pc_base = dr_get_pc_base(base_pc, &sh2s[!!is_slave]);

  // 1st pass: disassemble
  for (i = 0, pc = base_pc; ; i++, pc += 2) {
    // we need an ops[] entry after the last one initialized,
    // so do it before end_block checks
    opd = &ops[i];
    opd->op = OP_UNHANDLED;
    opd->rm = -1;
    opd->source = opd->dest = 0;
    opd->cycles = 1;
    opd->imm = 0;

    if (next_is_delay) {
      op_flags[i] |= OF_DELAY_OP;
      next_is_delay = 0;
    }
    else if (end_block || i >= BLOCK_INSN_LIMIT - 2)
      break;
    else if ((lowest_mova && lowest_mova <= pc) ||
              (lowest_literal && lowest_literal <= pc))
      break; // text area collides with data area

    op = FETCH_OP(pc);
    switch ((op & 0xf000) >> 12)
    {
    /////////////////////////////////////////////
    case 0x00:
      switch (op & 0x0f)
      {
      case 0x02:
        switch (GET_Fx())
        {
        case 0: // STC SR,Rn  0000nnnn00000010
          tmp = SHR_SR;
          break;
        case 1: // STC GBR,Rn 0000nnnn00010010
          tmp = SHR_GBR;
          break;
        case 2: // STC VBR,Rn 0000nnnn00100010
          tmp = SHR_VBR;
          break;
        default:
          goto undefined;
        }
        opd->op = OP_MOVE;
        opd->source = BITMASK1(tmp);
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x03:
        CHECK_UNHANDLED_BITS(0xd0, undefined);
        // BRAF Rm    0000mmmm00100011
        // BSRF Rm    0000mmmm00000011
        opd->op = OP_BRANCH_RF;
        opd->rm = GET_Rn();
        opd->source = BITMASK2(SHR_PC, opd->rm);
        opd->dest = BITMASK1(SHR_PC);
        if (!(op & 0x20))
          opd->dest |= BITMASK1(SHR_PR);
        opd->cycles = 2;
        next_is_delay = 1;
        if (!(opd->dest & BITMASK1(SHR_PR)))
          end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
        else
          op_flags[i+1+next_is_delay] |= OF_BTARGET;
        break;
      case 0x04: // MOV.B Rm,@(R0,Rn)   0000nnnnmmmm0100
      case 0x05: // MOV.W Rm,@(R0,Rn)   0000nnnnmmmm0101
      case 0x06: // MOV.L Rm,@(R0,Rn)   0000nnnnmmmm0110
        opd->source = BITMASK3(GET_Rm(), SHR_R0, GET_Rn());
        opd->dest = BITMASK1(SHR_MEM);
        break;
      case 0x07:
        // MUL.L     Rm,Rn      0000nnnnmmmm0111
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_MACL);
        opd->cycles = 2;
        break;
      case 0x08:
        CHECK_UNHANDLED_BITS(0xf00, undefined);
        switch (GET_Fx())
        {
        case 0: // CLRT               0000000000001000
          opd->op = OP_SETCLRT;
          opd->dest = BITMASK1(SHR_T);
          opd->imm = 0;
          break;
        case 1: // SETT               0000000000011000
          opd->op = OP_SETCLRT;
          opd->dest = BITMASK1(SHR_T);
          opd->imm = 1;
          break;
        case 2: // CLRMAC             0000000000101000
          opd->dest = BITMASK3(SHR_T, SHR_MACL, SHR_MACH);
          break;
        default:
          goto undefined;
        }
        break;
      case 0x09:
        switch (GET_Fx())
        {
        case 0: // NOP        0000000000001001
          CHECK_UNHANDLED_BITS(0xf00, undefined);
          break;
        case 1: // DIV0U      0000000000011001
          CHECK_UNHANDLED_BITS(0xf00, undefined);
          opd->dest = BITMASK2(SHR_SR, SHR_T);
          break;
        case 2: // MOVT Rn    0000nnnn00101001
          opd->source = BITMASK1(SHR_T);
          opd->dest = BITMASK1(GET_Rn());
          break;
        default:
          goto undefined;
        }
        break;
      case 0x0a:
        switch (GET_Fx())
        {
        case 0: // STS      MACH,Rn   0000nnnn00001010
          tmp = SHR_MACH;
          break;
        case 1: // STS      MACL,Rn   0000nnnn00011010
          tmp = SHR_MACL;
          break;
        case 2: // STS      PR,Rn     0000nnnn00101010
          tmp = SHR_PR;
          break;
        default:
          goto undefined;
        }
        opd->op = OP_MOVE;
        opd->source = BITMASK1(tmp);
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x0b:
        CHECK_UNHANDLED_BITS(0xf00, undefined);
        switch (GET_Fx())
        {
        case 0: // RTS        0000000000001011
          opd->op = OP_BRANCH_R;
          opd->rm = SHR_PR;
          opd->source = BITMASK1(opd->rm);
          opd->dest = BITMASK1(SHR_PC);
          opd->cycles = 2;
          next_is_delay = 1;
          end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
          break;
        case 1: // SLEEP      0000000000011011
          opd->op = OP_SLEEP;
          end_block = 1;
          break;
        case 2: // RTE        0000000000101011
          opd->op = OP_RTE;
          opd->source = BITMASK1(SHR_SP);
          opd->dest = BITMASK3(SHR_SP, SHR_SR, SHR_PC);
          opd->cycles = 4;
          next_is_delay = 1;
          end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
          break;
        default:
          goto undefined;
        }
        break;
      case 0x0c: // MOV.B    @(R0,Rm),Rn      0000nnnnmmmm1100
      case 0x0d: // MOV.W    @(R0,Rm),Rn      0000nnnnmmmm1101
      case 0x0e: // MOV.L    @(R0,Rm),Rn      0000nnnnmmmm1110
        opd->source = BITMASK3(GET_Rm(), SHR_R0, SHR_MEM);
        opd->dest = BITMASK1(GET_Rn());
        op_flags[i] |= OF_POLL_INSN;
        break;
      case 0x0f: // MAC.L   @Rm+,@Rn+  0000nnnnmmmm1111
        opd->source = BITMASK6(GET_Rm(), GET_Rn(), SHR_SR, SHR_MACL, SHR_MACH, SHR_MEM);
        opd->dest = BITMASK4(GET_Rm(), GET_Rn(), SHR_MACL, SHR_MACH);
        opd->cycles = 3;
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x01:
      // MOV.L Rm,@(disp,Rn) 0001nnnnmmmmdddd
      opd->source = BITMASK2(GET_Rm(), GET_Rn());
      opd->dest = BITMASK1(SHR_MEM);
      opd->imm = (op & 0x0f) * 4;
      break;

    /////////////////////////////////////////////
    case 0x02:
      switch (op & 0x0f)
      {
      case 0x00: // MOV.B Rm,@Rn        0010nnnnmmmm0000
      case 0x01: // MOV.W Rm,@Rn        0010nnnnmmmm0001
      case 0x02: // MOV.L Rm,@Rn        0010nnnnmmmm0010
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_MEM);
        break;
      case 0x04: // MOV.B Rm,@-Rn       0010nnnnmmmm0100
      case 0x05: // MOV.W Rm,@-Rn       0010nnnnmmmm0101
      case 0x06: // MOV.L Rm,@-Rn       0010nnnnmmmm0110
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK2(GET_Rn(), SHR_MEM);
        break;
      case 0x07: // DIV0S Rm,Rn         0010nnnnmmmm0111
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_SR);
        break;
      case 0x08: // TST Rm,Rn           0010nnnnmmmm1000
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_T);
        break;
      case 0x09: // AND Rm,Rn           0010nnnnmmmm1001
      case 0x0a: // XOR Rm,Rn           0010nnnnmmmm1010
      case 0x0b: // OR  Rm,Rn           0010nnnnmmmm1011
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x0c: // CMP/STR Rm,Rn       0010nnnnmmmm1100
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_T);
        break;
      case 0x0d: // XTRCT  Rm,Rn        0010nnnnmmmm1101
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x0e: // MULU.W Rm,Rn        0010nnnnmmmm1110
      case 0x0f: // MULS.W Rm,Rn        0010nnnnmmmm1111
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_MACL);
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x03:
      switch (op & 0x0f)
      {
      case 0x00: // CMP/EQ Rm,Rn        0011nnnnmmmm0000
      case 0x02: // CMP/HS Rm,Rn        0011nnnnmmmm0010
      case 0x03: // CMP/GE Rm,Rn        0011nnnnmmmm0011
      case 0x06: // CMP/HI Rm,Rn        0011nnnnmmmm0110
      case 0x07: // CMP/GT Rm,Rn        0011nnnnmmmm0111
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(SHR_T);
        break;
      case 0x04: // DIV1    Rm,Rn       0011nnnnmmmm0100
        opd->source = BITMASK3(GET_Rm(), GET_Rn(), SHR_SR);
        opd->dest = BITMASK2(GET_Rn(), SHR_SR);
        break;
      case 0x05: // DMULU.L Rm,Rn       0011nnnnmmmm0101
      case 0x0d: // DMULS.L Rm,Rn       0011nnnnmmmm1101
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK2(SHR_MACL, SHR_MACH);
        opd->cycles = 2;
        break;
      case 0x08: // SUB     Rm,Rn       0011nnnnmmmm1000
      case 0x0c: // ADD     Rm,Rn       0011nnnnmmmm1100
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x0a: // SUBC    Rm,Rn       0011nnnnmmmm1010
      case 0x0e: // ADDC    Rm,Rn       0011nnnnmmmm1110
        opd->source = BITMASK3(GET_Rm(), GET_Rn(), SHR_T);
        opd->dest = BITMASK2(GET_Rn(), SHR_T);
        break;
      case 0x0b: // SUBV    Rm,Rn       0011nnnnmmmm1011
      case 0x0f: // ADDV    Rm,Rn       0011nnnnmmmm1111
        opd->source = BITMASK2(GET_Rm(), GET_Rn());
        opd->dest = BITMASK2(GET_Rn(), SHR_T);
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x04:
      switch (op & 0x0f)
      {
      case 0x00:
        switch (GET_Fx())
        {
        case 0: // SHLL Rn    0100nnnn00000000
        case 2: // SHAL Rn    0100nnnn00100000
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK2(GET_Rn(), SHR_T);
          break;
        case 1: // DT Rn      0100nnnn00010000
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK2(GET_Rn(), SHR_T);
          op_flags[i] |= OF_DELAY_INSN;
          break;
        default:
          goto undefined;
        }
        break;
      case 0x01:
        switch (GET_Fx())
        {
        case 0: // SHLR Rn    0100nnnn00000001
        case 2: // SHAR Rn    0100nnnn00100001
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK2(GET_Rn(), SHR_T);
          break;
        case 1: // CMP/PZ Rn  0100nnnn00010001
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK1(SHR_T);
          break;
        default:
          goto undefined;
        }
        break;
      case 0x02:
      case 0x03:
        switch (op & 0x3f)
        {
        case 0x02: // STS.L    MACH,@-Rn 0100nnnn00000010
          tmp = SHR_MACH;
          break;
        case 0x12: // STS.L    MACL,@-Rn 0100nnnn00010010
          tmp = SHR_MACL;
          break;
        case 0x22: // STS.L    PR,@-Rn   0100nnnn00100010
          tmp = SHR_PR;
          break;
        case 0x03: // STC.L    SR,@-Rn   0100nnnn00000011
          tmp = SHR_SR;
          opd->cycles = 2;
          break;
        case 0x13: // STC.L    GBR,@-Rn  0100nnnn00010011
          tmp = SHR_GBR;
          opd->cycles = 2;
          break;
        case 0x23: // STC.L    VBR,@-Rn  0100nnnn00100011
          tmp = SHR_VBR;
          opd->cycles = 2;
          break;
        default:
          goto undefined;
        }
        opd->source = BITMASK2(GET_Rn(), tmp);
        opd->dest = BITMASK2(GET_Rn(), SHR_MEM);
        break;
      case 0x04:
      case 0x05:
        switch (op & 0x3f)
        {
        case 0x04: // ROTL   Rn          0100nnnn00000100
        case 0x05: // ROTR   Rn          0100nnnn00000101
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK2(GET_Rn(), SHR_T);
          break;
        case 0x24: // ROTCL  Rn          0100nnnn00100100
        case 0x25: // ROTCR  Rn          0100nnnn00100101
          opd->source = BITMASK2(GET_Rn(), SHR_T);
          opd->dest = BITMASK2(GET_Rn(), SHR_T);
          break;
        case 0x15: // CMP/PL Rn          0100nnnn00010101
          opd->source = BITMASK1(GET_Rn());
          opd->dest = BITMASK1(SHR_T);
          break;
        default:
          goto undefined;
        }
        break;
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
          opd->op = OP_LDC;
          opd->cycles = 3;
          break;
        case 0x17: // LDC.L @Rm+,GBR  0100mmmm00010111
          tmp = SHR_GBR;
          opd->op = OP_LDC;
          opd->cycles = 3;
          break;
        case 0x27: // LDC.L @Rm+,VBR  0100mmmm00100111
          tmp = SHR_VBR;
          opd->op = OP_LDC;
          opd->cycles = 3;
          break;
        default:
          goto undefined;
        }
        opd->source = BITMASK2(GET_Rn(), SHR_MEM);
        opd->dest = BITMASK2(GET_Rn(), tmp);
        break;
      case 0x08:
      case 0x09:
        switch (GET_Fx())
        {
        case 0:
          // SHLL2 Rn        0100nnnn00001000
          // SHLR2 Rn        0100nnnn00001001
          break;
        case 1:
          // SHLL8 Rn        0100nnnn00011000
          // SHLR8 Rn        0100nnnn00011001
          break;
        case 2:
          // SHLL16 Rn       0100nnnn00101000
          // SHLR16 Rn       0100nnnn00101001
          break;
        default:
          goto undefined;
        }
        opd->source = BITMASK1(GET_Rn());
        opd->dest = BITMASK1(GET_Rn());
        break;
      case 0x0a:
        switch (GET_Fx())
        {
        case 0: // LDS      Rm,MACH   0100mmmm00001010
          tmp = SHR_MACH;
          break;
        case 1: // LDS      Rm,MACL   0100mmmm00011010
          tmp = SHR_MACL;
          break;
        case 2: // LDS      Rm,PR     0100mmmm00101010
          tmp = SHR_PR;
          break;
        default:
          goto undefined;
        }
        opd->op = OP_MOVE;
        opd->source = BITMASK1(GET_Rn());
        opd->dest = BITMASK1(tmp);
        break;
      case 0x0b:
        switch (GET_Fx())
        {
        case 0: // JSR  @Rm   0100mmmm00001011
          opd->dest = BITMASK1(SHR_PR);
        case 2: // JMP  @Rm   0100mmmm00101011
          opd->op = OP_BRANCH_R;
          opd->rm = GET_Rn();
          opd->source = BITMASK1(opd->rm);
          opd->dest |= BITMASK1(SHR_PC);
          opd->cycles = 2;
          next_is_delay = 1;
          if (!(opd->dest & BITMASK1(SHR_PR)))
            end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
          else
            op_flags[i+1+next_is_delay] |= OF_BTARGET;
          break;
        case 1: // TAS.B @Rn  0100nnnn00011011
          opd->source = BITMASK2(GET_Rn(), SHR_MEM);
          opd->dest = BITMASK2(SHR_T, SHR_MEM);
          opd->cycles = 4;
          break;
        default:
          goto undefined;
        }
        break;
      case 0x0e:
        switch (GET_Fx())
        {
        case 0: // LDC Rm,SR   0100mmmm00001110
          tmp = SHR_SR;
          break;
        case 1: // LDC Rm,GBR  0100mmmm00011110
          tmp = SHR_GBR;
          break;
        case 2: // LDC Rm,VBR  0100mmmm00101110
          tmp = SHR_VBR;
          break;
        default:
          goto undefined;
        }
        opd->op = OP_LDC;
        opd->source = BITMASK1(GET_Rn());
        opd->dest = BITMASK1(tmp);
        break;
      case 0x0f:
        // MAC.W @Rm+,@Rn+  0100nnnnmmmm1111
        opd->source = BITMASK6(GET_Rm(), GET_Rn(), SHR_SR, SHR_MACL, SHR_MACH, SHR_MEM);
        opd->dest = BITMASK4(GET_Rm(), GET_Rn(), SHR_MACL, SHR_MACH);
        opd->cycles = 3;
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x05:
      // MOV.L @(disp,Rm),Rn 0101nnnnmmmmdddd
      opd->source = BITMASK2(GET_Rm(), SHR_MEM);
      opd->dest = BITMASK1(GET_Rn());
      opd->imm = (op & 0x0f) * 4;
      op_flags[i] |= OF_POLL_INSN;
      break;

    /////////////////////////////////////////////
    case 0x06:
      switch (op & 0x0f)
      {
      case 0x04: // MOV.B @Rm+,Rn       0110nnnnmmmm0100
      case 0x05: // MOV.W @Rm+,Rn       0110nnnnmmmm0101
      case 0x06: // MOV.L @Rm+,Rn       0110nnnnmmmm0110
        opd->dest = BITMASK2(GET_Rm(), GET_Rn());
        opd->source = BITMASK2(GET_Rm(), SHR_MEM);
        break;
      case 0x00: // MOV.B @Rm,Rn        0110nnnnmmmm0000
      case 0x01: // MOV.W @Rm,Rn        0110nnnnmmmm0001
      case 0x02: // MOV.L @Rm,Rn        0110nnnnmmmm0010
        opd->dest = BITMASK1(GET_Rn());
        opd->source = BITMASK2(GET_Rm(), SHR_MEM);
        op_flags[i] |= OF_POLL_INSN;
        break;
      case 0x0a: // NEGC   Rm,Rn        0110nnnnmmmm1010
        opd->source = BITMASK2(GET_Rm(), SHR_T);
        opd->dest = BITMASK2(GET_Rn(), SHR_T);
        break;
      case 0x03: // MOV    Rm,Rn        0110nnnnmmmm0011
        opd->op = OP_MOVE;
        goto arith_rmrn;
      case 0x07: // NOT    Rm,Rn        0110nnnnmmmm0111
      case 0x08: // SWAP.B Rm,Rn        0110nnnnmmmm1000
      case 0x09: // SWAP.W Rm,Rn        0110nnnnmmmm1001
      case 0x0b: // NEG    Rm,Rn        0110nnnnmmmm1011
      case 0x0c: // EXTU.B Rm,Rn        0110nnnnmmmm1100
      case 0x0d: // EXTU.W Rm,Rn        0110nnnnmmmm1101
      case 0x0e: // EXTS.B Rm,Rn        0110nnnnmmmm1110
      case 0x0f: // EXTS.W Rm,Rn        0110nnnnmmmm1111
      arith_rmrn:
        opd->source = BITMASK1(GET_Rm());
        opd->dest = BITMASK1(GET_Rn());
        break;
      }
      break;

    /////////////////////////////////////////////
    case 0x07:
      // ADD #imm,Rn  0111nnnniiiiiiii
      opd->source = opd->dest = BITMASK1(GET_Rn());
      opd->imm = (s8)op;
      break;

    /////////////////////////////////////////////
    case 0x08:
      switch (op & 0x0f00)
      {
      case 0x0000: // MOV.B R0,@(disp,Rn)  10000000nnnndddd
        opd->source = BITMASK2(GET_Rm(), SHR_R0);
        opd->dest = BITMASK1(SHR_MEM);
        opd->imm = (op & 0x0f);
        break;
      case 0x0100: // MOV.W R0,@(disp,Rn)  10000001nnnndddd
        opd->source = BITMASK2(GET_Rm(), SHR_R0);
        opd->dest = BITMASK1(SHR_MEM);
        opd->imm = (op & 0x0f) * 2;
        break;
      case 0x0400: // MOV.B @(disp,Rm),R0  10000100mmmmdddd
        opd->source = BITMASK2(GET_Rm(), SHR_MEM);
        opd->dest = BITMASK1(SHR_R0);
        opd->imm = (op & 0x0f);
        op_flags[i] |= OF_POLL_INSN;
        break;
      case 0x0500: // MOV.W @(disp,Rm),R0  10000101mmmmdddd
        opd->source = BITMASK2(GET_Rm(), SHR_MEM);
        opd->dest = BITMASK1(SHR_R0);
        opd->imm = (op & 0x0f) * 2;
        op_flags[i] |= OF_POLL_INSN;
        break;
      case 0x0800: // CMP/EQ #imm,R0       10001000iiiiiiii
        opd->source = BITMASK1(SHR_R0);
        opd->dest = BITMASK1(SHR_T);
        opd->imm = (s8)op;
        break;
      case 0x0d00: // BT/S label 10001101dddddddd
      case 0x0f00: // BF/S label 10001111dddddddd
        next_is_delay = 1;
        // fallthrough
      case 0x0900: // BT   label 10001001dddddddd
      case 0x0b00: // BF   label 10001011dddddddd
        opd->op = (op & 0x0200) ? OP_BRANCH_CF : OP_BRANCH_CT;
        opd->source = BITMASK2(SHR_PC, SHR_T);
        opd->dest = BITMASK1(SHR_PC);
        opd->imm = ((signed int)(op << 24) >> 23);
        opd->imm += pc + 4;
        if (base_pc <= opd->imm && opd->imm < base_pc + BLOCK_INSN_LIMIT * 2)
          op_flags[(opd->imm - base_pc) / 2] |= OF_BTARGET;
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x09:
      // MOV.W @(disp,PC),Rn  1001nnnndddddddd
      opd->op = OP_LOAD_POOL;
      tmp = pc + 2;
      if (op_flags[i] & OF_DELAY_OP) {
        if (ops[i-1].op == OP_BRANCH)
          tmp = ops[i-1].imm;
        else if (ops[i-1].op != OP_BRANCH_N)
          tmp = 0;
      }
      opd->source = BITMASK2(SHR_PC, SHR_MEM);
      opd->dest = BITMASK1(GET_Rn());
      if (tmp) {
        opd->imm = tmp + 2 + (op & 0xff) * 2;
        if (lowest_literal == 0 || opd->imm < lowest_literal)
          lowest_literal = opd->imm;
      }
      opd->size = 1;
      break;

    /////////////////////////////////////////////
    case 0x0b:
      // BSR  label 1011dddddddddddd
      opd->dest = BITMASK1(SHR_PR);
    case 0x0a:
      // BRA  label 1010dddddddddddd
      opd->op = OP_BRANCH;
      opd->source =  BITMASK1(SHR_PC);
      opd->dest |= BITMASK1(SHR_PC);
      opd->imm = ((signed int)(op << 20) >> 19);
      opd->imm += pc + 4;
      opd->cycles = 2;
      next_is_delay = 1;
      if (!(opd->dest & BITMASK1(SHR_PR))) {
        if (base_pc <= opd->imm && opd->imm < base_pc + BLOCK_INSN_LIMIT * 2) {
          op_flags[(opd->imm - base_pc) / 2] |= OF_BTARGET;
          if (opd->imm <= pc)
            end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
        } else
          end_block = !(op_flags[i+1+next_is_delay] & OF_BTARGET);
      } else
        op_flags[i+1+next_is_delay] |= OF_BTARGET;
      break;

    /////////////////////////////////////////////
    case 0x0c:
      switch (op & 0x0f00)
      {
      case 0x0000: // MOV.B R0,@(disp,GBR)   11000000dddddddd
      case 0x0100: // MOV.W R0,@(disp,GBR)   11000001dddddddd
      case 0x0200: // MOV.L R0,@(disp,GBR)   11000010dddddddd
        opd->source = BITMASK2(SHR_GBR, SHR_R0);
        opd->dest = BITMASK1(SHR_MEM);
        opd->size = (op & 0x300) >> 8;
        opd->imm = (op & 0xff) << opd->size;
        break;
      case 0x0400: // MOV.B @(disp,GBR),R0   11000100dddddddd
      case 0x0500: // MOV.W @(disp,GBR),R0   11000101dddddddd
      case 0x0600: // MOV.L @(disp,GBR),R0   11000110dddddddd
        opd->source = BITMASK2(SHR_GBR, SHR_MEM);
        opd->dest = BITMASK1(SHR_R0);
        opd->size = (op & 0x300) >> 8;
        opd->imm = (op & 0xff) << opd->size;
        op_flags[i] |= OF_POLL_INSN;
        break;
      case 0x0300: // TRAPA #imm      11000011iiiiiiii
        opd->op = OP_TRAPA;
        opd->source = BITMASK3(SHR_SP, SHR_PC, SHR_SR);
        opd->dest = BITMASK2(SHR_SP, SHR_PC);
        opd->imm = (op & 0xff);
        opd->cycles = 8;
        op_flags[i+1] |= OF_BTARGET;
        break;
      case 0x0700: // MOVA @(disp,PC),R0    11000111dddddddd
        opd->op = OP_MOVA;
        tmp = pc + 2;
        if (op_flags[i] & OF_DELAY_OP) {
          if (ops[i-1].op == OP_BRANCH)
            tmp = ops[i-1].imm;
          else if (ops[i-1].op != OP_BRANCH_N)
            tmp = 0;
        }
        opd->dest = BITMASK1(SHR_R0);
        if (tmp) {
          opd->imm = (tmp + 2 + (op & 0xff) * 4) & ~3;
          if (opd->imm >= base_pc) {
            if (lowest_mova == 0 || opd->imm < lowest_mova)
              lowest_mova = opd->imm;
          }
        }
        break;
      case 0x0800: // TST #imm,R0           11001000iiiiiiii
        opd->source = BITMASK1(SHR_R0);
        opd->dest = BITMASK1(SHR_T);
        opd->imm = op & 0xff;
        break;
      case 0x0900: // AND #imm,R0           11001001iiiiiiii
        opd->source = opd->dest = BITMASK1(SHR_R0);
        opd->imm = op & 0xff;
        break;
      case 0x0a00: // XOR #imm,R0           11001010iiiiiiii
        opd->source = opd->dest = BITMASK1(SHR_R0);
        opd->imm = op & 0xff;
        break;
      case 0x0b00: // OR  #imm,R0           11001011iiiiiiii
        opd->source = opd->dest = BITMASK1(SHR_R0);
        opd->imm = op & 0xff;
        break;
      case 0x0c00: // TST.B #imm,@(R0,GBR)  11001100iiiiiiii
        opd->source = BITMASK3(SHR_GBR, SHR_R0, SHR_MEM);
        opd->dest = BITMASK1(SHR_T);
        opd->imm = op & 0xff;
        op_flags[i] |= OF_POLL_INSN;
        opd->cycles = 3;
        break;
      case 0x0d00: // AND.B #imm,@(R0,GBR)  11001101iiiiiiii
      case 0x0e00: // XOR.B #imm,@(R0,GBR)  11001110iiiiiiii
      case 0x0f00: // OR.B  #imm,@(R0,GBR)  11001111iiiiiiii
        opd->source = BITMASK3(SHR_GBR, SHR_R0, SHR_MEM);
        opd->dest = BITMASK1(SHR_MEM);
        opd->imm = op & 0xff;
        opd->cycles = 3;
        break;
      default:
        goto undefined;
      }
      break;

    /////////////////////////////////////////////
    case 0x0d:
      // MOV.L @(disp,PC),Rn  1101nnnndddddddd
      opd->op = OP_LOAD_POOL;
      tmp = pc + 2;
      if (op_flags[i] & OF_DELAY_OP) {
        if (ops[i-1].op == OP_BRANCH)
          tmp = ops[i-1].imm;
        else if (ops[i-1].op != OP_BRANCH_N)
          tmp = 0;
      }
      opd->source = BITMASK2(SHR_PC, SHR_MEM);
      opd->dest = BITMASK1(GET_Rn());
      if (tmp) {
        opd->imm = (tmp + 2 + (op & 0xff) * 4) & ~3;
        if (lowest_literal == 0 || opd->imm < lowest_literal)
          lowest_literal = opd->imm;
      }
      opd->size = 2;
      break;

    /////////////////////////////////////////////
    case 0x0e:
      // MOV #imm,Rn   1110nnnniiiiiiii
      opd->op = OP_LOAD_CONST;
      opd->dest = BITMASK1(GET_Rn());
      opd->imm = (s8)op;
      break;

    default:
    undefined:
      opd->op = OP_UNDEFINED;
      // an unhandled instruction is probably not code if it's not the 1st insn
      if (!(op_flags[i] & OF_DELAY_OP) && pc != base_pc)
        goto end; 
      break;
    }

    if (op_flags[i] & OF_DELAY_OP) {
      switch (opd->op) {
      case OP_BRANCH:
      case OP_BRANCH_N:
      case OP_BRANCH_CT:
      case OP_BRANCH_CF:
      case OP_BRANCH_R:
      case OP_BRANCH_RF:
        elprintf(EL_ANOMALY, "%csh2 drc: branch in DS @ %08x",
          is_slave ? 's' : 'm', pc);
        opd->op = OP_UNDEFINED;
        op_flags[i] |= OF_B_IN_DS;
        next_is_delay = 0;
        break;
      }
    }
  }
end:
  i_end = i;
  end_pc = pc;

  // 2nd pass: some analysis
  lowest_literal = end_literals = lowest_mova = 0;
  t = T_UNKNOWN;
  last_btarget = 0;
  op = 0; // delay/poll insns counter
  for (i = 0, pc = base_pc; i < i_end; i++, pc += 2) {
    opd = &ops[i];
    crc += FETCH_OP(pc);

    // propagate T (TODO: DIV0U)
    if ((op_flags[i] & OF_BTARGET) || (opd->dest & BITMASK1(SHR_T)))
      t = T_UNKNOWN;

    if ((opd->op == OP_BRANCH_CT && t == T_SET) ||
        (opd->op == OP_BRANCH_CF && t == T_CLEAR)) {
      opd->op = OP_BRANCH;
      opd->cycles = (op_flags[i + 1] & OF_DELAY_OP) ? 2 : 3;
    } else if ((opd->op == OP_BRANCH_CT && t == T_CLEAR) ||
               (opd->op == OP_BRANCH_CF && t == T_SET))
      opd->op = OP_BRANCH_N;
    else if ((opd->op == OP_SETCLRT && !opd->imm) || opd->op == OP_BRANCH_CT)
      t = T_CLEAR;
    else if ((opd->op == OP_SETCLRT && opd->imm) || opd->op == OP_BRANCH_CF)
      t = T_SET;

    // "overscan" detection: unreachable code after unconditional branch
    // this can happen if the insn after a forward branch isn't a local target
    if (OP_ISBRAUC(opd->op)) {
      if (op_flags[i + 1] & OF_DELAY_OP) {
        if (i_end > i + 2 && !(op_flags[i + 2] & OF_BTARGET))
          i_end = i + 2;
      } else {
        if (i_end > i + 1 && !(op_flags[i + 1] & OF_BTARGET))
          i_end = i + 1;
      }
    }

    // literal pool size detection
    if (opd->op == OP_MOVA && opd->imm >= base_pc)
      if (lowest_mova == 0 || opd->imm < lowest_mova)
        lowest_mova = opd->imm;
    if (opd->op == OP_LOAD_POOL) {
      if (opd->imm >= base_pc && opd->imm < end_pc + MAX_LITERAL_OFFSET) {
        if (end_literals < opd->imm + opd->size * 2)
          end_literals = opd->imm + opd->size * 2;
        if (lowest_literal == 0 || lowest_literal > opd->imm)
          lowest_literal = opd->imm;
        if (opd->size == 2) {
          // tweak for NFL: treat a 32bit literal as an address and check if it
          // points to the literal space. In that case handle it like MOVA. 
          tmp = FETCH32(opd->imm) & ~0x20000000; // MUST ignore wt bit here
          if (tmp >= end_pc && tmp < end_pc + MAX_LITERAL_OFFSET)
            if (lowest_mova == 0 || tmp < lowest_mova)
              lowest_mova = tmp;
        }
      }
    }
#if LOOP_DETECTION
    // inner loop detection
    // 1. a loop always starts with a branch target (for the backwards jump)
    // 2. it doesn't contain more than one polling and/or delaying insn
    // 3. it doesn't contain unconditional jumps
    // 4. no overlapping of loops
    if (op_flags[i] & OF_BTARGET) {
      last_btarget = i;         // possible loop starting point
      op = 0;
    }
    // XXX let's hope nobody is putting a delay or poll insn in a delay slot :-/
    if (OP_ISBRAIMM(opd->op)) {
      // BSR, BRA, BT, BF with immediate target
      int i_tmp = (opd->imm - base_pc) / 2; // branch target, index in ops
      if (i_tmp == last_btarget && op <= 1) {
        op_flags[i_tmp] |= OF_LOOP; // conditions met -> mark loop
        last_btarget = i+1;     // condition 4
      } else if (opd->op == OP_BRANCH)
        last_btarget = i+1;     // condition 3
    }
    else if (OP_ISBRAIND(opd->op))
      // BRAF, BSRF, JMP, JSR, register indirect. treat it as off-limits jump
      last_btarget = i+1;       // condition 3
    else if (op_flags[i] & (OF_POLL_INSN|OF_DELAY_INSN))
      op ++;                    // condition 2 
#endif
  }
  end_pc = base_pc + i_end * 2;

  // end_literals is used to decide to inline a literal or not
  // XXX: need better detection if this actually is used in write
  if (lowest_literal >= base_pc) {
    if (lowest_literal < end_pc) {
      dbg(1, "warning: lowest_literal=%08x < end_pc=%08x", lowest_literal, end_pc);
      // TODO: does this always mean end_pc covers data?
    }
  }
  if (lowest_mova >= base_pc) {
    if (lowest_mova < end_literals) {
      dbg(1, "warning: mova=%08x < end_literals=%08x", lowest_mova, end_literals);
      end_literals = lowest_mova;
    }
    if (lowest_mova < end_pc) {
      dbg(1, "warning: mova=%08x < end_pc=%08x", lowest_mova, end_pc);
      end_literals = end_pc;
    }
  }
  if (lowest_literal >= end_literals)
    lowest_literal = end_literals;

  if (lowest_literal && end_literals)
    for (pc = lowest_literal; pc < end_literals; pc += 2)
      crc += FETCH_OP(pc);

  *end_pc_out = end_pc;
  if (base_literals_out != NULL)
    *base_literals_out = (lowest_literal ?: end_pc);
  if (end_literals_out != NULL)
    *end_literals_out = (end_literals ?: end_pc);

  // crc overflow handling, twice to collect all overflows
  crc = (crc & 0xffff) + (crc >> 16);
  crc = (crc & 0xffff) + (crc >> 16);
  return crc;
}

// vim:shiftwidth=2:ts=2:expandtab
