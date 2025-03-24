/* PicoDrive's wrapper for emu2413
 */

#include "emu2413/emu2413.c"

// the one instance that can be in a Mark III
OPLL *opll = NULL;


void YM2413_regWrite(unsigned data){
  OPLL_writeIO(opll,0,data);
}

void YM2413_dataWrite(unsigned data){
  OPLL_writeIO(opll,1,data);
}


// state saving/loading - old save states only have adr and reg saved, new
// saves have necessary internal state data as well. Most of the state is
// recovered from the registers, which keeps the internal state data smaller.

#include "../state.h"

#define SLOT_SIZE_MIN 12
#define OTHER_SIZE_MIN 32

static size_t save_slot(u8 *buf, const OPLL_SLOT *slot)
{
	size_t b = 0;

	b++; // length, assumes slot state won't grow beyond 255
	save_u32(buf, &b, slot->pg_phase);
	save_u8_(buf, &b, slot->key_flag);
	save_u8_(buf, &b, slot->eg_state);
	save_u8_(buf, &b, slot->eg_out);
	save_s16(buf, &b, slot->output[0]);
	save_s16(buf, &b, slot->output[1]);

	//printf("slot size: %zd\n", b);
	assert(b >= SLOT_SIZE_MIN);
	assert(b < 256u);
	buf[0] = b - 1;
	return b;
}

static void load_slot(const u8 *buf, OPLL_SLOT *slot)
{
	size_t b = 0;

	slot->pg_phase  = load_u32(buf, &b);
	slot->key_flag  = load_u8_(buf, &b);
	slot->eg_state  = load_u8_(buf, &b);
	slot->eg_out    = load_u8_(buf, &b);
	slot->output[0] = load_s16(buf, &b);
	slot->output[1] = load_s16(buf, &b);

	slot->pg_out = slot->pg_phase >> DP_BASE_BITS;
}

size_t ym2413_pack_state(void *buf_, size_t size)
{
	size_t i, b = 0;
	u8 *buf = buf_;

	// regs and adr first, for backwards compatibility
	save_u32(buf, &b, opll->adr);
	for (i = 0; i < 0x40; i++)
		save_u8_(buf, &b, opll->reg[i]);

	// user patches only, all others are read-only anyway
	for (i = 0; i < 18; i++)
		b += save_slot(&buf[b], &opll->slot[i]);

	for (i = 0; i < 9; i++)
		save_u8_(buf, &b, opll->patch_number[i]);
	save_u32(buf, &b, opll->slot_key_status);
	save_u32(buf, &b, opll->eg_counter);
	save_u8_(buf, &b, opll->lfo_am);
	save_u32(buf, &b, opll->pm_phase);
	save_s32(buf, &b, opll->am_phase);
	save_u32(buf, &b, opll->noise);
	save_u16(buf, &b, opll->short_noise);

	printf("ym2413 state size: %zu\n", b);
	assert(b <= size);
	return b;
}

void ym2413_unpack_state(const void *buf_, size_t size)
{
	const u8 *buf = buf_;
	size_t i, b = 0;

	// registers, write to opll too to take over to internal data structures
	opll->adr = load_u32(buf, &b);
	for (i = 0; i < 0x40; i++) {
		opll->reg[i] = load_u8_(buf, &b);
		// skip the shadow registers
		if ((i & 0xf) < 9 || (i & 0x30) == 0)
			OPLL_writeReg(opll, i, opll->reg[i]);
	}

	if (b >= size) return; // old save

	for (i = 0; i < 18; i++) {
		u8 sz = load_u8_(buf, &b);
		load_slot(&buf[b], &opll->slot[i]);
		b += sz;
	}

	for (i = 0; i < 9; i++)
		opll->patch_number[i] = load_u8_(buf, &b);
	opll->slot_key_status = load_u32(buf, &b);
	opll->eg_counter =  load_u32(buf, &b);
	opll->lfo_am =      load_u8_(buf, &b);
	opll->pm_phase =    load_u32(buf, &b);
	opll->am_phase =    load_s32(buf, &b);
	opll->noise =       load_u32(buf, &b);
	opll->short_noise = load_u16(buf, &b);

	OPLL_forceRefresh(opll);
}

