
#include "app.h"

extern "C" {
struct Pico
{
  unsigned char ram[0x10000];  // 0x00000 scratch ram
  unsigned short vram[0x8000]; // 0x10000
  unsigned char zram[0x2000];  // 0x20000 Z80 ram
  unsigned char ioports[0x10];
  unsigned int pad[0x3c];      // unused
  unsigned short cram[0x40];   // 0x22100
  unsigned short vsram[0x40];  // 0x22180

  unsigned char *rom;          // 0x22200
  unsigned int romsize;        // 0x22204

//  struct PicoMisc m;
//  struct PicoVideo video;
};
  extern struct Pico Pico;
}

unsigned short *EmuScreen=NULL;
extern "C" unsigned short *framebuff=NULL;
int EmuWidth=0,EmuHeight=0;
static int frame=0;
static int EmuScan(unsigned int num, void *sdata);

int EmuInit()
{
  int len=0;

//  PicoOpt=-1;
//  PsndRate=44100; PsndLen=DSoundSeg;

  PicoInit();

  // Allocate screen:
  EmuWidth=320; EmuHeight=224;
  len=EmuWidth*EmuHeight; len<<=1;
  EmuScreen=(unsigned short *)malloc(len); if (EmuScreen==NULL) return 1;
  framebuff=(unsigned short *)malloc((8+320)*(8+224+8)*2);
  memset(EmuScreen,0,len);

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

  do { *pd++=(unsigned short)PicoCram(*ps++); } while (pd<end);
  
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

  // rendermode2
  if(PicoOpt&0x10) {
	unsigned short *pd=EmuScreen;
	unsigned char  *ps=(unsigned char*)framebuff+328*8;

	unsigned short palHigh[0x40];
	for(int i = 0; i < 0x40; i++)
	  palHigh[i]=(unsigned short)PicoCram(Pico.cram[i]);

    for(int y=0; y < 224; y++) {
	  ps+=8;
	  for(int x=0; x < 320; x++)
		*pd++=palHigh[*ps++];
	}
  }

  return 0;
}
