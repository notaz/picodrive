#include "app.h"
#include "FileMenu.h"

// sram
struct PicoSRAM
{
  unsigned char *data; // actual data
  unsigned int start;  // start address in 68k address space
  unsigned int end;
  unsigned char resize; // 1=SRAM size changed and needs to be reallocated on PicoReset
  unsigned char reg_back; // copy of Pico.m.sram_reg to set after reset
  unsigned char changed;
  unsigned char pad;
};

extern "C" PicoSRAM SRam;
extern char *romname;
int fastForward=0;
int frameStep=0;

char LoopQuit=0;
static FILE *DebugFile=NULL;
int LoopMode=0;
static void UpdateSound();

int LoopInit()
{
  int ret=0;

  // bits LSb->MSb:
  // enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound;
  // alt_renderer, 6button_gamepad, accurate_timing, accurate_sprites
  PicoOpt=0x1f;
  PsndRate=44100;
  //PsndLen=PsndRate/60;   // calculated later by pico itself

  // Init Direct3D:
  ret=DirectInit(); if (ret) return 1;
  InputInit();

  // Init DirectSound:
  //DSoundInit();

  ret=EmuInit(); if (ret) return 1;
  FileMenu.init();

  LoopMode=8;
  PicoWriteSound = UpdateSound;

  return 0;
}

void preLoopInit()
{
  romname[strlen(romname)-3] = 0;
  strcat(romname, "srm");
  int sram_size = SRam.end-SRam.start+1;
  if(SRam.reg_back & 4) sram_size=0x2000;
  FILE *f = fopen(romname, "rb");
  if(f && SRam.data)
    fread(SRam.data, 1, sram_size, f);
  if(f) fclose(f);
}

extern "C" char *debugString();

void LoopExit()
{
  dprintf(debugString());

  romname[strlen(romname)-3] = 0;
  strcat(romname, "srm");
  int sram_size = SRam.end-SRam.start+1;
  if(SRam.reg_back & 4) sram_size=0x2000;
  for(; sram_size > 0; sram_size--)
	if(SRam.data[sram_size-1]) break;
  if(sram_size) {
    FILE *f = fopen(romname, "wb");
    if(f) {
      fwrite(SRam.data, 1, sram_size, f);
      fclose(f);
	}
  }

  FileMenu.exit();
  EmuExit();
  DSoundExit(); PsndLen=0;
  InputExit();
  DirectExit();

  if (DebugFile) fclose(DebugFile);
  DebugFile=NULL;
}

// ----------------------------------------------------------------

int emu_frame = 0;

static int DoGame()
{
  if(fastForward) { PicoSkipFrame+=1; PicoSkipFrame&=7; }
  else PicoSkipFrame=0;

  if(frameStep==1)      return 0;
  else if(frameStep==3) frameStep=1;

  EmuFrame();
  emu_frame++;

  if (Inp.held[7]==1) LoopMode=2; // Right thumb = Toggle Menu

  return 0;
}
// ----------------------------------------------------------------

static int MenuUpdate()
{
  int delta=0;

  if (Inp.repeat[0]) delta-=0x100;
  if (Inp.repeat[1]) delta+=0x100;

  if (Inp.button[14]>30) delta-=Inp.button[14]-30;
  if (Inp.button[15]>30) delta+=Inp.button[15]-30;

  if (delta) FileMenu.scroll(delta);

  if (Inp.held[8]==1 || Inp.held[10]==1 || Inp.held[4]==1) // A, X or Start
  {
    //RomFree();
    //FileMenu.getFilePath(RomName);
    //RomLoad();
    //LoopMode=8; // Go to game
  }

  if (Inp.held[7]==1) LoopMode=8; // Right thumb = Toggle Menu

  return 0;
}

static int MenuRender()
{
  WCHAR text[80]={0};
  wsprintfW(text,L"%.40S v%x.%.3x",AppName,PicoVer>>12,PicoVer&0xfff);
  FontSetColour(0x60c0ff);
  FontText(text,64,48);

  FileMenu.render();

  return 0;
}

// ----------------------------------------------------------------

static int ModeUpdate()
{
  if (Inp.held[14] && Inp.held[15] && Inp.held[12]==1) LoopQuit=1; // L+R+black to quit:
  if (Inp.button[4]>30 && Inp.button[5]>30) LoopQuit=1; // Start and back to quit

  if (LoopMode==8) { DoGame(); return 0; }

  if (DSoundNext) memset(DSoundNext,0,PsndLen<<2);

  if (LoopMode==2) { FileMenu.scan(); LoopMode++; return 0; }
  if (LoopMode==3) { MenuUpdate(); return 0; }
  if (LoopMode==4) { LightCalUpdate(); return 0; }

  LoopMode=2; // Unknown mode, go to rom menu
  return 0;
}


static int ModeRender()
{
  DirectScreen();
  if (LoopMode==3) MenuRender();
  if (LoopMode==4) LightCalRender();

  return 0;
}

static void UpdateSound()
{
  if(fastForward) return;
  while (DSoundUpdate()) { Sleep(1); }
  while (DSoundUpdate()==0) { }
}

int LoopCode()
{

  // Main loop:
  while (!LoopQuit)
  {
    InputUpdate();

    DirectClear(0);
    ModeUpdate();
    ModeRender();
    DirectPresent();
//      UpdateSound();
  }

  return 0;
}

// -------------------------------------------------------------------------------------

extern "C" int dprintf(char *format, ...)
{
  char *name=NULL;
  va_list val=NULL;

#ifdef _XBOX
  name="d:\\zout.txt";
#else
  name="zout.txt";
#endif

  if (DebugFile==NULL) DebugFile=fopen(name,"wt");
  if (DebugFile==NULL) return 1;

  fprintf(DebugFile, "%05i: ", emu_frame);
  va_start(val,format);
  vfprintf(DebugFile,format,val);
  fprintf(DebugFile, "\n");
  fflush(DebugFile);

  va_end(val);
  return 0;
}

extern "C" int dprintf2(char *format, ...)
{
  char str[512];
  va_list val=NULL;

  va_start(val,format);
  vsprintf(str,format,val);
  va_end(val);
  OutputDebugString(str);

  return 0;
}
