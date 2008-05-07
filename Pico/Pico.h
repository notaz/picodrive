
// -------------------- Pico Library --------------------

// Pico Library - Header File

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2008 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.

#ifndef PICO_H
#define PICO_H

#include <stdio.h>

// port-specific compile-time settings
#include <port_config.h>

#ifdef __cplusplus
extern "C" {
#endif

// external funcs for Sega/Mega CD
int  mp3_get_bitrate(FILE *f, int size);
void mp3_start_play(FILE *f, int pos);
void mp3_update(int *buffer, int length, int stereo);


// Pico.c
#define POPT_EN_FM          (1<< 0) // 00 000x
#define POPT_EN_PSG         (1<< 1)
#define POPT_EN_Z80         (1<< 2)
#define POPT_EN_STEREO      (1<< 3)
#define POPT_ALT_RENDERER   (1<< 4) // 00 00x0
#define POPT_6BTN_PAD       (1<< 5)
#define POPT_ACC_TIMING     (1<< 6)
#define POPT_ACC_SPRITES    (1<< 7)
#define POPT_DIS_32C_BORDER (1<< 8) // 00 0x00
#define POPT_EXT_FM         (1<< 9)
#define POPT_EN_MCD_PCM     (1<<10)
#define POPT_EN_MCD_CDDA    (1<<11)
#define POPT_EN_MCD_GFX     (1<<12) // 00 x000
#define POPT_EN_MCD_PSYNC   (1<<13)
#define POPT_EN_SOFTSCALE   (1<<14)
#define POPT_EN_MCD_RAMCART (1<<15)
#define POPT_DIS_VDP_FIFO   (1<<16) // 0x 0000
#define POPT_EN_SVP_DRC     (1<<17)
#define POPT_DIS_SPRITE_LIM (1<<18)
extern int PicoOpt; // bitfield
extern int PicoVer;
extern int PicoSkipFrame; // skip rendering frame, but still do sound (if enabled) and emulation stuff
extern int PicoRegionOverride; // override the region detection 0: auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
extern int PicoAutoRgnOrder; // packed priority list of regions, for example 0x148 means this detection order: EUR, USA, JAP
extern int PicoSVPCycles;
int  PicoInit(void);
void PicoExit(void);
void PicoPower(void);
int  PicoReset(void);
int  PicoFrame(void);
void PicoFrameDrawOnly(void);
extern int PicoPad[2]; // Joypads, format is MXYZ SACB RLDU
extern void (*PicoWriteSound)(int len); // called once per frame at the best time to send sound buffer (PsndOut) to hardware
extern void (*PicoMessage)(const char *msg); // callback to output text message from emu
typedef enum { PS_PAL, PS_40_CELL, PS_240_LINES } pstat_t;
int  PicoGetStat(pstat_t which);

// cd/Pico.c
extern void (*PicoMCDopenTray)(void);
extern int  (*PicoMCDcloseTray)(void);
extern int PicoCDBuffers;

// Pico/Pico.c
typedef struct
{
	int pen_pos[2];
	int page;
	// internal
	int fifo_bytes;
	int line_counter;
	unsigned int r1, r12;
} picohw_state;
extern picohw_state PicoPicohw;

// Area.c
typedef size_t (arearw)(void *p, size_t _size, size_t _n, void *file);
typedef size_t (areaeof)(void *file);
typedef int    (areaseek)(void *file, long offset, int whence);
typedef int    (areaclose)(void *file);
// Save or load the state from PmovFile:
int PmovState(int PmovAction, void *PmovFile); // &1=for reading &2=for writing &4=volatile &8=non-volatile
extern arearw  *areaRead;  // external read and write function pointers for
extern arearw  *areaWrite; // gzip save state ability
extern areaeof *areaEof;
extern areaseek *areaSeek;
extern areaclose *areaClose;
extern void (*PicoStateProgressCB)(const char *str);

// cd/Area.c
int PicoCdLoadStateGfx(void *file);

// cd/buffering.c
void PicoCDBufferInit(void);
void PicoCDBufferFree(void);
void PicoCDBufferFlush(void);

// cd/cd_sys.c
int Insert_CD(char *iso_name, int is_bin);
int Insert_CD(char *cdimg_name, int type);
void Stop_CD(void); // releases all resources taken when CD game was started.

// Cart.c
typedef enum
{
	PMT_UNCOMPRESSED = 0,
	PMT_ZIP,
	PMT_CSO
} pm_type;
typedef struct
{
	void *file;		/* file handle */
	void *param;		/* additional file related field */
	unsigned int size;	/* size */
	pm_type type;
} pm_file;
pm_file *pm_open(const char *path);
size_t   pm_read(void *ptr, size_t bytes, pm_file *stream);
int      pm_seek(pm_file *stream, long offset, int whence);
int      pm_close(pm_file *fp);
int PicoCartLoad(pm_file *f,unsigned char **prom,unsigned int *psize);
int PicoCartInsert(unsigned char *rom,unsigned int romsize);
void Byteswap(unsigned char *data,int len);
int PicoCartUnload(void);
extern void (*PicoCartLoadProgressCB)(int percent);
extern void (*PicoCDLoadProgressCB)(int percent);

// Draw.c
void PicoDrawSetColorFormat(int which); // 0=BGR444, 1=RGB555, 2=8bit(HighPal pal)
extern void *DrawLineDest;
#if OVERRIDE_HIGHCOL
extern unsigned char *HighCol;
#endif
extern int (*PicoScanBegin)(unsigned int num);
extern int (*PicoScanEnd)(unsigned int num);
// utility
#ifdef _ASM_DRAW_C
void vidConvCpyRGB565(void *to, void *from, int pixels);
#endif
// internals
#define PDRAW_SPRITES_MOVED (1<<0)
#define PDRAW_WND_DIFF_PRIO (1<<1) // not all window tiles use same priority
#define PDRAW_ACC_SPRITES   (1<<2) // accurate sprites (copied from PicoOpt)
#define PDRAW_INTERLACE     (1<<3) //
#define PDRAW_DIRTY_SPRITES (1<<4)
#define PDRAW_SONIC_MODE    (1<<5) // mid-frame palette changes for 8bit renderer
#define PDRAW_PLANE_HI_PRIO (1<<6) // have layer with all hi prio tiles (mk3)
#define PDRAW_SHHI_DONE     (1<<7) // layer sh/hi already processed
#define PDRAW_EARLY_BLANK   (1<<8) // blanking enabled at the start of prev line
extern int rendstatus;
extern unsigned short HighPal[0x100];

// Draw2.c
// stuff below is optional
extern unsigned char  *PicoDraw2FB;  // buffer for fast renderer in format (8+320)x(8+224+8) (eights for borders)
extern unsigned short *PicoCramHigh; // pointer to CRAM buff (0x40 shorts), converted to native device color (works only with 16bit for now)
extern void (*PicoPrepareCram)();    // prepares PicoCramHigh for renderer to use

// sound.c
extern int PsndRate,PsndLen;
extern short *PsndOut;
extern void (*PsndMix_32_to_16l)(short *dest, int *src, int count);
void PsndRerate(int preserve_state);

// Utils.c
extern int PicuAnd;
int PicuQuick(unsigned short *dest,unsigned short *src);
int PicuShrink(unsigned short *dest,int destLen,unsigned short *src,int srcLen);
int PicuShrinkReverse(unsigned short *dest,int destLen,unsigned short *src,int srcLen);
int PicuMerge(unsigned short *dest,int destLen,unsigned short *src,int srcLen);

#ifdef __cplusplus
} // End of extern "C"
#endif

#endif // PICO_H
