#include "../PicoInt.h"
#include "../sound/sn76496.h"

#ifndef UTYPES_DEFINED
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
#define UTYPES_DEFINED
#endif


// -----------------------------------------------------------------
//                     Read Rom and read Ram

static u32 PicoReadPico8(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d = *(u8 *)(Pico.ram+((a^1)&0xffff)); goto end; } // Ram
  if (a<Pico.romsize) { d = *(u8 *)(Pico.rom+(a^1)); goto end; } // Rom

  a&=0xffffff;

  if ((a&0xfffff0)==0xc00000) { // VDP
    d=PicoVideoRead(a);
    if ((a&1)==0) d>>=8;
    goto end;
  }

  if ((a&0xffffe0)==0x800000) // Pico I/O
  {
    switch (a & 0x1f)
    {
      case 0x01: d = PicoPicohw.r1; break;
      case 0x03:
        d  =  PicoPad[0]&0x1f; // d-pad
        d |= (PicoPad[0]&0x20) << 2; // pen push -> C
        d  = ~d;
        break;

      case 0x05: d = (PicoPicohw.pen_pos[0] >> 8);  break; // what is MS bit for? Games read it..
      case 0x07: d =  PicoPicohw.pen_pos[0] & 0xff; break;
      case 0x09: d = (PicoPicohw.pen_pos[1] >> 8);  break;
      case 0x0b: d =  PicoPicohw.pen_pos[1] & 0xff; break;
      case 0x0d: d = (1 << (PicoPicohw.page & 7)) - 1; break;
      case 0x12: d = PicoPicohw.fifo_bytes == 0 ? 0x80 : 0; break; // guess
      default:
        elprintf(EL_UIO, "r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
        break;
    }
  }

  //elprintf(EL_UIO, "r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);

end:
  elprintf(EL_IO, "r8 : %06x,   %02x @%06x", a&0xffffff, (u8)d, SekPc);
  return d;
}

static u32 PicoReadPico16(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { d=*(u16 *)(Pico.ram+(a&0xfffe)); goto end; } // Ram

  a&=0xfffffe;

  if (a<Pico.romsize) { d = *(u16 *)(Pico.rom+a); goto end; } // Rom

  if ((a&0xfffff0)==0xc00000) {
    d = PicoVideoRead(a);
    goto end;
  }

  if      (a == 0x800010)
    d = (PicoPicohw.fifo_bytes > 0x3f) ? 0 : (0x3f - PicoPicohw.fifo_bytes);
  else if (a == 0x800012)
    d = PicoPicohw.fifo_bytes == 0 ? 0x8000 : 0; // guess
  else
    elprintf(EL_UIO, "r16: %06x, %04x @%06x", a&0xffffff, d, SekPc);

  //elprintf(EL_UIO, "r16: %06x, %04x @%06x", a&0xffffff, d, SekPc);

end:
  elprintf(EL_IO, "r16: %06x, %04x @%06x", a&0xffffff, d, SekPc);
  return d;
}

static u32 PicoReadPico32(u32 a)
{
  u32 d=0;

  if ((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); d = (pm[0]<<16)|pm[1]; goto end; } // Ram

  a&=0xfffffe;

  if (a<Pico.romsize) { u16 *pm=(u16 *)(Pico.rom+a); d = (pm[0]<<16)|pm[1]; goto end; } // Rom

  if ((a&0xfffff0)==0xc00000) {
    d = (PicoVideoRead(a)<<16)|PicoVideoRead(a+2);
    goto end;
  }

  elprintf(EL_UIO, "r32: %06x, %08x @%06x", a&0xffffff, d, SekPc);

end:
  elprintf(EL_IO, "r32: %06x, %08x @%06x", a&0xffffff, d, SekPc);
  return d;
}

// -----------------------------------------------------------------
//                            Write Ram

/*
void dump(u16 w)
{
  static FILE *f[0x10] = { NULL, };
  char fname[32];
  int num = PicoPicohw.r12 & 0xf;

  sprintf(fname, "ldump%i.bin", num);
  if (f[num] == NULL)
    f[num] = fopen(fname, "wb");
  fwrite(&w, 1, 2, f[num]);
  //fclose(f);
}
*/

static void PicoWritePico8(u32 a,u8 d)
{
  elprintf(EL_IO, "w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);

  if ((a&0xe00000)==0xe00000) { *(u8 *)(Pico.ram+((a^1)&0xffff))=d; return; } // Ram

  a&=0xffffff;
  if ((a&0xfffff9)==0xc00011) { if (PicoOpt&2) SN76496Write(d); return; } // PSG Sound

  if ((a&0xfffff0)==0xc00000) { // VDP
    d&=0xff;
    PicoVideoWrite(a,(u16)(d|(d<<8))); // Byte access gets mirrored
    return;
  }

  switch (a & 0x1f) {
    case 0x19: case 0x1b: case 0x1d: case 0x1f: break; // 'S' 'E' 'G' 'A'
    default:
      elprintf(EL_UIO, "w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
      break;
  }
  //elprintf(EL_UIO, "w8 : %06x,   %02x @%06x", a&0xffffff, d, SekPc);
}

static void PicoWritePico16(u32 a,u16 d)
{
  elprintf(EL_IO, "w16: %06x, %04x", a&0xffffff, d);

  if ((a&0xe00000)==0xe00000) { *(u16 *)(Pico.ram+(a&0xfffe))=d; return; } // Ram

  a&=0xfffffe;
  if ((a&0xfffff0)==0xc00000) { PicoVideoWrite(a,(u16)d); return; } // VDP

//  if (a == 0x800010) dump(d);
  if (a == 0x800010)
  {
    PicoPicohw.fifo_bytes += 2;

    if (PicoPicohw.xpcm_ptr < PicoPicohw.xpcm_buffer + XPCM_BUFFER_SIZE) {
      *PicoPicohw.xpcm_ptr++ = d >> 8;
      *PicoPicohw.xpcm_ptr++ = d;
    }
    else if (PicoPicohw.xpcm_ptr == PicoPicohw.xpcm_buffer + XPCM_BUFFER_SIZE) {
      elprintf(EL_ANOMALY|EL_PICOHW, "xpcm_buffer overflow!");
      PicoPicohw.xpcm_ptr++;
    }
  }
  else if (a == 0x800012) {
    int r12_old = PicoPicohw.r12;
    PicoPicohw.r12 = d;
    if (r12_old != d)
      PicoReratePico();
  }
  else
    elprintf(EL_UIO, "w16: %06x, %04x", a&0xffffff, d);

  //elprintf(EL_UIO, "w16: %06x, %04x", a&0xffffff, d);
}

static void PicoWritePico32(u32 a,u32 d)
{
  elprintf(EL_IO, "w32: %06x, %08x", a&0xffffff, d);

  if ((a&0xe00000)==0xe00000)
  {
    // Ram:
    u16 *pm=(u16 *)(Pico.ram+(a&0xfffe));
    pm[0]=(u16)(d>>16); pm[1]=(u16)d;
    return;
  }

  a&=0xfffffe;
  if ((a&0xfffff0)==0xc00000)
  {
    // VDP:
    PicoVideoWrite(a,  (u16)(d>>16));
    PicoVideoWrite(a+2,(u16)d);
    return;
  }

  elprintf(EL_UIO, "w32: %06x, %08x", a&0xffffff, d);
}

#ifdef EMU_M68K
extern unsigned int (*pm68k_read_memory_8) (unsigned int address);
extern unsigned int (*pm68k_read_memory_16)(unsigned int address);
extern unsigned int (*pm68k_read_memory_32)(unsigned int address);
extern void (*pm68k_write_memory_8) (unsigned int address, unsigned char  value);
extern void (*pm68k_write_memory_16)(unsigned int address, unsigned short value);
extern void (*pm68k_write_memory_32)(unsigned int address, unsigned int   value);
extern unsigned int (*pm68k_read_memory_pcr_8) (unsigned int address);
extern unsigned int (*pm68k_read_memory_pcr_16)(unsigned int address);
extern unsigned int (*pm68k_read_memory_pcr_32)(unsigned int address);

static unsigned int m68k_read_memory_pcrp_8(unsigned int a)
{
  if((a&0xe00000)==0xe00000) return *(u8 *)(Pico.ram+((a^1)&0xffff)); // Ram
  return 0;
}

static unsigned int m68k_read_memory_pcrp_16(unsigned int a)
{
  if((a&0xe00000)==0xe00000) return *(u16 *)(Pico.ram+(a&0xfffe)); // Ram
  return 0;
}

static unsigned int m68k_read_memory_pcrp_32(unsigned int a)
{
  if((a&0xe00000)==0xe00000) { u16 *pm=(u16 *)(Pico.ram+(a&0xfffe)); return (pm[0]<<16)|pm[1]; } // Ram
  return 0;
}
#endif // EMU_M68K


PICO_INTERNAL void PicoMemSetupPico(void)
{
#ifdef EMU_C68K
  PicoCpuCM68k.checkpc=PicoCheckPc;
  PicoCpuCM68k.fetch8 =PicoCpuCM68k.read8 =PicoReadPico8;
  PicoCpuCM68k.fetch16=PicoCpuCM68k.read16=PicoReadPico16;
  PicoCpuCM68k.fetch32=PicoCpuCM68k.read32=PicoReadPico32;
  PicoCpuCM68k.write8 =PicoWritePico8;
  PicoCpuCM68k.write16=PicoWritePico16;
  PicoCpuCM68k.write32=PicoWritePico32;
#endif
#ifdef EMU_M68K
  pm68k_read_memory_8  = PicoReadPico8;
  pm68k_read_memory_16 = PicoReadPico16;
  pm68k_read_memory_32 = PicoReadPico32;
  pm68k_write_memory_8  = PicoWritePico8;
  pm68k_write_memory_16 = PicoWritePico16;
  pm68k_write_memory_32 = PicoWritePico32;
  pm68k_read_memory_pcr_8  = m68k_read_memory_pcrp_8;
  pm68k_read_memory_pcr_16 = m68k_read_memory_pcrp_16;
  pm68k_read_memory_pcr_32 = m68k_read_memory_pcrp_32;
#endif
#ifdef EMU_F68K
  // use standard setup, only override handlers
  PicoMemSetup();
  PicoCpuFM68k.read_byte =PicoReadPico8;
  PicoCpuFM68k.read_word =PicoReadPico16;
  PicoCpuFM68k.read_long =PicoReadPico32;
  PicoCpuFM68k.write_byte=PicoWritePico8;
  PicoCpuFM68k.write_word=PicoWritePico16;
  PicoCpuFM68k.write_long=PicoWritePico32;
#endif
}

