// This is part of Pico Library

// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "../PicoInt.h"

// ym2612
#include "../sound/ym2612.h"

// sn76496
extern int *sn76496_regs;


typedef enum {
	CHUNK_M68K = 1,
	CHUNK_RAM,
	CHUNK_VRAM,
	CHUNK_ZRAM,
	CHUNK_CRAM,
	CHUNK_VSRAM,
	CHUNK_MISC,
	CHUNK_VIDEO,
	CHUNK_Z80,
	CHUNK_PSG,
	CHUNK_FM,
	// CD stuff
} chunk_name_e;


static int write_chunk(chunk_name_e name, int len, void *data, void *file)
{
	size_t bwritten = 0;
	bwritten += areaWrite(&name, 1, 1, file);
	bwritten += areaWrite(&len, 1, 4, file);
	bwritten += areaWrite(data, 1, len, file);

	return (bwritten == len + 4 + 1);
}


#define CHECKED_WRITE(name,len,data) \
	if (!write_chunk(name, len, data, file)) return 1;

#define CHECKED_WRITE_BUFF(name,buff) \
	if (!write_chunk(name, sizeof(buff), &buff, file)) return 1;

int PicoCdSaveState(void *file)
{
	unsigned char buff[0x60];
	void *ym2612_regs = YM2612GetRegs();

	areaWrite("PicoSMCD", 1, 8, file);
	areaWrite(&PicoVer, 1, 4, file);

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
	if(PicoOpt&7) {
		memset(buff, 0, sizeof(buff));
		z80_pack(buff);
		CHECKED_WRITE_BUFF(CHUNK_Z80, buff);
	}
	if(PicoOpt&3)
		CHECKED_WRITE(CHUNK_PSG, 28*4, sn76496_regs);
	if(PicoOpt&1)
		CHECKED_WRITE(CHUNK_FM, 0x200+4, ym2612_regs);

	// TODO: cd stuff

	return 0;
}

static int g_read_offs = 0;

#define CHECKED_READ(len,data) \
	if (areaRead(data, 1, len, file) != len) { \
		g_read_offs += len; \
		printf("areaRead: premature EOF\n"); \
		return 0; \
	}

#define R_ERROR_RETURN(error) \
{ \
	printf("PicoCdLoadState @ %x: " error "\n", g_read_offs); \
	return 1; \
}

#define CHECKED_READ2(len2,data) \
	if (len2 != len) R_ERROR_RETURN("unexpected len, wanted " #len2); \
	CHECKED_READ(len2, data)

#define CHECKED_READ_BUFF(buff) CHECKED_READ2(sizeof(buff), &buff);

int PicoCdLoadState(void *file)
{
	unsigned char buff[0x60];
	int ver, len;
	void *ym2612_regs = YM2612GetRegs();

	g_read_offs = 0;
	CHECKED_READ(8, buff);
	if (strncmp((char *)buff, "PicoSMCD", 8)) R_ERROR_RETURN("bad header");
	CHECKED_READ(4, &ver);

	while (!areaEof(file))
	{
		CHECKED_READ(1, buff);
		CHECKED_READ(4, &len);
		if (len < 0 || len > 1024*256) R_ERROR_RETURN("bad length");

		switch (buff[0])
		{
			case CHUNK_M68K:
				CHECKED_READ_BUFF(buff);
				PicoAreaUnpackCpu(buff, 0);
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
				YM2612PicoStateLoad();
				break;
		}
	}

	return 0;
}

