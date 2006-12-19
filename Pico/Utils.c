// This is part of Pico Library

// (c) Copyright 2004 Dave, All rights reserved.
// (c) Copyright 2006 notaz, All rights reserved.
// Free for non-commercial use.

// For commercial use, separate licencing terms must be obtained.


#include "PicoInt.h"

int PicuAnd=0xf7de;

// Quick low-quality conversion of 320 to 176:
int PicuQuick(unsigned short *dest,unsigned short *src)
{
  unsigned short *end=NULL;

  src+=13; end=src+290;
  dest++;

  do
  {
    *dest++=*src++;
    *dest++=*src; src+=2;
    *dest++=*src; src+=2;
    *dest++=*src++;
    *dest++=*src; src+=2;
    *dest++=*src; src+=2;
  }
  while (src<end);

  return 0;
}

// Shrink the pixels in src/srcLen, to the screen line pointed to by dest/destLen
int PicuShrink(unsigned short *dest,int destLen,unsigned short *src,int srcLen)
{
  unsigned short *end=NULL;
  int bias=0,pa=0,sub=0;

  end=dest+destLen;
  sub=srcLen-destLen;

  do
  {
    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    *dest++=(unsigned short)pa;

    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    *dest++=(unsigned short)pa;
  }
  while (dest<end);
  
  return 0;
}

// same thing, only reversed (dest is in pre-decremental mode)
int PicuShrinkReverse(unsigned short *dest,int destLen,unsigned short *src,int srcLen)
{
  unsigned short *end=NULL;
  int bias=0,pa=0,sub=0;

  end=dest-destLen;
  sub=srcLen-destLen;

  do
  {
    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    *(--dest)=(unsigned short)pa;

    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    *(--dest)=(unsigned short)pa;
  }
  while (dest>end);
  
  return 0;
}

int PicuMerge(unsigned short *dest,int destLen,unsigned short *src,int srcLen)
{
  unsigned short *end=NULL;
  int bias=0,pa=0,mask=PicuAnd,sub=0;

  end=dest+destLen;
  sub=srcLen-destLen;

  do
  {
    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    pa&=mask; pa+=(*dest)&mask; pa>>=1;
    *dest++=(unsigned short)pa;

    pa=*src++; bias-=sub;
    if (bias<0) { pa+=*src++; pa>>=1; bias+=destLen; }
    pa&=mask; pa+=(*dest)&mask; pa>>=1;
    *dest++=(unsigned short)pa;
  }
  while (dest<end);
  
  return 0;
}

