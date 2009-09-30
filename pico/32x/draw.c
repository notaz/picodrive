#include "../pico_int.h"

static void convert_pal555(int invert_prio)
{
  unsigned int *ps = (void *)Pico32xMem->pal;
  unsigned int *pd = (void *)Pico32xMem->pal_native;
  unsigned int m1 = 0x001f001f;
  unsigned int m2 = 0x03e003e0;
  unsigned int m3 = 0xfc00fc00;
  unsigned int inv = 0;
  int i;

  if (invert_prio)
    inv = 0x00200020;

  // place prio to LS green bit
  for (i = 0x100/2; i > 0; i--, ps++, pd++) {
    unsigned int t = *ps;
    *pd = (((t & m1) << 11) | ((t & m2) << 1) | ((t & m3) >> 10)) ^ inv;
  }

  Pico32x.dirty_pal = 0;
}

void FinalizeLine32xRGB555(int sh, int line)
{
  unsigned short *pd = DrawLineDest;
  unsigned short *pal = Pico32xMem->pal_native;
  unsigned char *pb = HighCol + 8;
  unsigned short *dram, *ps, cram0;
  int i;

  // this is a bit hackish:
  // we swap cram color 0 with color that is used for background,
  // as bg is forced to 0 when we do 32X
  cram0 = Pico.cram[0];
  Pico.cram[0] = Pico.cram[Pico.video.reg[7] & 0x3f];

  FinalizeLineRGB555(sh, line);
  Pico.cram[0] = cram0;

  if ((Pico32x.vdp_regs[0] & P32XV_Mx) == 0)
    return; // blanking

  // XXX: how is 32col mode hadled by real hardware?
  if (!(Pico.video.reg[12] & 1))
    return;

  if (!(PicoDrawMask & PDRAW_32X_ON))
    return;

  dram = (void *)Pico32xMem->dram[Pico32x.vdp_regs[0x0a/2] & P32XV_FS];
  ps = dram + dram[line];

  if ((Pico32x.vdp_regs[0] & P32XV_Mx) == 2) { // Direct Color Mode
    int inv = (Pico32x.vdp_regs[0] & P32XV_PRI) ? 0x8000 : 0;
    unsigned int m1 = 0x001f001f;
    unsigned int m2 = 0x03e003e0;
    unsigned int m3 = 0xfc00fc00;

    for (i = 320; i > 0; i--, ps++, pd++, pb++) {
      unsigned short t = *ps;
      if (*pb != 0 && !((t ^ inv) & 0x8000))
        continue;

      *pd = ((t & m1) << 11) | ((t & m2) << 1) | ((t & m3) >> 10);
    }
    return;
  }

  if (Pico32x.dirty_pal)
    convert_pal555(Pico32x.vdp_regs[0] & P32XV_PRI);

  if ((Pico32x.vdp_regs[0] & P32XV_Mx) == 1) { // Packed Pixel Mode
    unsigned short t;
    for (i = 320/2; i > 0; i--, ps++, pd += 2, pb += 2) {
      t = pal[*ps >> 8];
      if (pb[0] == 0 || (t & 0x20))
        pd[0] = t;
      t = pal[*ps & 0xff];
      if (pb[1] == 0 || (t & 0x20))
        pd[1] = t;
    }
  }
  else { // Run Length Mode
    unsigned short len, t;
    for (i = 320; i > 0; ps++) {
      t = pal[*ps & 0xff];
      for (len = (*ps >> 8) + 1; len > 0 && i > 0; len--, i--, pd++, pb++)
        if (*pb == 0 || (t & 0x20))
          *pd = t;
    }
  }
}

