#include "app.h"
#include "version.h"
#include <crtdbg.h>
#include <commdlg.h>

char *romname=NULL;
HWND FrameWnd=NULL;
RECT FrameRectMy;
int lock_to_1_1 = 1;
static HWND PicoSwWnd=NULL, PicoPadWnd=NULL;

int MainWidth=720,MainHeight=480;

static HMENU mdisplay = 0, mpicohw = 0;
static int rom_loaded = 0;

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

  PicoCartUnload();
  PicoCartInsert(rom_data_new, rom_size);

  rom_loaded = 1;
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
        case 1001: PicoReset(); return 0;
        case 1002: PostQuitMessage(0); return 0;
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
          if (rom_loaded) LoopWait=0;
          return 0;
        case 1104:
          lock_to_1_1=!lock_to_1_1;
          CheckMenuItem(mdisplay, 1104, lock_to_1_1 ? MF_CHECKED : MF_UNCHECKED);
          return 0;
        case 1210:
        case 1211:
          i = IsWindowVisible((LOWORD(wparam)&1) ? PicoPadWnd : PicoSwWnd);
          i = !i;
          ShowWindow((LOWORD(wparam)&1) ? PicoPadWnd : PicoSwWnd, i ? SW_SHOWNA : SW_HIDE);
          CheckMenuItem(mpicohw, LOWORD(wparam), i ? MF_CHECKED : MF_UNCHECKED);
          return 0;
        case 1220:
        case 1221:
        case 1222:
        case 1223:
        case 1224:
        case 1225:
        case 1226:
          PicoPicohw.page = LOWORD(wparam) % 10;
          for (i = 0; i < 7; i++)
            CheckMenuItem(mpicohw, 1220 + i, MF_UNCHECKED);
          CheckMenuItem(mpicohw, 1220 + PicoPicohw.page, MF_CHECKED);
          return 0;
        case 1300:
          MessageBox(FrameWnd, "PicoDrive v" VERSION " (c) notaz, 2006-2008\n"
              "SVP and Pico demo edition\n\n"
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

static LRESULT CALLBACK PicoSwWndProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  switch (msg)
  {
    case WM_CLOSE: return 0;
    case WM_DESTROY: PicoSwWnd=NULL; break;
  }

  return DefWindowProc(hwnd,msg,wparam,lparam);
}

static LRESULT CALLBACK PicoPadWndProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam)
{
  switch (msg)
  {
    case WM_CLOSE: return 0;
    case WM_DESTROY: PicoPadWnd=NULL; break;
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
  wc.lpszClassName="PicoMainFrame";
  RegisterClass(&wc);

  wc.lpszClassName="PicoSwWnd";
  wc.lpfnWndProc=PicoSwWndProc;
  RegisterClass(&wc);

  wc.lpszClassName="PicoPadWnd";
  wc.lpfnWndProc=PicoPadWndProc;
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
  InsertMenu(mfile, -1, MF_BYPOSITION|MF_STRING, 1001, "&Reset");
  InsertMenu(mfile, -1, MF_BYPOSITION|MF_STRING, 1002, "E&xit");
  mdisplay = CreateMenu();
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1100, "320x224");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1101, "256x224");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1102, "640x448");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1103, "512x448");
  InsertMenu(mdisplay, -1, MF_BYPOSITION|MF_STRING, 1104, "Lock to 1:1");
  mpicohw = CreateMenu();
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1210, "Show &Storyware");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1211, "Show &Drawing pad");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_SEPARATOR, 0, NULL);
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1220, "Title page (&0)");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1221, "Page &1");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1222, "Page &2");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1223, "Page &3");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1224, "Page &4");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1225, "Page &5");
  InsertMenu(mpicohw, -1, MF_BYPOSITION|MF_STRING, 1226, "Page &6");
  mmain = CreateMenu();
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT_PTR) mfile,    "&File");
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT_PTR) mdisplay, "&Display");
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT_PTR) mpicohw,  "&Pico");
//  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, 1200, "&Config");
  InsertMenu(mmain, -1, MF_BYPOSITION|MF_STRING, 1300, "&About");

  // Create the window:
  FrameWnd=CreateWindow("PicoMainFrame","PicoDrive " VERSION,style|WS_VISIBLE,
    left,top,width,height,NULL,mmain,NULL,NULL);

  CheckMenuItem(mdisplay, 1104, lock_to_1_1 ? MF_CHECKED : MF_UNCHECKED);
  ShowWindow(FrameWnd, SW_NORMAL);
  UpdateWindow(FrameWnd);
  UpdateRect();

  // create Pico windows
  style = WS_OVERLAPPED|WS_CAPTION|WS_BORDER;
  rect.left=rect.top=0;
  rect.right =320;
  rect.bottom=224;

  AdjustWindowRect(&rect,style,1);
  width =rect.right-rect.left;
  height=rect.bottom-rect.top;

  left += 326;
  PicoSwWnd=CreateWindow("PicoSwWnd","Storyware",style,
    left,top,width,height,FrameWnd,NULL,NULL,NULL);

  top += 266;
  PicoPadWnd=CreateWindow("PicoPadWnd","Drawing Pad",style,
    left,top,width,height,FrameWnd,NULL,NULL,NULL);

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

