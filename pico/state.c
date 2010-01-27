// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2010 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "pico_int.h"
#include <zlib/zlib.h>

#include "../cpu/sh2/sh2.h"
#include "sound/ym2612.h"

// sn76496
extern int *sn76496_regs;

typedef size_t (arearw)(void *p, size_t _size, size_t _n, void *file);
typedef size_t (areaeof)(void *file);
typedef int    (areaseek)(void *file, long offset, int whence);
typedef int    (areaclose)(void *file);

static arearw    *areaRead;
static arearw    *areaWrite;
static areaeof   *areaEof;
static areaseek  *areaSeek;
static areaclose *areaClose;

carthw_state_chunk *carthw_chunks;
void (*PicoStateProgressCB)(const char *str);
void (*PicoLoadStateHook)(void);


/* I/O functions */
static size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
  return gzread(file, p, _size * _n);
}

static size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
{
  return gzwrite(file, p, _size * _n);
}

static void set_cbs(int gz)
{
  if (gz) {
    areaRead  = gzRead2;
    areaWrite = gzWrite2;
    areaEof   = (areaeof *) gzeof;
    areaSeek  = (areaseek *) gzseek;
    areaClose = (areaclose *) gzclose;
  } else {
    areaRead  = (arearw *) fread;
    areaWrite = (arearw *) fwrite;
    areaEof   = (areaeof *) feof;
    areaSeek  = (areaseek *) fseek;
    areaClose = (areaclose *) fclose;
  }
}

static void *open_save_file(const char *fname, int is_save)
{
  int len = strlen(fname);
  void *afile = NULL;

  if (len > 3 && strcmp(fname + len - 3, ".gz") == 0)
  {
    if ( (afile = gzopen(fname, is_save ? "wb" : "rb")) ) {
      set_cbs(1);
      if (is_save)
        gzsetparams(afile, 9, Z_DEFAULT_STRATEGY);
    }
  }
  else
  {
    if ( (afile = fopen(fname, is_save ? "wb" : "rb")) ) {
      set_cbs(0);
    }
  }

  return afile;
}

// legacy savestate loading
#define SCANP(f, x) areaRead(&Pico.x, sizeof(Pico.x), 1, f)

static int state_load_legacy(void *file)
{
  unsigned char head[32];
  unsigned char cpu[0x60];
  unsigned char cpu_z80[Z80_STATE_SIZE];
  void *ym2612_regs;
  int ok;

  memset(&cpu,0,sizeof(cpu));
  memset(&cpu_z80,0,sizeof(cpu_z80));

  memset(head, 0, sizeof(head));
  areaRead(head, sizeof(head), 1, file);
  if (strcmp((char *)head, "Pico") != 0)
    return -1;

  elprintf(EL_STATUS, "legacy savestate");

  // Scan all the memory areas:
  SCANP(file, ram);
  SCANP(file, vram);
  SCANP(file, zram);
  SCANP(file, cram);
  SCANP(file, vsram);

  // Pack, scan and unpack the cpu data:
  areaRead(cpu, sizeof(cpu), 1, file);
  SekUnpackCpu(cpu, 0);

  SCANP(file, m);
  SCANP(file, video);

  ok = areaRead(cpu_z80, sizeof(cpu_z80), 1, file) == sizeof(cpu_z80);
  // do not unpack if we fail to load z80 state
  if (!ok) z80_reset();
  else     z80_unpack(cpu_z80);

  ym2612_regs = YM2612GetRegs();
  areaRead(sn76496_regs, 28*4, 1, file);
  areaRead(ym2612_regs, 0x200+4, 1, file);
  ym2612_unpack_state();

  return 0;
}

// ---------------------------------------------------------------------------

typedef enum {
  CHUNK_M68K = 1,
  CHUNK_RAM,
  CHUNK_VRAM,
  CHUNK_ZRAM,
  CHUNK_CRAM,    // 5
  CHUNK_VSRAM,
  CHUNK_MISC,
  CHUNK_VIDEO,
  CHUNK_Z80,
  CHUNK_PSG,     // 10
  CHUNK_FM,
  // CD stuff
  CHUNK_S68K,
  CHUNK_PRG_RAM,
  CHUNK_WORD_RAM,
  CHUNK_PCM_RAM, // 15
  CHUNK_BRAM,
  CHUNK_GA_REGS,
  CHUNK_PCM,
  CHUNK_CDC,
  CHUNK_CDD,     // 20
  CHUNK_SCD,
  CHUNK_RC,
  CHUNK_MISC_CD,
  //
  CHUNK_IOPORTS, // versions < 1.70 did not save that..
  CHUNK_SMS,     // 25
  // 32x
  CHUNK_MSH2,
  CHUNK_MSH2_DATA,
  CHUNK_MSH2_PERI,
  CHUNK_SSH2,
  CHUNK_SSH2_DATA, // 30
  CHUNK_SSH2_PERI,
  CHUNK_32XSYS,
  CHUNK_M68K_BIOS,
  CHUNK_MSH2_BIOS,
  CHUNK_SSH2_BIOS, // 35
  CHUNK_SDRAM,
  CHUNK_DRAM,
  CHUNK_32XPAL,
  //
  CHUNK_DEFAULT_COUNT,
  CHUNK_CARTHW_ = CHUNK_CARTHW,  // defined in PicoInt
} chunk_name_e;

static const char * const chunk_names[] = {
  "INVALID!",
  "M68K state",
  "RAM",
  "VRAM",
  "ZRAM",
  "CRAM",      // 5
  "VSRAM",
  "emu state",
  "VIDEO",
  "Z80 state",
  "PSG",       // 10
  "FM",
  // CD stuff
  "S68K state",
  "PRG_RAM",
  "WORD_RAM",
  "PCM_RAM",   // 15
  "BRAM",
  "GATE ARRAY regs",
  "PCM state",
  "CDC",
  "CDD",       // 20
  "SCD",
  "GFX chip",
  "MCD state",
  //
  "IO",
  "SMS state", // 25
  // 32x
  "MSH2",
  "MSH2 data",
  "MSH2 peri",
  "SSH2",
  "SSH2 data", // 30
  "SSH2 peri",
  "32X system regs",
  "M68K BIOS",
  "MSH2 BIOS",
  "SSH2 BIOS", // 35
  "SDRAM",
  "DRAM",
  "PAL",
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
  if (PicoStateProgressCB && name < CHUNK_DEFAULT_COUNT) { \
    strncpy(sbuff + 9, chunk_names[name], sizeof(sbuff) - 9); \
    PicoStateProgressCB(sbuff); \
  } \
  if (!write_chunk(name, len, data, file)) return 1; \
}

#define CHECKED_WRITE_BUFF(name,buff) { \
  if (PicoStateProgressCB && name < CHUNK_DEFAULT_COUNT) { \
    strncpy(sbuff + 9, chunk_names[name], sizeof(sbuff) - 9); \
    PicoStateProgressCB(sbuff); \
  } \
  if (!write_chunk(name, sizeof(buff), &buff, file)) return 1; \
}

static int state_save(void *file)
{
  char sbuff[32] = "Saving.. ";
  unsigned char buff[0x60], buff_z80[Z80_STATE_SIZE];
  void *ym2612_regs = YM2612GetRegs();
  int ver = 0x0170; // not really used..

  areaWrite("PicoSEXT", 1, 8, file);
  areaWrite(&ver, 1, 4, file);

  if (!(PicoAHW & PAHW_SMS)) {
    memset(buff, 0, sizeof(buff));
    SekPackCpu(buff, 0);
    CHECKED_WRITE_BUFF(CHUNK_M68K,  buff);
    CHECKED_WRITE_BUFF(CHUNK_RAM,   Pico.ram);
    CHECKED_WRITE_BUFF(CHUNK_VSRAM, Pico.vsram);
    CHECKED_WRITE_BUFF(CHUNK_IOPORTS, Pico.ioports);
    ym2612_pack_state();
    CHECKED_WRITE(CHUNK_FM, 0x200+4, ym2612_regs);
  }
  else {
    CHECKED_WRITE_BUFF(CHUNK_SMS, Pico.ms);
  }

  CHECKED_WRITE_BUFF(CHUNK_VRAM,  Pico.vram);
  CHECKED_WRITE_BUFF(CHUNK_ZRAM,  Pico.zram);
  CHECKED_WRITE_BUFF(CHUNK_CRAM,  Pico.cram);
  CHECKED_WRITE_BUFF(CHUNK_MISC,  Pico.m);
  CHECKED_WRITE_BUFF(CHUNK_VIDEO, Pico.video);

  z80_pack(buff_z80);
  CHECKED_WRITE_BUFF(CHUNK_Z80, buff_z80);
  CHECKED_WRITE(CHUNK_PSG, 28*4, sn76496_regs);

  if (PicoAHW & PAHW_MCD)
  {
    memset(buff, 0, sizeof(buff));
    SekPackCpu(buff, 1);
    if (Pico_mcd->s68k_regs[3] & 4) // 1M mode?
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

    if (Pico_mcd->s68k_regs[3] & 4) // convert back
      wram_2M_to_1M(Pico_mcd->word_ram2M);
  }

  if (PicoAHW & PAHW_32X)
  {
    unsigned char cpubuff[SH2_STATE_SIZE];

    memset(cpubuff, 0, sizeof(cpubuff));

    sh2_pack(&sh2s[0], cpubuff);
    CHECKED_WRITE_BUFF(CHUNK_MSH2,      cpubuff);
    CHECKED_WRITE_BUFF(CHUNK_MSH2_DATA, Pico32xMem->data_array[0]);
    CHECKED_WRITE_BUFF(CHUNK_MSH2_PERI, Pico32xMem->sh2_peri_regs[0]);

    sh2_pack(&sh2s[1], cpubuff);
    CHECKED_WRITE_BUFF(CHUNK_SSH2,      cpubuff);
    CHECKED_WRITE_BUFF(CHUNK_SSH2_DATA, Pico32xMem->data_array[1]);
    CHECKED_WRITE_BUFF(CHUNK_SSH2_PERI, Pico32xMem->sh2_peri_regs[1]);

    CHECKED_WRITE_BUFF(CHUNK_32XSYS,    Pico32x);
    CHECKED_WRITE_BUFF(CHUNK_M68K_BIOS, Pico32xMem->m68k_rom);
    CHECKED_WRITE_BUFF(CHUNK_MSH2_BIOS, Pico32xMem->sh2_rom_m);
    CHECKED_WRITE_BUFF(CHUNK_SSH2_BIOS, Pico32xMem->sh2_rom_s);
    CHECKED_WRITE_BUFF(CHUNK_SDRAM,     Pico32xMem->sdram);
    CHECKED_WRITE_BUFF(CHUNK_DRAM,      Pico32xMem->dram);
    CHECKED_WRITE_BUFF(CHUNK_32XPAL,    Pico32xMem->pal);
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
  elprintf(EL_STATUS, "load_state @ %x: " error, g_read_offs); \
  return 1; \
}

// when is eof really set?
#define CHECKED_READ(len,data) { \
  if (areaRead(data, 1, len, file) != len) { \
    if (len == 1 && areaEof(file)) goto readend; \
    R_ERROR_RETURN("areaRead: premature EOF\n"); \
    return 1; \
  } \
  g_read_offs += len; \
}

#define CHECKED_READ2(len2,data) { \
  if (len2 != len) { \
    elprintf(EL_STATUS, "unexpected len %i, wanted %i (%s)", len, len2, #len2); \
    if (len > len2) R_ERROR_RETURN("failed."); \
    /* else read anyway and hope for the best.. */ \
  } \
  CHECKED_READ(len, data); \
}

#define CHECKED_READ_BUFF(buff) CHECKED_READ2(sizeof(buff), &buff);

static int state_load(void *file)
{
  unsigned char buff_m68k[0x60], buff_s68k[0x60];
  unsigned char buff_z80[Z80_STATE_SIZE];
  unsigned char buff_sh2[SH2_STATE_SIZE];
  unsigned char chunk;
  void *ym2612_regs;
  char header[8];
  int ver, len;

  g_read_offs = 0;
  CHECKED_READ(8, header);
  if (strncmp(header, "PicoSMCD", 8) && strncmp(header, "PicoSEXT", 8))
    R_ERROR_RETURN("bad header");
  CHECKED_READ(4, &ver);

  while (!areaEof(file))
  {
    CHECKED_READ(1, &chunk);
    CHECKED_READ(4, &len);
    if (len < 0 || len > 1024*512) R_ERROR_RETURN("bad length");
    if (CHUNK_S68K <= chunk && chunk <= CHUNK_MISC_CD && !(PicoAHW & PAHW_MCD))
      R_ERROR_RETURN("cd chunk in non CD state?");
    if (CHUNK_MSH2 <= chunk && chunk <= CHUNK_32XPAL && !(PicoAHW & PAHW_32X))
      R_ERROR_RETURN("32x chunk in non 32x state?");

    switch (chunk)
    {
      case CHUNK_M68K:
        CHECKED_READ_BUFF(buff_m68k);
        break;

      case CHUNK_Z80:
        CHECKED_READ_BUFF(buff_z80);
        z80_unpack(buff_z80);
        break;

      case CHUNK_RAM:     CHECKED_READ_BUFF(Pico.ram); break;
      case CHUNK_VRAM:    CHECKED_READ_BUFF(Pico.vram); break;
      case CHUNK_ZRAM:    CHECKED_READ_BUFF(Pico.zram); break;
      case CHUNK_CRAM:    CHECKED_READ_BUFF(Pico.cram); break;
      case CHUNK_VSRAM:   CHECKED_READ_BUFF(Pico.vsram); break;
      case CHUNK_MISC:    CHECKED_READ_BUFF(Pico.m); break;
      case CHUNK_VIDEO:   CHECKED_READ_BUFF(Pico.video); break;
      case CHUNK_IOPORTS: CHECKED_READ_BUFF(Pico.ioports); break;
      case CHUNK_PSG:     CHECKED_READ2(28*4, sn76496_regs); break;
      case CHUNK_FM:
        ym2612_regs = YM2612GetRegs();
        CHECKED_READ2(0x200+4, ym2612_regs);
        ym2612_unpack_state();
        break;

      case CHUNK_SMS:
        CHECKED_READ_BUFF(Pico.ms);
        break;

      // cd stuff
      case CHUNK_S68K:
        CHECKED_READ_BUFF(buff_s68k);
        break;

      case CHUNK_PRG_RAM:  CHECKED_READ_BUFF(Pico_mcd->prg_ram); break;
      case CHUNK_WORD_RAM: CHECKED_READ_BUFF(Pico_mcd->word_ram2M); break;
      case CHUNK_PCM_RAM:  CHECKED_READ_BUFF(Pico_mcd->pcm_ram); break;
      case CHUNK_BRAM:     CHECKED_READ_BUFF(Pico_mcd->bram); break;
      case CHUNK_GA_REGS:  CHECKED_READ_BUFF(Pico_mcd->s68k_regs); break;
      case CHUNK_PCM:      CHECKED_READ_BUFF(Pico_mcd->pcm); break;
      case CHUNK_CDD:      CHECKED_READ_BUFF(Pico_mcd->cdd); break;
      case CHUNK_CDC:      CHECKED_READ_BUFF(Pico_mcd->cdc); break;
      case CHUNK_SCD:      CHECKED_READ_BUFF(Pico_mcd->scd); break;
      case CHUNK_RC:       CHECKED_READ_BUFF(Pico_mcd->rot_comp); break;
      case CHUNK_MISC_CD:  CHECKED_READ_BUFF(Pico_mcd->m); break;

      // 32x stuff
      case CHUNK_MSH2:
        CHECKED_READ_BUFF(buff_sh2);
        sh2_unpack(&sh2s[0], buff_sh2);
        break;

      case CHUNK_SSH2:
        CHECKED_READ_BUFF(buff_sh2);
        sh2_unpack(&sh2s[1], buff_sh2);
        break;

      case CHUNK_MSH2_DATA:   CHECKED_READ_BUFF(Pico32xMem->data_array[0]); break;
      case CHUNK_MSH2_PERI:   CHECKED_READ_BUFF(Pico32xMem->sh2_peri_regs[0]); break;
      case CHUNK_SSH2_DATA:   CHECKED_READ_BUFF(Pico32xMem->data_array[1]); break;
      case CHUNK_SSH2_PERI:   CHECKED_READ_BUFF(Pico32xMem->sh2_peri_regs[1]); break;
      case CHUNK_32XSYS:      CHECKED_READ_BUFF(Pico32x); break;
      case CHUNK_M68K_BIOS:   CHECKED_READ_BUFF(Pico32xMem->m68k_rom); break;
      case CHUNK_MSH2_BIOS:   CHECKED_READ_BUFF(Pico32xMem->sh2_rom_m); break;
      case CHUNK_SSH2_BIOS:   CHECKED_READ_BUFF(Pico32xMem->sh2_rom_s); break;
      case CHUNK_SDRAM:       CHECKED_READ_BUFF(Pico32xMem->sdram); break;
      case CHUNK_DRAM:        CHECKED_READ_BUFF(Pico32xMem->dram); break;
      case CHUNK_32XPAL:      CHECKED_READ_BUFF(Pico32xMem->pal); break;

      default:
        if (carthw_chunks != NULL)
        {
          carthw_state_chunk *chwc;
          for (chwc = carthw_chunks; chwc->ptr != NULL; chwc++) {
            if (chwc->chunk == chunk) {
              CHECKED_READ2(chwc->size, chwc->ptr);
              goto breakswitch;
            }
          }
        }
        elprintf(EL_STATUS, "load_state: skipping unknown chunk %i of size %i", chunk, len);
        areaSeek(file, len, SEEK_CUR);
        break;
    }
breakswitch:;
  }

readend:
  if (PicoAHW & PAHW_SMS)
    PicoStateLoadedMS();

  if (PicoAHW & PAHW_MCD)
  {
    PicoMemStateLoaded();

    if (!(Pico_mcd->s68k_regs[0x36] & 1) && (Pico_mcd->scd.Status_CDC & 1))
      cdda_start_play();

    // must unpack after mem is set up
    SekUnpackCpu(buff_s68k, 1);
  }

  if (!(PicoAHW & PAHW_SMS))
    SekUnpackCpu(buff_m68k, 0);

  if (PicoAHW & PAHW_32X)
    Pico32xStateLoaded();

  return 0;
}

static int state_load_gfx(void *file)
{
  int ver, len, found = 0, to_find = 4;
  char buff[8];

  if (PicoAHW & PAHW_32X)
    to_find += 2;

  g_read_offs = 0;
  CHECKED_READ(8, buff);
  if (strncmp((char *)buff, "PicoSMCD", 8) && strncmp((char *)buff, "PicoSEXT", 8))
    R_ERROR_RETURN("bad header");
  CHECKED_READ(4, &ver);

  while (!areaEof(file) && found < to_find)
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

      case CHUNK_DRAM:
        if (Pico32xMem != NULL)
          CHECKED_READ_BUFF(Pico32xMem->dram);
        break;

      case CHUNK_32XPAL:
        if (Pico32xMem != NULL)
          CHECKED_READ_BUFF(Pico32xMem->pal);
        Pico32x.dirty_pal = 1;
        break;

      case CHUNK_32XSYS:
        CHECKED_READ_BUFF(Pico32x);
        break;

      default:
        areaSeek(file, len, SEEK_CUR);
        break;
    }
  }

readend:
  return 0;
}

int PicoState(const char *fname, int is_save)
{
  void *afile = NULL;
  int ret;

  afile = open_save_file(fname, is_save);
  if (afile == NULL)
    return -1;

  if (is_save)
    ret = state_save(afile);
  else {
    ret = state_load(afile);
    if (ret != 0) {
      areaSeek(afile, 0, SEEK_SET);
      ret = state_load_legacy(afile);
    }

    if (PicoLoadStateHook != NULL)
      PicoLoadStateHook();
    Pico.m.dirtyPal = 1;
  }

  areaClose(afile);
  return ret;
}

int PicoStateLoadGfx(const char *fname)
{
  void *afile;
  int ret;

  afile = open_save_file(fname, 0);
  if (afile == NULL)
    return -1;

  ret = state_load_gfx(afile);
  if (ret != 0) {
    // assume legacy
    areaSeek(afile, 0x10020, SEEK_SET);  // skip header and RAM
    areaRead(Pico.vram, 1, sizeof(Pico.vram), afile);
    areaSeek(afile, 0x2000, SEEK_CUR);
    areaRead(Pico.cram, 1, sizeof(Pico.cram), afile);
    areaRead(Pico.vsram, 1, sizeof(Pico.vsram), afile);
    areaSeek(afile, 0x221a0, SEEK_SET);
    areaRead(&Pico.video, 1, sizeof(Pico.video), afile);
  }
  areaClose(afile);
  return 0;
}

// tmp state
struct PicoTmp
{
  unsigned short vram[0x8000];
  unsigned short cram[0x40];
  unsigned short vsram[0x40];

  //struct PicoMisc m;
  struct PicoVideo video;

  struct {
    struct Pico32x p32x;
    unsigned short dram[2][0x20000/2];
    unsigned short pal[0x100];
  } t32x;
};

// returns data ptr to free() or PicoTmpStateRestore()
void *PicoTmpStateSave(void)
{
  // gfx only for now
  struct PicoTmp *t = malloc(sizeof(*t));
  if (t == NULL)
    return NULL;

  memcpy(t->vram, Pico.vram, sizeof(Pico.vram));
  memcpy(t->cram, Pico.cram, sizeof(Pico.cram));
  memcpy(t->vsram, Pico.vsram, sizeof(Pico.vsram));
  memcpy(&t->video, &Pico.video, sizeof(Pico.video));

  if (PicoAHW & PAHW_32X) {
    memcpy(&t->t32x.p32x, &Pico32x, sizeof(Pico32x));
    memcpy(t->t32x.dram, Pico32xMem->dram, sizeof(Pico32xMem->dram));
    memcpy(t->t32x.pal, Pico32xMem->pal, sizeof(Pico32xMem->pal));
  }

  return t;
}

void PicoTmpStateRestore(void *data)
{
  struct PicoTmp *t = data;
  if (t == NULL)
    return;

  memcpy(Pico.vram, t->vram, sizeof(Pico.vram));
  memcpy(Pico.cram, t->cram, sizeof(Pico.cram));
  memcpy(Pico.vsram, t->vsram, sizeof(Pico.vsram));
  memcpy(&Pico.video, &t->video, sizeof(Pico.video));
  Pico.m.dirtyPal = 1;

  if (PicoAHW & PAHW_32X) {
    memcpy(&Pico32x, &t->t32x.p32x, sizeof(Pico32x));
    memcpy(Pico32xMem->dram, t->t32x.dram, sizeof(Pico32xMem->dram));
    memcpy(Pico32xMem->pal, t->t32x.pal, sizeof(Pico32xMem->pal));
    Pico32x.dirty_pal = 1;
  }
}

// vim:shiftwidth=2:expandtab
