#include "app.h"
//#include "FileMenu.h"

char LoopQuit=0;
static FILE *DebugFile=NULL;
int LoopMode=0;
static void UpdateSound(int len);

int LoopInit()
{
  int ret=0;

  // bits LSb->MSb:
  // enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound;
  // alt_renderer, 6button_gamepad, accurate_timing, accurate_sprites
  PicoOpt=0xbcc7;
  PsndRate=44100;

  // Init Direct3D:
  ret=DirectInit(); if (ret) { error("DirectX video init failed"); return 1; }
  InputInit();

  // Init DirectSound:
  //DSoundInit();

  ret=EmuInit(); if (ret) return 1;
  //FileMenu.init();

  LoopMode=8;
  PicoWriteSound = UpdateSound;

  return 0;
}

extern "C" char *debugString();

void LoopExit()
{
  dprintf(debugString());

  //FileMenu.exit();
  EmuExit();
  DSoundExit(); PsndLen=0;
  InputExit();
  DirectExit();

  if (DebugFile) fclose(DebugFile);
  DebugFile=NULL;
}

// ----------------------------------------------------------------

static int DoGame()
{
  EmuFrame();

  if (Inp.held[7]==1) LoopMode=2; // Right thumb = Toggle Menu

  return 0;
}
// ----------------------------------------------------------------

/*
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
*/

// ----------------------------------------------------------------

static int ModeUpdate()
{
  if (Inp.held[14] && Inp.held[15] && Inp.held[12]==1) LoopQuit=1; // L+R+black to quit:
  if (Inp.button[4]>30 && Inp.button[5]>30) LoopQuit=1; // Start and back to quit

  if (LoopMode==8) { DoGame(); return 0; }

//  if (DSoundNext) memset(DSoundNext,0,PsndLen<<2);

//  if (LoopMode==2) { FileMenu.scan(); LoopMode++; return 0; }
//  if (LoopMode==3) { MenuUpdate(); return 0; }
//  if (LoopMode==4) { LightCalUpdate(); return 0; }

  LoopMode=2; // Unknown mode, go to rom menu
  return 0;
}


static int ModeRender()
{
  DirectScreen();
//  if (LoopMode==3) MenuRender();
//  if (LoopMode==4) LightCalRender();

  return 0;
}

static void UpdateSound(int len)
{
  while (DSoundUpdate() > 0) { Sleep(1); }
  while (DSoundUpdate()== 0) { }
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

#if 0
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
#endif

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

