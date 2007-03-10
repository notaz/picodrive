// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"
#include "../zlib/zlib.h"
#include "../unzip/unzip.h"
#include "../unzip/unzip_stream.h"

static char *rom_exts[] = { "bin", "gen", "smd", "iso" };


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

        ext = zipentry->name+strlen(zipentry->name)-3;
        for (i = 0; i < sizeof(rom_exts)/sizeof(rom_exts[0]); i++)
          if (!strcasecmp(ext, rom_exts[i]) == 0) goto found_rom_zip;
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
  else
    ret = 0;

  return ret;
}

int pm_seek(pm_file *stream, long offset, int whence)
{
  if (stream->type == PMT_UNCOMPRESSED)
  {
    return fseek(stream->file, offset, whence);
  }
  else if (stream->type == PMT_ZIP)
  {
    return gzseek((gzFile) stream->param, offset, whence);
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
  dprintf("sizeof(mcd_state): %i", sizeof(mcd_state));
  rom=realloc(old, sizeof(mcd_state));
  if (rom) memset(rom+0x20000, 0, sizeof(mcd_state)-0x20000);
  return rom;
}

static unsigned char *PicoCartAlloc(int filesize)
{
  int alloc_size;
  unsigned char *rom;

  if (PicoMCD & 1) return cd_realloc(NULL, filesize);

  alloc_size=filesize+0x7ffff;
  if((filesize&0x3fff)==0x200) alloc_size-=0x200;
  alloc_size&=~0x7ffff; // use alloc size of multiples of 512K, so that memhandlers could be set up more efficiently
  if((filesize&0x3fff)==0x200) alloc_size+=0x200;
  else if(alloc_size-filesize < 4) alloc_size+=4; // padding for out-of-bound exec protection
  //dprintf("alloc_size: %x\n",  alloc_size);

  // Allocate space for the rom plus padding
  rom=(unsigned char *)malloc(alloc_size);
  if(rom) memset(rom+alloc_size-0x80000,0,0x80000);
  return rom;
}

int PicoCartLoad(pm_file *f,unsigned char **prom,unsigned int *psize)
{
  unsigned char *rom=NULL; int size;
  if (f==NULL) return 1;

  size=f->size;
  if (size <= 0) return 1;
  size=(size+3)&~3; // Round up to a multiple of 4

  // Allocate space for the rom plus padding
  rom=PicoCartAlloc(size);
  if (rom==NULL) return 1; // { fclose(f); return 1; }

  pm_read(rom,size,f); // Load up the rom

  // maybe we are loading MegaCD BIOS?
  if (!(PicoMCD&1) && size == 0x20000 && (!strncmp((char *)rom+0x124, "BOOT", 4) || !strncmp((char *)rom+0x128, "BOOT", 4))) {
    PicoMCD |= 1;
    rom = cd_realloc(rom, size);
  }

  // Check for SMD:
  if ((size&0x3fff)==0x200) { DecodeSmd(rom,size); size-=0x200; } // Decode and byteswap SMD
  else Byteswap(rom,size); // Just byteswap

  if (prom)  *prom=rom;
  if (psize) *psize=size;

  return 0;
}

// Insert/remove a cartridge:
int PicoCartInsert(unsigned char *rom,unsigned int romsize)
{
  // notaz: add a 68k "jump one op back" opcode to the end of ROM.
  // This will hang the emu, but will prevent nasty crashes.
  // note: 4 bytes are padded to every ROM
  if(rom != NULL)
    *(unsigned long *)(rom+romsize) = 0xFFFE4EFA; // 4EFA FFFE byteswapped

  SRam.resize=1;
  Pico.rom=rom;
  Pico.romsize=romsize;

  return PicoReset(1);
}

int PicoUnloadCart(unsigned char* romdata)
{
  free(romdata);
  return 0;
}

