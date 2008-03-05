#include "app.h"
#include "version.h"
#include <crtdbg.h>
#include <commdlg.h>

char *romname=NULL;
HWND FrameWnd=NULL;
RECT FrameRectMy;
int lock_to_1_1 = 1;

int MainWidth=720,MainHeight=480;

static HMENU mdisplay = 0;
static unsigned char *rom_data = NULL;

static void UpdateRect()
{
  WINDOWINFO wi;
  memset(&wi, 0, sizeof(wi));
  wi.cbSize = sizeof(wi);
  GetWindowInfo(FrameWnd, &wi);
  FrameRectMy = wi.rcClient;
}

static void LoadROM(const char *cmdpath)
{
  static char rompath[MAX_PATH] = { 0, };
  unsigned char *rom_data_new = NULL;
  unsigned int rom_size = 0;
  pm_file *rom = NULL;
  int oldwait=LoopWait;
  int i, ret;

  if (cmdpath) {
    strcpy(rompath, cmdpath + (cmdpath[0] == '\"' ? 1 : 0));
    if (rompath[strlen(rompath)-1] == '\"') rompath[strlen(rompath)-1] = 0;
    if (strlen(rompath) > 4) rom = pm_open(rompath);
  }

  if (!rom) {
    OPENFILENAME of; ZeroMemory(&of, sizeof(OPENFILENAME));
    of.lStructSize = sizeof(OPENFILENAME);
    of.lpstrFilter = "ROMs\0*.smd;*.bin;*.gen;*.zip\0";
    of.lpstrFile = rompath; rompath[0] = 0;
    of.nMaxFile = MAX_PATH;
    of.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
    of.hwndOwner = FrameWnd;
    if (!GetOpenFileName(&of)) return;
    rom = pm_open(rompath);
    if (!rom) { error("failed to open ROM"); return; }
  }

  ret=PicoCartLoad(rom, &rom_data_new, &rom_size);
  pm_close(rom);
  if (ret) {
    error("failed to load ROM");
    return;
  }

  // halt the work thread..
  // just a hack, should've used proper sync. primitives here, but who will use this emu anyway..
  LoopWaiting=0;
  LoopWait=1;
  for (i = 0; LoopWaiting == 0 && i < 10; i++) Sleep(100);

  PicoCartInsert(rom_data_new, rom_size);

  if (rom_data) free(rom_data);
  rom_data = rom_data_new;
  romname = rompath;
  LoopWait=0;
}

static int rect_widths[4]  = { 320, 256, 640, 512 };
static int rect_heights[4] = { 224, 224, 448, 448 };

// Window proc for the frame window:
static LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  int i;
  switch (msg)
  {
    case WM_CLOSE:   PostQuitMessage(0); return 0;
    case WM_DESTROY: FrameWnd=NULL; break; // Blank handle
    case WM_SIZE:
    case WM_MOVE:
    case WM_SIZING:  UpdateRect(); break;
    case WM_COMMAND:
      switch (LOWORD(wparam))
      {
        case 1000: LoadROM(NULL); break;
        case 1001: PostQuitMessage(0); return 0;
        case 1100:
        case 1101:
        case 1102:
        case 1103:
          LoopWait=1; // another sync hack
          for (i = 0; !LoopWaiting && i < 10; i++) Sleep(10);
          FrameRectMy.right  = FrameRectMy.left + rect_widths[wparam&3];
          FrameRectMy.bottom = FrameRectMy.top  + rect_heights[wparam&3];
          AdjustWindowRect(&FrameRectMy, WS_OVERLAPPEDWINDOW, 1);
          MoveWindow(hwnd, FrameRectMy.left, FrameRectMy.top,
            FrameRectMy.right-FrameRectMy.left, FrameRectMy.bottom-FrameRectMy.top, 1);
          UpdateRect();
          if (HIWORD(wparam) == 0) { // locally sent
            lock_to_1_1=0;
            CheckMenuItem(mdisplay, 1104, MF_UNCHECKED);
          }
          if (rom_data != NULL) LoopWait=0;
          return 0;
        case 1104:
          lock_to_1_1=!lock_to_1_1;
          CheckMenuItem(mdisplay, 1104, lock_to_1_1 ? MF_CHECKED : MF_UNCHECKED);
          return 0;
        case 1200: break;
        case 1300:
          MessageBox(FrameWnd, "PicoDrive v" VERSION " (c) notaz, 2006-2008\n"
              "SVP demo edition\n\n"
              "Credits:\n"
              "fDave: base code of PicoDrive, GenaDrive (the frontend)\n"
              "Chui: Fame/C\n"
              "NJ: CZ80\n"
              "MAME devs: YM2612 and SN76496 cores\n"
              "Stéphane Dallongeville: Gens code, base of Fame/C (C68K), CZ80\n"
              "Tasco Deluxe: SVP RE work\n"
              "Pierpaolo Prazzoli: info about SSP16 chips\n",
              "About", 0);
          return 0;
      }
      break;
  }

  return DefWindowProc(hwnd,msg,wparam,lparam);
}

static int FrameInit()
{
  WNDCLASS wc;
  RECT rect={0,0,0,0};
  HMENU mmain, mfile;
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
  AdjustWindowRect(&rect,style,1);
  width =rect.right-rect.left;
  height=rect.bottom-rect.top;

  // Place window in the centre of the screen:
  SystemParametersInfo(SPI_GETWORKAREA,0,&rect,0);
  left=rect.left+rect.right;
  top=rect.top+rect.bottom;

  left-=width; left>>=1;
  top-=height; top>>=1;

  // Create menu:
  mfile = CreateMenu();
  InsertMenu(mfile, -1, MF_BYPOSITION|MF_STRING, 1000, "&Load ROM");
  InsertMenu(mfile, -1, MF_BYPOSITION|MF_STRING, 1001, "E&xit");
  mdisplay = CreateMenu();
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1100, "320x224");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1101, "256x224");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1102, "640x448");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1103, "512x448");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1104, "Lock to 1:1");
  mmain = CreateMenu();
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT_PTR) mfile, "&File");
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT_PTR) mdisplay, "&Display");
//  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, 1200, "&Config");
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING, 1300, "&About");

  // Create the window:
  FrameWnd=CreateWindow(wc.lpszClassName,"PicoDrive " VERSION,style|WS_VISIBLE,
    left,top,width,height,NULL,mmain,NULL,NULL);

  CheckMenuItem(mdisplay, 1104, lock_to_1_1 ? MF_CHECKED : MF_UNCHECKED);
  ShowWindow(FrameWnd, SW_NORMAL);
  UpdateWindow(FrameWnd);
  UpdateRect();

  return 0;
}

// --------------------

static DWORD WINAPI ThreadCode(void *)
{
  LoopCode();
  return 0;
}

int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR cmdline,int)
{
  MSG msg;
  int ret=0;
  DWORD tid=0;
  HANDLE thread=NULL;

  FrameInit();
  ret=LoopInit(); if (ret) goto end0;

  // Make another thread to run LoopCode():
  LoopQuit=0;
  LoopWait=1; // wait for ROM to be loaded
  thread=CreateThread(NULL,0,ThreadCode,NULL,0,&tid);

  LoadROM(cmdline);

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

end0:
  LoopExit();
  DestroyWindow(FrameWnd);

  _CrtDumpMemoryLeaks();
  return 0;
}

extern void error(char *text)
{
  MessageBox(FrameWnd, text, "Error", 0);
}

