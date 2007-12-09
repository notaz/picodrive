
// -------------------- Pico Library --------------------

// Pico Library - Header File

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2007 notaz, All rights reserved.
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
int  mp3_get_offset(void); // 0-1023
void mp3_update(int *buffer, int length, int stereo);


// Pico.c
// PicoOpt bits LSb->MSb:
// enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound,
// alt_renderer, 6button_gamepad, accurate_timing, accurate_sprites,
// draw_no_32col_border, external_ym2612, enable_cd_pcm, enable_cd_cdda
// enable_cd_gfx, cd_perfect_sync, soft_32col_scaling, enable_cd_ramcart
// disable_vdp_fifo
extern int PicoOpt;
extern int PicoVer;
extern int PicoSkipFrame; // skip rendering frame, but still do sound (if enabled) and emulation stuff
extern int PicoRegionOverride; // override the region detection 0: auto, 1: Japan NTSC, 2: Japan PAL, 4: US, 8: Europe
extern int PicoAutoRgnOrder; // packed priority list of regions, for example 0x148 means this detection order: EUR, USA, JAP
int PicoInit(void);
void PicoExit(void);
int PicoReset(int hard);
int PicoFrame(void);
void PicoFrameDrawOnly(void);
extern int PicoPad[2]; // Joypads, format is MXYZ SACB RLDU
extern void (*PicoWriteSound)(int len); // called once per frame at the best time to send sound buffer (PsndOut) to hardware
extern void (*PicoMessage)(const char *msg); // callback to output text message from emu

// cd/Pico.c
extern void (*PicoMCDopenTray)(void);
extern int  (*PicoMCDcloseTray)(void);
extern int PicoCDBuffers;

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

// cd/cd_sys.c
int Insert_CD(char *iso_name, int is_bin);
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
int PicoUnloadCart(unsigned char* romdata);
extern void (*PicoCartLoadProgressCB)(int percent);
extern void (*PicoCDLoadProgressCB)(int percent);

// Draw.c
void PicoDrawSetColorFormat(int which); // 0=BGR444, 1=RGB555, 2=8bit(HighPal pal)
extern void *DrawLineDest;
#if OVERRIDE_HIGHCOL
extern unsigned char *HighCol;
#endif
extern int (*PicoScan)(unsigned int num, void *data);
// internals
extern unsigned short HighPal[0x100];
extern int rendstatus;
// utility
#ifdef _ASM_DRAW_C
void *blockcpy(void *dst, const void *src, size_t n);
void vidConvCpyRGB565(void *to, void *from, int pixels);
#else
#define blockcpy memcpy
#endif

// Draw2.c
// stuff below is optional
extern unsigned char  *PicoDraw2FB;  // buffer for fasr renderer in format (8+320)x(8+224+8) (eights for borders)
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
