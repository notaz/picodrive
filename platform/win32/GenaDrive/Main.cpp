
#include "app.h"
#include <crtdbg.h>
#include <commdlg.h>

char *romname;
HWND FrameWnd=NULL;

int MainWidth=720,MainHeight=480;

char AppName[]="GenaDrive";

#ifdef STARSCREAM
  extern "C" int SekReset();
#endif

// ------------------------------------ XBox Main ------------------------------------------
#ifdef _XBOX

static int MainCode()
{
  int ret=0;

  ret=LoopInit(); if (ret) { LoopExit(); return 1; }

  LoopQuit=0; LoopCode();
  LoopExit();

  return 0;
}

int __cdecl main()
{
  LD_LAUNCH_DASHBOARD launch;

  MainCode();

  // Go back to dashboard:
  memset(&launch,0,sizeof(launch));
  launch.dwReason=XLD_LAUNCH_DASHBOARD_MAIN_MENU;
  XLaunchNewImage(NULL,(LAUNCH_DATA *)&launch);
}
#endif

// ----------------------------------- Windows Main ----------------------------------------
#ifndef _XBOX
// Window proc for the frame window:
static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  if (msg==WM_CLOSE) { PostQuitMessage(0); return 0; }
  if (msg==WM_DESTROY) FrameWnd=NULL; // Blank handle

  return DefWindowProc(hwnd,msg,wparam,lparam);
}

static int FrameInit()
{
  WNDCLASS wc;
  RECT rect={0,0,0,0};
  int style=0;
  int left=0,top=0,width=0,height=0;

  memset(&wc,0,sizeof(wc));

  // Register the window class:
  wc.lpfnWndProc=WndProc;
  wc.hInstance=GetModuleHandle(NULL);
  wc.hCursor=LoadCursor(NULL,IDC_ARROW);
  wc.hbrBackground=CreateSolidBrush(0);
  wc.lpszClassName="MainFrame";
  RegisterClass(&wc);

  rect.right =320;//MainWidth;
  rect.bottom=224;//MainHeight;

  // Adjust size of windows based on borders:
  style=WS_OVERLAPPEDWINDOW;
  AdjustWindowRect(&rect,style,0);
  width =rect.right-rect.left;
  height=rect.bottom-rect.top;

  // Place window in the centre of the screen:
  SystemParametersInfo(SPI_GETWORKAREA,0,&rect,0);
  left=rect.left+rect.right;
  top=rect.top+rect.bottom;

  left-=width; left>>=1;
  top-=height; top>>=1;

  // Create the window:
  FrameWnd=CreateWindow(wc.lpszClassName,AppName,style|WS_VISIBLE,
    left,top,width,height,NULL,NULL,NULL,NULL);

  return 0;
}

// --------------------

static DWORD WINAPI ThreadCode(void *)
{
  LoopCode();
  return 0;
}

// starscream needs this
unsigned char *rom_data = 0;
unsigned int rom_size = 0;

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR cmdline,int)
{
  MSG msg;
  int ret=0;
  DWORD tid=0;
  HANDLE thread=NULL;

  // notaz: load rom
  static char rompath[MAX_PATH]; rompath[0] = 0;
  strcpy(rompath, cmdline + (cmdline[0] == '\"' ? 1 : 0));
  if(rompath[strlen(rompath)-1] == '\"') rompath[strlen(rompath)-1] = 0;

  FILE *rom = 0;
  if(strlen(rompath) > 4) rom = fopen(rompath, "rb");
  if(!rom) {
    OPENFILENAME of; ZeroMemory(&of, sizeof(OPENFILENAME));
	of.lStructSize = sizeof(OPENFILENAME);
	of.lpstrFilter = "ROMs\0*.smd;*.bin;*.gen\0";
	of.lpstrFile = rompath; rompath[0] = 0;
	of.nMaxFile = MAX_PATH;
	of.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	if(!GetOpenFileName(&of)) return 1;
	rom = fopen(rompath, "rb");
	if(!rom) return 1;
  }
  romname = rompath;

  if(PicoCartLoad(rom, &rom_data, &rom_size)) {
	//RDebug::Print(_L("PicoCartLoad() failed."));
	//goto cleanup;
  }

  FrameInit();
  ret=LoopInit(); if (ret) { LoopExit(); return 1; }

  PicoCartInsert(rom_data, rom_size);

  // only now we got the mode (pal/ntsc), so init sound now
  DSoundInit();

  preLoopInit();

  // Make another thread to run LoopCode():
  LoopQuit=0;
  thread=CreateThread(NULL,0,ThreadCode,NULL,0,&tid);

  // Main window loop:
  for (;;)
  {
    GetMessage(&msg,NULL,0,0);
    if (msg.message==WM_QUIT) break;

    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Signal thread to quit and wait for it to exit:
  LoopQuit=1; WaitForSingleObject(thread,5000);
  CloseHandle(thread); thread=NULL;

  LoopExit();
  DestroyWindow(FrameWnd);

  free(rom_data);

  _CrtDumpMemoryLeaks();
  return 0;
}
#endif

