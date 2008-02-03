
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d8.h>

#include <d3dx8.h>

#include <Pico/Pico.h>

#define PI 3.14159265f

#define RELEASE(x) if (x) x->Release();  x=NULL;

#ifndef __FUNCTION__
#define __FUNCTION__ ""
#endif

#define LOGFAIL() dprintf2("fail: %s %s:%i\n", __FUNCTION__, __FILE__, __LINE__)


// Emu.cpp
extern unsigned short *EmuScreen;
extern int EmuWidth,EmuHeight;
extern RECT EmuScreenRect;
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

// Loop.cpp
extern char LoopQuit,LoopWait,LoopWaiting;
extern int LoopMode;

int LoopInit();
void LoopExit();
int LoopCode();
//extern "C" int dprintf(char *format, ...);
extern "C" int dprintf2(char *format, ...);

// Main.cpp
extern char *romname;
extern HWND FrameWnd;
extern RECT FrameRectMy;
extern int MainWidth,MainHeight;
extern int lock_to_1_1;
extern void error(char *text);

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

// TexScreen.cpp
extern IDirect3DTexture8 *TexScreen;
extern int TexWidth,TexHeight;
int TexScreenInit();
void TexScreenExit();
int TexScreenSwizzle();
int TexScreenLinear();
