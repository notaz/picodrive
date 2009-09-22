// Savestate handling for emulated Sega/Mega CD machine.
// (c) Copyright 2007, Grazvydas "notaz" Ignotas


#include "../pico_int.h"

// ym2612
#include "../sound/ym2612.h"

// sn76496
extern int *sn76496_regs;

carthw_state_chunk *carthw_chunks;
void (*PicoStateProgressCB)(const char *str) = 0;


typedef enum {
	CHUNK_M68K = 1,
	CHUNK_RAM,
	CHUNK_VRAM,
	CHUNK_ZRAM,
	CHUNK_CRAM,	// 5
	CHUNK_VSRAM,
	CHUNK_MISC,
	CHUNK_VIDEO,
	CHUNK_Z80,
	CHUNK_PSG,	// 10
	CHUNK_FM,
	// CD stuff
	CHUNK_S68K,
	CHUNK_PRG_RAM,
	CHUNK_WORD_RAM,
	CHUNK_PCM_RAM,	// 15
	CHUNK_BRAM,
	CHUNK_GA_REGS,
	CHUNK_PCM,
	CHUNK_CDC,
	CHUNK_CDD,	// 20
	CHUNK_SCD,
	CHUNK_RC,
	CHUNK_MISC_CD,
	CHUNK_DEFAULT_COUNT
	// CHUNK_CARTHW = 64,	// defined in PicoInt
} chunk_name_e;


static char *chunk_names[] = {
	"INVALID!",
	"Saving.. M68K state",
	"Saving.. RAM",
	"Saving.. VRAM",
	"Saving.. ZRAM",
	"Saving.. CRAM",	// 5
	"Saving.. VSRAM",
	"Saving.. emu state",
	"Saving.. VIDEO",
	"Saving.. Z80 state",
	"Saving.. PSG",		// 10
	"Saving.. FM",
	// CD stuff
	"Saving.. S68K state",
	"Saving.. PRG_RAM",
	"Saving.. WORD_RAM",
	"Saving.. PCM_RAM",	// 15
	"Saving.. BRAM",
	"Saving.. GATE ARRAY regs",
	"Saving.. PCM state",
	"Saving.. CDC",
	"Saving.. CDD",		// 20
	"Saving.. SCD",
	"Saving.. GFX chip",
	"Saving.. MCD state",
};


static int write_chunk(chunk_name_e name, int len, void *data, void *file)
{
	size_t bwritten = 0;
	bwritten += areaWrite(&name, 1, 1, file);
	bwritten += areaWrite(&len, 1, 4, file);
	bwritten += areaWrite(data, 1, len, file);

	return (bwritten == len + 4 + 1);
}


#define CHECKED_WRITE(name,len,data) { \
	if (PicoStateProgressCB && name < CHUNK_DEFAULT_COUNT) \
		PicoStateProgressCB(chunk_names[name]); \
	if (!write_chunk(name, len, data, file)) return 1; \
}

#define CHECKED_WRITE_BUFF(name,buff) { \
	if (PicoStateProgressCB && name < CHUNK_DEFAULT_COUNT) \
		PicoStateProgressCB(chunk_names[name]); \
	if (!write_chunk(name, sizeof(buff), &buff, file)) return 1; \
}

PICO_INTERNAL int PicoCdSaveState(void *file)
{
	unsigned char buff[0x60];
	void *ym2612_regs = YM2612GetRegs();
	int ver = 0x0133; // not really used..

	areaWrite("PicoSEXT", 1, 8, file);
	areaWrite(&ver, 1, 4, file);

	memset(buff, 0, sizeof(buff));
	PicoAreaPackCpu(buff, 0);
	CHECKED_WRITE_BUFF(CHUNK_M68K,  buff);
	CHECKED_WRITE_BUFF(CHUNK_RAM,   Pico.ram);
	CHECKED_WRITE_BUFF(CHUNK_VRAM,  Pico.vram);
	CHECKED_WRITE_BUFF(CHUNK_ZRAM,  Pico.zram);
	CHECKED_WRITE_BUFF(CHUNK_CRAM,  Pico.cram);
	CHECKED_WRITE_BUFF(CHUNK_VSRAM, Pico.vsram);
	CHECKED_WRITE_BUFF(CHUNK_MISC,  Pico.m);
	CHECKED_WRITE_BUFF(CHUNK_VIDEO, Pico.video);

	memset(buff, 0, sizeof(buff));
	z80_pack(buff);
	CHECKED_WRITE_BUFF(CHUNK_Z80, buff);
	CHECKED_WRITE(CHUNK_PSG, 28*4, sn76496_regs);
	ym2612_pack_state();
	CHECKED_WRITE(CHUNK_FM, 0x200+4, ym2612_regs);

	if (PicoAHW & PAHW_MCD)
	{
		memset(buff, 0, sizeof(buff));
		PicoAreaPackCpu(buff, 1);
		if (Pico_mcd->s68k_regs[3]&4) // 1M mode?
			wram_1M_to_2M(Pico_mcd->word_ram2M);
        	Pico_mcd->m.hint_vector = *(unsigned short *)(Pico_mcd->bios + 0x72);

		CHECKED_WRITE_BUFF(CHUNK_S68K,     buff);
		CHECKED_WRITE_BUFF(CHUNK_PRG_RAM,  Pico_mcd->prg_ram);
		CHECKED_WRITE_BUFF(CHUNK_WORD_RAM, Pico_mcd->word_ram2M); // in 2M format
		CHECKED_WRITE_BUFF(CHUNK_PCM_RAM,  Pico_mcd->pcm_ram);
		CHECKED_WRITE_BUFF(CHUNK_BRAM,     Pico_mcd->bram);
		CHECKED_WRITE_BUFF(CHUNK_GA_REGS,  Pico_mcd->s68k_regs); // GA regs, not CPU regs
		CHECKED_WRITE_BUFF(CHUNK_PCM,      Pico_mcd->pcm);
		CHECKED_WRITE_BUFF(CHUNK_CDD,      Pico_mcd->cdd);
		CHECKED_WRITE_BUFF(CHUNK_CDC,      Pico_mcd->cdc);
		CHECKED_WRITE_BUFF(CHUNK_SCD,      Pico_mcd->scd);
		CHECKED_WRITE_BUFF(CHUNK_RC,       Pico_mcd->rot_comp);
		CHECKED_WRITE_BUFF(CHUNK_MISC_CD,  Pico_mcd->m);

		if (Pico_mcd->s68k_regs[3]&4) // convert back
			wram_2M_to_1M(Pico_mcd->word_ram2M);
	}

	if (carthw_chunks != NULL)
	{
		carthw_state_chunk *chwc;
		if (PicoStateProgressCB)
			PicoStateProgressCB("Saving.. cart hw state");
		for (chwc = carthw_chunks; chwc->ptr != NULL; chwc++)
			CHECKED_WRITE(chwc->chunk, chwc->size, chwc->ptr);
	}

	return 0;
}

static int g_read_offs = 0;

#define R_ERROR_RETURN(error) \
{ \
	elprintf(EL_STATUS, "PicoCdLoadState @ %x: " error, g_read_offs); \
	return 1; \
}

// when is eof really set?
#define CHECKED_READ(len,data) \
	if (areaRead(data, 1, len, file) != len) { \
		if (len == 1 && areaEof(file)) goto readend; \
		R_ERROR_RETURN("areaRead: premature EOF\n"); \
		return 1; \
	} \
	g_read_offs += len;

#define CHECKED_READ2(len2,data) \
	if (len2 != len) { \
		elprintf(EL_STATUS, "unexpected len %i, wanted %i (%s)", len, len2, #len2); \
		if (len > len2) R_ERROR_RETURN("failed."); \
		/* else read anyway and hope for the best.. */ \
	} \
	CHECKED_READ(len, data)

#define CHECKED_READ_BUFF(buff) CHECKED_READ2(sizeof(buff), &buff);

PICO_INTERNAL int PicoCdLoadState(void *file)
{
	unsigned char buff[0x60], buff_m68k[0x60], buff_s68k[0x60];
	int ver, len;
	void *ym2612_regs = YM2612GetRegs();

	g_read_offs = 0;
	CHECKED_READ(8, buff);
	if (strncmp((char *)buff, "PicoSMCD", 8) && strncmp((char *)buff, "PicoSEXT", 8))
		R_ERROR_RETURN("bad header");
	CHECKED_READ(4, &ver);

	while (!areaEof(file))
	{
		CHECKED_READ(1, buff);
		CHECKED_READ(4, &len);
		if (len < 0 || len > 1024*512) R_ERROR_RETURN("bad length");
		if (buff[0] > CHUNK_FM && buff[0] <= CHUNK_MISC_CD && !(PicoAHW & PAHW_MCD))
			R_ERROR_RETURN("cd chunk in non CD state?");

		switch (buff[0])
		{
			case CHUNK_M68K:
				CHECKED_READ_BUFF(buff_m68k);
				break;

			case CHUNK_Z80:
				CHECKED_READ_BUFF(buff);
				z80_unpack(buff);
				break;

			case CHUNK_RAM:   CHECKED_READ_BUFF(Pico.ram); break;
			case CHUNK_VRAM:  CHECKED_READ_BUFF(Pico.vram); break;
			case CHUNK_ZRAM:  CHECKED_READ_BUFF(Pico.zram); break;
			case CHUNK_CRAM:  CHECKED_READ_BUFF(Pico.cram); break;
			case CHUNK_VSRAM: CHECKED_READ_BUFF(Pico.vsram); break;
			case CHUNK_MISC:  CHECKED_READ_BUFF(Pico.m); break;
			case CHUNK_VIDEO: CHECKED_READ_BUFF(Pico.video); break;
			case CHUNK_PSG:   CHECKED_READ2(28*4, sn76496_regs); break;
			case CHUNK_FM:
				CHECKED_READ2(0x200+4, ym2612_regs);
				ym2612_unpack_state();
				break;

			// cd stuff
			case CHUNK_S68K:
				CHECKED_READ_BUFF(buff_s68k);
				break;

			case CHUNK_PRG_RAM:	CHECKED_READ_BUFF(Pico_mcd->prg_ram); break;
			case CHUNK_WORD_RAM:	CHECKED_READ_BUFF(Pico_mcd->word_ram2M); break;
			case CHUNK_PCM_RAM:	CHECKED_READ_BUFF(Pico_mcd->pcm_ram); break;
			case CHUNK_BRAM:	CHECKED_READ_BUFF(Pico_mcd->bram); break;
			case CHUNK_GA_REGS:	CHECKED_READ_BUFF(Pico_mcd->s68k_regs); break;
			case CHUNK_PCM:		CHECKED_READ_BUFF(Pico_mcd->pcm); break;
			case CHUNK_CDD:		CHECKED_READ_BUFF(Pico_mcd->cdd); break;
			case CHUNK_CDC:		CHECKED_READ_BUFF(Pico_mcd->cdc); break;
			case CHUNK_SCD:		CHECKED_READ_BUFF(Pico_mcd->scd); break;
			case CHUNK_RC:		CHECKED_READ_BUFF(Pico_mcd->rot_comp); break;
			case CHUNK_MISC_CD:	CHECKED_READ_BUFF(Pico_mcd->m); break;

			default:
				if (carthw_chunks != NULL)
				{
					carthw_state_chunk *chwc;
					for (chwc = carthw_chunks; chwc->ptr != NULL; chwc++) {
						if (chwc->chunk == buff[0]) {
							CHECKED_READ2(chwc->size, chwc->ptr);
							goto breakswitch;
						}
					}
				}
				elprintf(EL_STATUS, "PicoCdLoadState: skipping unknown chunk %i of size %i", buff[0], len);
				areaSeek(file, len, SEEK_CUR);
				break;
		}
		breakswitch:;
	}

readend:
	if (PicoAHW & PAHW_MCD)
	{
		PicoMemStateLoaded();

		if (!(Pico_mcd->s68k_regs[0x36] & 1) && (Pico_mcd->scd.Status_CDC & 1))
			cdda_start_play();

		// must unpack after other CD stuff is loaded
		PicoAreaUnpackCpu(buff_s68k, 1);
	}
	PicoAreaUnpackCpu(buff_m68k, 0);

	return 0;
}


int PicoCdLoadStateGfx(void *file)
{
	int ver, len, found = 0;
	char buff[8];

	g_read_offs = 0;
	CHECKED_READ(8, buff);
	if (strncmp((char *)buff, "PicoSMCD", 8) && strncmp((char *)buff, "PicoSEXT", 8))
		R_ERROR_RETURN("bad header");
	CHECKED_READ(4, &ver);

	while (!areaEof(file) && found < 4)
	{
		CHECKED_READ(1, buff);
		CHECKED_READ(4, &len);
		if (len < 0 || len > 1024*512) R_ERROR_RETURN("bad length");
		if (buff[0] > CHUNK_FM && buff[0] <= CHUNK_MISC_CD && !(PicoAHW & PAHW_MCD))
			R_ERROR_RETURN("cd chunk in non CD state?");

		switch (buff[0])
		{
			case CHUNK_VRAM:  CHECKED_READ_BUFF(Pico.vram);  found++; break;
			case CHUNK_CRAM:  CHECKED_READ_BUFF(Pico.cram);  found++; break;
			case CHUNK_VSRAM: CHECKED_READ_BUFF(Pico.vsram); found++; break;
			case CHUNK_VIDEO: CHECKED_READ_BUFF(Pico.video); found++; break;
			default:
				areaSeek(file, len, SEEK_CUR);
				break;
		}
	}

readend:
	return 0;
}


