// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

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

int PicoCartLoad(FILE *f,unsigned char **prom,unsigned int *psize)
{
  unsigned char *rom=NULL; int size;
  if (f==NULL) return 1;

  fseek(f,0,SEEK_END); size=ftell(f); fseek(f,0,SEEK_SET);
  if (size <= 0) return 1;
  size=(size+3)&~3; // Round up to a multiple of 4

  // Allocate space for the rom plus padding
  rom=PicoCartAlloc(size);
  if (rom==NULL) return 1; // { fclose(f); return 1; }

  fread(rom,1,size,f); // Load up the rom

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


#ifdef _UNZIP_SUPPORT

// notaz
#include "../unzip/unzip.h"

// nearly same as PicoCartLoad, but works with zipfiles
int CartLoadZip(const char *fname, unsigned char **prom, unsigned int *psize)
{
	unsigned char *rom=0;
	struct zipent* zipentry;
	int size;
	ZIP *zipfile = openzip(fname);

	if(!zipfile) return 1;

	// find first bin or smd
	while((zipentry = readzip(zipfile)) != 0)
	{
		char *ext;
		if(strlen(zipentry->name) < 5) continue;
		ext = zipentry->name+strlen(zipentry->name)-4;

		if(!strcasecmp(ext, ".bin") || !strcasecmp(ext, ".smd") || !strcasecmp(ext, ".gen")) break;
	}

	if(!zipentry) {
		closezip(zipfile);
		return 4; // no roms
	}

	size = zipentry->uncompressed_size;

	size=(size+3)&~3; // Round up to a multiple of 4

	// Allocate space for the rom plus padding
	rom=PicoCartAlloc(size);
	if (rom==NULL) { closezip(zipfile); return 2; }

	if(readuncompresszip(zipfile, zipentry, (char *)rom) != 0) {
		free(rom);
		rom = 0;
		closezip(zipfile);
		return 5; // unzip failed
	}

	closezip(zipfile);

        // maybe we are loading MegaCD BIOS?
        if (!(PicoMCD&1) && size == 0x20000 &&
			(!strncmp((char *)rom+0x124, "BOOT", 4) || !strncmp((char *)rom+0x128, "BOOT", 4))) {
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

#endif
