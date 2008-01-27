
#include "app.h"

unsigned short *EmuScreen=NULL;
extern "C" unsigned short *framebuff=NULL;
int EmuWidth=0,EmuHeight=0;
static int frame=0;
static int EmuScan(unsigned int num, void *sdata);
unsigned char *PicoDraw2FB = NULL;

int EmuInit()
{
  int len=0;

  PicoInit();

  // Allocate screen:
  EmuWidth=320; EmuHeight=224;
  len=EmuWidth*EmuHeight; len<<=1;
  EmuScreen=(unsigned short *)malloc(len); if (EmuScreen==NULL) return 1;
  framebuff=(unsigned short *)malloc((8+320)*(8+224+8)*2);
  memset(EmuScreen,0,len);

  PicoDraw2FB = (unsigned char *)framebuff;
  PicoDrawSetColorFormat(1);
  PicoScan=EmuScan;

  return 0;
}

void EmuExit()
{
  //RomFree();
  free(EmuScreen); EmuScreen=NULL; // Deallocate screen
  free(framebuff);
  EmuWidth=EmuHeight=0;

  PicoExit();
}

// Megadrive scanline callback:
static int EmuScan(unsigned int num, void *sdata)
{
  unsigned short *pd=NULL,*end=NULL;
  unsigned short *ps=NULL;

  if (num>=(unsigned int)EmuHeight) return 0;

  // Copy scanline to screen buffer:
  pd=EmuScreen+(num<<8)+(num<<6); end=pd+320;
  ps=(unsigned short *)sdata;

  do { *pd++=*ps++; } while (pd<end);
  
  return 0;
}

int EmuFrame()
{
  char map[12]={0,1,2,3,8,9,10,4,11,12,13,14};  // Joypads, format is UDLR BCAS ZYXM
  int a=0,input=0;
 
  // Set Megadrive buttons:
  for (a=0;a<12;a++)
  {
    int m=map[a];
    if (m>=0) if (Inp.button[m]>30) input|=1<<a;
  }

  PicoPad[0]=input;

  frame++;
  PsndOut=(short *)DSoundNext; PicoFrame(); PsndOut=NULL;

  return 0;
}



int mp3_get_offset(void) // 0-1023
{
  return 0;
}

void mp3_update(int *buffer, int length, int stereo)
{
}

void mp3_start_play(FILE *f, int pos)
{
}

int mp3_get_bitrate(FILE *f, int size)
{
  return -1;
}

