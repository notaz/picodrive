
#include <stdio.h>

#ifdef _XBOX
#include <xtl.h>
#endif

#ifndef _XBOX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d8.h>
#endif

#include <d3dx8.h>

#include "Pico.h"

#define PI 3.14159265f

#define RELEASE(x) if (x) x->Release();  x=NULL;

#ifdef _XBOX
#define HOME "d:\\"
#else
#define HOME ".\\"
#endif

// Emu.cpp
extern unsigned short *EmuScreen;
extern int EmuWidth,EmuHeight;
int EmuInit();
void EmuExit();
int EmuRomLoad(char *name);
int EmuFrame();

// Input.cpp
struct Input
{
  short axis[4];
  unsigned char button[16];
  unsigned char held[16]; // How long has the button been held
  char repeat[16]; // Auto-repeat
};
extern struct Input Inp;
int InputInit();
void InputExit();
int InputUpdate();
int InputLightCal(int cx,int cy,int ux,int uy);

// LightCal.cpp
int LightCalReset();
int LightCalUpdate();
int LightCalRender();

// Loop.cpp
void preLoopInit();
extern char LoopQuit;
extern int LoopMode;

int LoopInit();
void LoopExit();
int LoopCode();

// Main.cpp
extern HWND FrameWnd;
extern int MainWidth,MainHeight;
extern char AppName[];
extern "C" int dprintf(char *format, ...);

// Rom.cpp
extern unsigned char *RomData;
extern int RomLen;
extern char RomName[260];
int RomLoad();
void RomFree();

// --------------------------------------------
// Direct.cpp
extern IDirect3DDevice8 *Device;
extern IDirect3DSurface8 *DirectBack; // Back Buffer
int DirectInit();
int DirectClear(unsigned int colour);
int DirectScreen();
int DirectPresent();
void DirectExit();

// DSound.cpp:
int DSoundInit();
void DSoundExit();
int DSoundUpdate();
extern short *DSoundNext; // Buffer for next sound data to put in loop
//extern int DSoundSeg; // Seg length in samples
void DSoundMute();
void DSoundUnMute();

// Font.cpp
int FontInit();
void FontExit();
int FontSetColour(unsigned int colour);
int FontText(WCHAR *,int,int);

// TexScreen.cpp
extern IDirect3DTexture8 *TexScreen;
extern int TexWidth,TexHeight;
int TexScreenInit();
void TexScreenExit();
int TexScreenSwizzle();
int TexScreenLinear();
