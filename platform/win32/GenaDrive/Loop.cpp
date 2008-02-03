#include "app.h"
//#include "FileMenu.h"

char LoopQuit=0,LoopWait=0,LoopWaiting=0;
static FILE *DebugFile=NULL;
int LoopMode=0;
static void UpdateSound(int len);

int LoopInit()
{
  int ret=0;

  // bits LSb->MSb:
  // enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound;
  // alt_renderer, 6button_gamepad, accurate_timing, accurate_sprites
  PicoOpt=0xbccf;
  PsndRate=44100;

  // Init Direct3D:
  ret=DirectInit(); if (ret) { error("DirectX video init failed"); return 1; }
  InputInit();

  // Init DirectSound:
  //DSoundInit();

  ret=EmuInit(); if (ret) return 1;

  LoopMode=8;
  PicoWriteSound = UpdateSound;
  PicoAutoRgnOrder = 0x184;

  return 0;
}

extern "C" char *debugString();

void LoopExit()
{
  dprintf(debugString());

  EmuExit();
  InputExit();
  DirectExit();

  if (DebugFile) fclose(DebugFile);
  DebugFile=NULL;
}

// ----------------------------------------------------------------

static void UpdateSound(int len)
{
  while (DSoundUpdate() > 0) { Sleep(1); }
  //while (DSoundUpdate()== 0) { }
}

static void PostProcess()
{
  static int lock_to_1_1_prev = 0, is_40_prev = 0;
  int is_40 = PicoGetStat(PS_40_CELL);
  if (lock_to_1_1)
  {
    if (is_40 != is_40_prev || !lock_to_1_1_prev)
      PostMessage(FrameWnd, WM_COMMAND, 0x20000 | (is_40 ? 1100 : 1101), 0);
  }
  if (is_40 != is_40_prev)
  {
    EmuScreenRect.left  = is_40 ?   0 :  32;
    EmuScreenRect.right = is_40 ? 320 : 256+32;
  }
  lock_to_1_1_prev = lock_to_1_1;
  is_40_prev = is_40;
}

int LoopCode()
{

  // Main loop:
  while (!LoopQuit)
  {
    if (LoopWait)
    {
      DSoundExit();
      while (!LoopQuit && LoopWait) { LoopWaiting=1; Sleep(100); }
      if (LoopQuit) break;
      DSoundInit();
    }
    InputUpdate();

    DirectClear(0);
    EmuFrame();
    PostProcess();
    DirectScreen();
    DirectPresent();
//      UpdateSound();
  }
  DSoundExit();

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

