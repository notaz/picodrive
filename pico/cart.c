// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006-2007, Grazvydas "notaz" Ignotas
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "pico_int.h"
#include "../zlib/zlib.h"
#include "../unzip/unzip.h"
#include "../unzip/unzip_stream.h"


static const char *rom_exts[] = { "bin", "gen", "smd", "iso", "sms", "gg", "sg" };

void (*PicoCartUnloadHook)(void) = NULL;

void (*PicoCartLoadProgressCB)(int percent) = NULL;
void (*PicoCDLoadProgressCB)(const char *fname, int percent) = NULL; // handled in Pico/cd/cd_file.c

static void PicoCartDetect(void);

/* cso struct */
typedef struct _cso_struct
{
  unsigned char in_buff[2*2048];
  unsigned char out_buff[2048];
  struct {
    char          magic[4];
    unsigned int  unused;
    unsigned int  total_bytes;
    unsigned int  total_bytes_high; // ignored here
    unsigned int  block_size;  // 10h
    unsigned char ver;
    unsigned char align;
    unsigned char reserved[2];
  } header;
  unsigned int  fpos_in;  // input file read pointer
  unsigned int  fpos_out; // pos in virtual decompressed file
  int block_in_buff;      // block which we have read in in_buff
  int pad;
  int index[0];
}
cso_struct;

static int uncompress2(void *dest, int destLen, void *source, int sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*)source;
    stream.avail_in = (uInt)sourceLen;
    stream.next_out = dest;
    stream.avail_out = (uInt)destLen;

    stream.zalloc = NULL;
    stream.zfree = NULL;

    err = inflateInit2(&stream, -15);
    if (err != Z_OK) return err;

    err = inflate(&stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        return err;
    }
    //*destLen = stream.total_out;

    return inflateEnd(&stream);
}

pm_file *pm_open(const char *path)
{
  pm_file *file = NULL;
  const char *ext;
  FILE *f;

  if (path == NULL) return NULL;

  if (strlen(path) < 5) ext = NULL; // no ext
  else ext = path + strlen(path) - 3;

  if (ext && strcasecmp(ext, "zip") == 0)
  {
    struct zipent *zipentry;
    gzFile gzf = NULL;
    ZIP *zipfile;
    int i;

    zipfile = openzip(path);

    if (zipfile != NULL)
    {
      /* search for suitable file (right extension or large enough file) */
      while ((zipentry = readzip(zipfile)) != NULL)
      {
        if (zipentry->uncompressed_size >= 128*1024) goto found_rom_zip;
        if (strlen(zipentry->name) < 5) continue;

        ext = zipentry->name + strlen(zipentry->name) - 2;
        if (ext[-1] != '.') ext--;
        if (ext[-1] != '.') ext--;

        for (i = 0; i < sizeof(rom_exts)/sizeof(rom_exts[0]); i++)
          if (strcasecmp(ext, rom_exts[i]) == 0) goto found_rom_zip;
      }

      /* zipfile given, but nothing found suitable for us inside */
      goto zip_failed;

found_rom_zip:
      /* try to convert to gzip stream, so we could use standard gzio functions from zlib */
      gzf = zip2gz(zipfile, zipentry);
      if (gzf == NULL)  goto zip_failed;

      file = malloc(sizeof(*file));
      if (file == NULL) goto zip_failed;
      file->file  = zipfile;
      file->param = gzf;
      file->size  = zipentry->uncompressed_size;
      file->type  = PMT_ZIP;
      return file;

zip_failed:
      if (gzf) {
        gzclose(gzf);
        zipfile->fp = NULL; // gzclose() closed it
      }
      closezip(zipfile);
      return NULL;
    }
  }
  else if (ext && strcasecmp(ext, "cso") == 0)
  {
    cso_struct *cso = NULL, *tmp = NULL;
    int size;
    f = fopen(path, "rb");
    if (f == NULL)
      goto cso_failed;

#ifndef __EPOC32__
    /* we use our own buffering */
    setvbuf(f, NULL, _IONBF, 0);
#endif

    cso = malloc(sizeof(*cso));
    if (cso == NULL)
      goto cso_failed;

    if (fread(&cso->header, 1, sizeof(cso->header), f) != sizeof(cso->header))
      goto cso_failed;

    if (strncmp(cso->header.magic, "CISO", 4) != 0) {
      elprintf(EL_STATUS, "cso: bad header");
      goto cso_failed;
    }

    if (cso->header.block_size != 2048) {
      elprintf(EL_STATUS, "cso: bad block size (%u)", cso->header.block_size);
      goto cso_failed;
    }

    size = ((cso->header.total_bytes >> 11) + 1)*4 + sizeof(*cso);
    tmp = realloc(cso, size);
    if (tmp == NULL)
      goto cso_failed;
    cso = tmp;
    elprintf(EL_STATUS, "allocated %i bytes for CSO struct", size);

    size -= sizeof(*cso); // index size
    if (fread(cso->index, 1, size, f) != size) {
      elprintf(EL_STATUS, "cso: premature EOF");
      goto cso_failed;
    }

    // all ok
    cso->fpos_in = ftell(f);
    cso->fpos_out = 0;
    cso->block_in_buff = -1;
    file = malloc(sizeof(*file));
    if (file == NULL) goto cso_failed;
    file->file  = f;
    file->param = cso;
    file->size  = cso->header.total_bytes;
    file->type  = PMT_CSO;
    return file;

cso_failed:
    if (cso != NULL) free(cso);
    if (f != NULL) fclose(f);
    return NULL;
  }

  /* not a zip, treat as uncompressed file */
  f = fopen(path, "rb");
  if (f == NULL) return NULL;

  file = malloc(sizeof(*file));
  if (file == NULL) {
    fclose(f);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  file->file  = f;
  file->param = NULL;
  file->size  = ftell(f);
  file->type  = PMT_UNCOMPRESSED;
  fseek(f, 0, SEEK_SET);

#ifndef __EPOC32__ // makes things worse on Symbian
  if (file->size > 0x400000)
    /* we use our own buffering */
    setvbuf(f, NULL, _IONBF, 0);
#endif

  return file;
}

size_t pm_read(void *ptr, size_t bytes, pm_file *stream)
{
  int ret;

  if (stream->type == PMT_UNCOMPRESSED)
  {
    ret = fread(ptr, 1, bytes, stream->file);
  }
  else if (stream->type == PMT_ZIP)
  {
    gzFile gf = stream->param;
    int err;
    ret = gzread(gf, ptr, bytes);
    err = gzerror2(gf);
    if (ret > 0 && (err == Z_DATA_ERROR || err == Z_STREAM_END))
      /* we must reset stream pointer or else next seek/read fails */
      gzrewind(gf);
  }
  else if (stream->type == PMT_CSO)
  {
    cso_struct *cso = stream->param;
    int read_pos, read_len, out_offs, rret;
    int block = cso->fpos_out >> 11;
    int index = cso->index[block];
    int index_end = cso->index[block+1];
    unsigned char *out = ptr, *tmp_dst;

    ret = 0;
    while (bytes != 0)
    {
      out_offs = cso->fpos_out&0x7ff;
      if (out_offs == 0 && bytes >= 2048)
           tmp_dst = out;
      else tmp_dst = cso->out_buff;

      read_pos = (index&0x7fffffff) << cso->header.align;

      if (index < 0) {
        if (read_pos != cso->fpos_in)
          fseek(stream->file, read_pos, SEEK_SET);
        rret = fread(tmp_dst, 1, 2048, stream->file);
        cso->fpos_in = read_pos + rret;
        if (rret != 2048) break;
      } else {
        read_len = (((index_end&0x7fffffff) << cso->header.align) - read_pos) & 0xfff;
        if (block != cso->block_in_buff)
        {
          if (read_pos != cso->fpos_in)
            fseek(stream->file, read_pos, SEEK_SET);
          rret = fread(cso->in_buff, 1, read_len, stream->file);
          cso->fpos_in = read_pos + rret;
          if (rret != read_len) {
            elprintf(EL_STATUS, "cso: read failed @ %08x", read_pos);
            break;
          }
          cso->block_in_buff = block;
        }
        rret = uncompress2(tmp_dst, 2048, cso->in_buff, read_len);
        if (rret != 0) {
          elprintf(EL_STATUS, "cso: uncompress failed @ %08x with %i", read_pos, rret);
          break;
        }
      }

      rret = 2048;
      if (out_offs != 0 || bytes < 2048) {
        //elprintf(EL_STATUS, "cso: unaligned/nonfull @ %08x, offs=%i, len=%u", cso->fpos_out, out_offs, bytes);
        if (bytes < rret) rret = bytes;
        if (2048 - out_offs < rret) rret = 2048 - out_offs;
        memcpy(out, tmp_dst + out_offs, rret);
      }
      ret += rret;
      out += rret;
      cso->fpos_out += rret;
      bytes -= rret;
      block++;
      index = index_end;
      index_end = cso->index[block+1];
    }
  }
  else
    ret = 0;

  return ret;
}

int pm_seek(pm_file *stream, long offset, int whence)
{
  if (stream->type == PMT_UNCOMPRESSED)
  {
    fseek(stream->file, offset, whence);
    return ftell(stream->file);
  }
  else if (stream->type == PMT_ZIP)
  {
    if (PicoMessage != NULL && offset > 6*1024*1024) {
      long pos = gztell((gzFile) stream->param);
      if (offset < pos || offset - pos > 6*1024*1024)
        PicoMessage("Decompressing data...");
    }
    return gzseek((gzFile) stream->param, offset, whence);
  }
  else if (stream->type == PMT_CSO)
  {
    cso_struct *cso = stream->param;
    switch (whence)
    {
      case SEEK_CUR: cso->fpos_out += offset; break;
      case SEEK_SET: cso->fpos_out  = offset; break;
      case SEEK_END: cso->fpos_out  = cso->header.total_bytes - offset; break;
    }
    return cso->fpos_out;
  }
  else
    return -1;
}

int pm_close(pm_file *fp)
{
  int ret = 0;

  if (fp == NULL) return EOF;

  if (fp->type == PMT_UNCOMPRESSED)
  {
    fclose(fp->file);
  }
  else if (fp->type == PMT_ZIP)
  {
    ZIP *zipfile = fp->file;
    gzclose((gzFile) fp->param);
    zipfile->fp = NULL; // gzclose() closed it
    closezip(zipfile);
  }
  else if (fp->type == PMT_CSO)
  {
    free(fp->param);
    fclose(fp->file);
  }
  else
    ret = EOF;

  free(fp);
  return ret;
}


void Byteswap(unsigned char *data,int len)
{
  int i=0;

  if (len<2) return; // Too short

  do
  {
    unsigned short *pd=(unsigned short *)(data+i);
    int value=*pd; // Get 2 bytes

    value=(value<<8)|(value>>8); // Byteswap it
    *pd=(unsigned short)value; // Put 2b ytes
    i+=2;
  }
  while (i+2<=len);
}

// Interleve a 16k block and byteswap
static int InterleveBlock(unsigned char *dest,unsigned char *src)
{
  int i=0;
  for (i=0;i<0x2000;i++) dest[(i<<1)  ]=src[       i]; // Odd
  for (i=0;i<0x2000;i++) dest[(i<<1)+1]=src[0x2000+i]; // Even
  return 0;
}

// Decode a SMD file
static int DecodeSmd(unsigned char *data,int len)
{
  unsigned char *temp=NULL;
  int i=0;

  temp=(unsigned char *)malloc(0x4000);
  if (temp==NULL) return 1;
  memset(temp,0,0x4000);

  // Interleve each 16k block and shift down by 0x200:
  for (i=0; i+0x4200<=len; i+=0x4000)
  {
    InterleveBlock(temp,data+0x200+i); // Interleve 16k to temporary buffer
    memcpy(data+i,temp,0x4000); // Copy back in
  }

  free(temp);
  return 0;
}

static unsigned char *cd_realloc(void *old, int filesize)
{
  unsigned char *rom;
  rom=realloc(old, sizeof(mcd_state));
  if (rom) memset(rom+0x20000, 0, sizeof(mcd_state)-0x20000);
  return rom;
}

static unsigned char *PicoCartAlloc(int filesize)
{
  int alloc_size;
  unsigned char *rom;

  if (PicoAHW & PAHW_MCD) return cd_realloc(NULL, filesize);

  alloc_size=filesize+0x7ffff;
  if((filesize&0x3fff)==0x200) alloc_size-=0x200;
  alloc_size&=~0x7ffff; // use alloc size of multiples of 512K, so that memhandlers could be set up more efficiently
  if((filesize&0x3fff)==0x200) alloc_size+=0x200;
  else if(alloc_size-filesize < 4) alloc_size+=4; // padding for out-of-bound exec protection

  // Allocate space for the rom plus padding
  rom=(unsigned char *)malloc(alloc_size);
  if(rom) memset(rom+alloc_size-0x80000,0,0x80000);
  return rom;
}

int PicoCartLoad(pm_file *f,unsigned char **prom,unsigned int *psize,int is_sms)
{
  unsigned char *rom=NULL; int size, bytes_read;
  if (f==NULL) return 1;

  size=f->size;
  if (size <= 0) return 1;
  size=(size+3)&~3; // Round up to a multiple of 4

  // Allocate space for the rom plus padding
  rom=PicoCartAlloc(size);
  if (rom==NULL) {
    elprintf(EL_STATUS, "out of memory (wanted %i)", size);
    return 2;
  }

  if (PicoCartLoadProgressCB != NULL)
  {
    // read ROM in blocks, just for fun
    int ret;
    unsigned char *p = rom;
    bytes_read=0;
    do
    {
      int todo = size - bytes_read;
      if (todo > 256*1024) todo = 256*1024;
      ret = pm_read(p,todo,f);
      bytes_read += ret;
      p += ret;
      PicoCartLoadProgressCB(bytes_read * 100 / size);
    }
    while (ret > 0);
  }
  else
    bytes_read = pm_read(rom,size,f); // Load up the rom
  if (bytes_read <= 0) {
    elprintf(EL_STATUS, "read failed");
    free(rom);
    return 3;
  }

  if (!is_sms)
  {
    // maybe we are loading MegaCD BIOS?
    if (!(PicoAHW & PAHW_MCD) && size == 0x20000 && (!strncmp((char *)rom+0x124, "BOOT", 4) ||
         !strncmp((char *)rom+0x128, "BOOT", 4))) {
      PicoAHW |= PAHW_MCD;
      rom = cd_realloc(rom, size);
    }

    // Check for SMD:
    if (size >= 0x4200 && (size&0x3fff)==0x200 &&
        ((rom[0x2280] == 'S' && rom[0x280] == 'E') || (rom[0x280] == 'S' && rom[0x2281] == 'E'))) {
      DecodeSmd(rom,size); size-=0x200; // Decode and byteswap SMD
    }
    else Byteswap(rom,size); // Just byteswap
  }

  if (prom)  *prom=rom;
  if (psize) *psize=size;

  return 0;
}

// Insert a cartridge:
int PicoCartInsert(unsigned char *rom,unsigned int romsize)
{
  // notaz: add a 68k "jump one op back" opcode to the end of ROM.
  // This will hang the emu, but will prevent nasty crashes.
  // note: 4 bytes are padded to every ROM
  if (rom != NULL)
    *(unsigned long *)(rom+romsize) = 0xFFFE4EFA; // 4EFA FFFE byteswapped

  Pico.rom=rom;
  Pico.romsize=romsize;

  if (SRam.data) {
    free(SRam.data);
    SRam.data = NULL;
  }

  if (PicoCartUnloadHook != NULL) {
    PicoCartUnloadHook();
    PicoCartUnloadHook = NULL;
  }

  PicoAHW &= PAHW_MCD|PAHW_SMS;

  PicoMemResetHooks();
  PicoDmaHook = NULL;
  PicoResetHook = NULL;
  PicoLineHook = NULL;
  PicoLoadStateHook = NULL;
  carthw_chunks = NULL;

  PicoMemReset();

  if (!(PicoAHW & (PAHW_MCD|PAHW_SMS)))
    PicoCartDetect();

  // setup correct memory map for loaded ROM
  // call PicoMemReset again due to possible memmap change
  switch (PicoAHW) {
    default:
      elprintf(EL_STATUS|EL_ANOMALY, "starting in unknown hw configuration: %x", PicoAHW);
    case 0:
    case PAHW_SVP:  PicoMemSetup(); break;
    case PAHW_MCD:  PicoMemSetupCD(); break;
    case PAHW_PICO: PicoMemSetupPico(); break;
    case PAHW_SMS:  PicoMemSetupMS(); break;
  }
  PicoMemReset();

  if (PicoAHW & PAHW_SMS)
    PicoPowerMS();
  else
    PicoPower();

  return 0;
}

void PicoCartUnload(void)
{
  if (PicoCartUnloadHook != NULL) {
    PicoCartUnloadHook();
    PicoCartUnloadHook = NULL;
  }

  if (Pico.rom != NULL) {
    SekFinishIdleDet();
    free(Pico.rom);
    Pico.rom=NULL;
  }
}

static int rom_strcmp(int rom_offset, const char *s1)
{
  int i, len = strlen(s1);
  const char *s_rom = (const char *)Pico.rom + rom_offset;
  for (i = 0; i < len; i++)
    if (s1[i] != s_rom[i^1])
      return 1;
  return 0;
}

static int name_cmp(const char *name)
{
  return rom_strcmp(0x150, name);
}

/*
 * various cart-specific things, which can't be handled by generic code
 * (maybe I should start using CRC for this stuff?)
 */
static void PicoCartDetect(void)
{
  int sram_size = 0, csum;
  Pico.m.sram_reg = 0;

  csum = PicoRead32(0x18c) & 0xffff;

  if (Pico.rom[0x1B1] == 'R' && Pico.rom[0x1B0] == 'A')
  {
    if (Pico.rom[0x1B2] & 0x40)
    {
      // EEPROM
      SRam.start = PicoRead32(0x1B4) & ~1; // zero address is used for clock by some games
      SRam.end   = PicoRead32(0x1B8);
      sram_size  = 0x2000;
      Pico.m.sram_reg |= 4;
    } else {
      // normal SRAM
      SRam.start = PicoRead32(0x1B4) & ~0xff;
      SRam.end   = PicoRead32(0x1B8) | 1;
      sram_size  = SRam.end - SRam.start + 1;
    }
    SRam.start &= ~0xff000000;
    SRam.end   &= ~0xff000000;
    Pico.m.sram_reg |= 0x10; // SRAM was detected
  }
  if (sram_size <= 0)
  {
    // some games may have bad headers, like S&K and Sonic3
    // note: majority games use 0x200000 as starting address, but there are some which
    // use something else (0x300000 by HardBall '95). Luckily they have good headers.
    SRam.start = 0x200000;
    SRam.end   = 0x203FFF;
    sram_size  = 0x004000;
  }

  // this game actually doesn't have SRAM, but some weird protection
  if (rom_strcmp(0x120, "PUGGSY") == 0)
  {
    SRam.start = SRam.end = sram_size = 0;
  }

  if (sram_size)
  {
    SRam.data = (unsigned char *) calloc(sram_size, 1);
    if (SRam.data == NULL) return;
  }
  SRam.changed = 0;

  // set EEPROM defaults, in case it gets detected
  SRam.eeprom_type   = 0; // 7bit (24C01)
  SRam.eeprom_abits  = 3; // eeprom access must be odd addr for: bit0 ~ cl, bit1 ~ in
  SRam.eeprom_bit_cl = 1;
  SRam.eeprom_bit_in = 0;
  SRam.eeprom_bit_out= 0;

  // some known EEPROM data (thanks to EkeEke)
  if (name_cmp("COLLEGE SLAM") == 0 ||
      name_cmp("FRANK THOMAS BIGHURT BASEBAL") == 0)
  {
    SRam.eeprom_type = 3;
    SRam.eeprom_abits = 2;
    SRam.eeprom_bit_cl = 0;
  }
  else if (name_cmp("NBA JAM TOURNAMENT EDITION") == 0 ||
           name_cmp("NFL QUARTERBACK CLUB") == 0)
  {
    SRam.eeprom_type = 2;
    SRam.eeprom_abits = 2;
    SRam.eeprom_bit_cl = 0;
  }
  else if (name_cmp("NBA JAM") == 0)
  {
    SRam.eeprom_type = 2;
    SRam.eeprom_bit_out = 1;
    SRam.eeprom_abits = 0;
  }
  else if (name_cmp("NHLPA HOCKEY '93") == 0 ||
           name_cmp("NHLPA Hockey '93") == 0 ||
           name_cmp("RINGS OF POWER") == 0)
  {
    SRam.start = SRam.end = 0x200000;
    Pico.m.sram_reg = 0x14;
    SRam.eeprom_abits = 0;
    SRam.eeprom_bit_cl = 6;
    SRam.eeprom_bit_in = 7;
    SRam.eeprom_bit_out= 7;
  }
  else if ( name_cmp("MICRO MACHINES II") == 0 ||
           (name_cmp("        ") == 0 && // Micro Machines {Turbo Tournament '96, Military - It's a Blast!}
           (csum == 0x165e || csum == 0x168b || csum == 0xCEE0 || csum == 0x2C41)))
  {
    SRam.start = 0x300000;
    SRam.end   = 0x380001;
    Pico.m.sram_reg = 0x14;
    SRam.eeprom_type = 2;
    SRam.eeprom_abits = 0;
    SRam.eeprom_bit_cl = 1;
    SRam.eeprom_bit_in = 0;
    SRam.eeprom_bit_out= 7;
  }

  // SVP detection
  else if (name_cmp("Virtua Racing") == 0 ||
           name_cmp("VIRTUA RACING") == 0)
  {
    PicoSVPStartup();
  }

  // Pico
  else if (rom_strcmp(0x100, "SEGA PICO") == 0 ||
           rom_strcmp(0x100, "IMA IKUNOUJYUKU") == 0) // what is that supposed to mean?
  {
    PicoInitPico();
  }

  // Detect 12-in-1 mapper
  else if ((name_cmp("ROBOCOP 3") == 0 && Pico.romsize == 0x200000) ||
    (rom_strcmp(0x160, "FLICKY") == 0 && Pico.romsize >= 0x200000)  ||
    (name_cmp(" SHOVE IT!") == 0 && Pico.romsize >= 0x200000) ||
    (name_cmp("MS PACMAN") == 0 && Pico.romsize >= 0x200000) || // bad dump?
    (name_cmp("ALIEN 3") == 0 && Pico.romsize == 0x200000))
  {
    carthw_12in1_startup();
  }

  // Realtec mapper
  else if (Pico.romsize == 512*1024 && (
    rom_strcmp(0x94, "THE EARTH DEFEND") == 0 ||
    rom_strcmp(0xfe, "WISEGAME 11-03-1993") == 0 || // Funny World
    rom_strcmp(0x95, "MALLET LEGEND ") == 0)) // Whac-A-Critter
  {
    carthw_realtec_startup();
  }

  // Radica mapper
  else if (name_cmp("KID CHAMELEON") == 0 && Pico.romsize > 0x100000)
  {
    carthw_radica_startup();
  }

  // Some games malfunction if SRAM is not filled with 0xff
  if (name_cmp("DINO DINI'S SOCCER") == 0 ||
      name_cmp("MICRO MACHINES II") == 0)
  {
    memset(SRam.data, 0xff, sram_size);
  }

  // Unusual region 'code'
  if (rom_strcmp(0x1f0, "EUROPE") == 0 || rom_strcmp(0x1f0, "Europe") == 0)
    *(int *) (Pico.rom+0x1f0) = 0x20204520;
}

