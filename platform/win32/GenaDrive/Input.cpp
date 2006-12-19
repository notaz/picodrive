
#include "app.h"
#include <commdlg.h>

extern char *romname;
extern unsigned char *rom_data;
extern unsigned int rom_size;
extern int fastForward;
extern int frameStep;
extern int emu_frame;

struct Input Inp;

// --------------------- XBox Input -----------------------------
#ifdef _XBOX
static HANDLE GamePad=NULL;
static XINPUT_STATE Pad;

static int DeadZone(short *paxis)
{
  int zone=0x2000;
  int a=*paxis;

       if (a<-zone) a+=zone;
  else if (a> zone) a-=zone; else a=0;

  *paxis=(short)a;
  return 0;
}

static int DeviceRead()
{
  int but=0,a=0;

  memset(Inp.axis,  0,sizeof(Inp.axis));
  memset(Inp.button,0,sizeof(Inp.button));

  if (GamePad==NULL) GamePad=XInputOpen(XDEVICE_TYPE_GAMEPAD,0,XDEVICE_NO_SLOT,NULL);
  if (GamePad==NULL) return 1;

  // Read XBox joypad:
  XInputGetState(GamePad,&Pad);

  // Get analog axes:
  Inp.axis[0]=Pad.Gamepad.sThumbLX;
  Inp.axis[1]=Pad.Gamepad.sThumbLY;
  Inp.axis[2]=Pad.Gamepad.sThumbRX;
  Inp.axis[3]=Pad.Gamepad.sThumbRY;

  for (a=0;a<4;a++) DeadZone(Inp.axis+a);

  // Get digital buttons:
  but=Pad.Gamepad.wButtons;
  for (a=0;a<8;a++)
  {
    if (but&(1<<a)) Inp.button[a]=0xff;
  }

  // Get analog buttons:
  memcpy(Inp.button+8, Pad.Gamepad.bAnalogButtons, 8);

  return 0;
}

#endif

// --------------------- Windows  Input -----------------------------

#ifndef _XBOX

static int DeviceRead()
{
  int push=0x6000;
  int axis[]={0,0,0,0};
  int i=0;

  memset(Inp.axis,  0,sizeof(Inp.axis));
  memset(Inp.button,0,sizeof(Inp.button));

  if (GetForegroundWindow()!=FrameWnd) return 1;

  if (GetAsyncKeyState(VK_LEFT )) axis[0]-=push;
  if (GetAsyncKeyState(VK_RIGHT)) axis[0]+=push;
  if (GetAsyncKeyState(VK_DOWN )) axis[1]-=push;
  if (GetAsyncKeyState(VK_UP   )) axis[1]+=push;
  for (i=0;i<4;i++) Inp.axis[i]=(short)axis[i];

  if (GetAsyncKeyState(VK_RETURN)) Inp.button[4]=0xff; // Start
  //if (GetAsyncKeyState(VK_ESCAPE)) Inp.button[7]=0xff; // Right thumb

  if (GetAsyncKeyState('Z')) Inp.button[10]=0xff;
  if (GetAsyncKeyState('X')) Inp.button[ 8]=0xff;
  if (GetAsyncKeyState('C')) Inp.button[ 9]=0xff;

  if (GetAsyncKeyState('A')) Inp.button[13]=0xff;
  if (GetAsyncKeyState('S')) Inp.button[12]=0xff;
  if (GetAsyncKeyState('D')) Inp.button[11]=0xff;
  if (GetAsyncKeyState('F')) Inp.button[14]=0xff;

  static int sblobked = 0;
  if(!sblobked && GetAsyncKeyState(VK_F6)) {
	FILE *PmovFile;
    romname[strlen(romname)-3] = 0;
    strcat(romname, "mds");
	PmovFile = fopen(romname, "wb");
	if(PmovFile) {
	  PmovState(5, PmovFile);
	  fclose(PmovFile);
	}
	sblobked = 1;
  }
  else if(!sblobked && GetAsyncKeyState(VK_F9)) {
	FILE *PmovFile;
    romname[strlen(romname)-3] = 0;
    strcat(romname, "mds");
	PmovFile = fopen(romname, "rb");
	if(PmovFile) {
	  PmovState(6, PmovFile);
	  fclose(PmovFile);
	}
	sblobked = 1;
  }
  else if(!sblobked && GetAsyncKeyState(VK_TAB)) {
	PicoReset(0);
	sblobked = 1;
	emu_frame = 0;
  }
  else if(!sblobked && GetAsyncKeyState(VK_ESCAPE)) {
	DSoundMute();
    FILE *rom = 0;
    OPENFILENAME of; ZeroMemory(&of, sizeof(OPENFILENAME));
	of.lStructSize = sizeof(OPENFILENAME);
	of.lpstrFilter = "ROMs\0*.smd;*.bin;*.gen\0";
	of.lpstrFile = romname; romname[0] = 0;
	of.nMaxFile = MAX_PATH;
	of.Flags = OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
	GetOpenFileName(&of);
	rom = fopen(romname, "rb");
	DSoundUnMute();
	if(!rom) return 1;
    PicoCartLoad(rom, &rom_data, &rom_size);
    PicoCartInsert(rom_data, rom_size);
	fclose(rom);
	sblobked = 1;
  }
  else if(!sblobked && GetAsyncKeyState(VK_BACK)) {
	if(frameStep) frameStep=0;
	else fastForward^=1;
	sblobked = 1;
  }
  else if(!sblobked && GetAsyncKeyState(VK_OEM_5)) {
	frameStep=3;
	sblobked = 1;
  }
  else
    sblobked = GetAsyncKeyState(VK_F6) | GetAsyncKeyState(VK_F9) |
		GetAsyncKeyState(VK_TAB)  | GetAsyncKeyState(VK_ESCAPE) |
		GetAsyncKeyState(VK_BACK) | GetAsyncKeyState(VK_OEM_5);
  
  return 0;
}

#endif

int InputInit()
{
  memset(&Inp,0,sizeof(Inp));
#ifdef _XBOX
  memset(&Pad,0,sizeof(Pad));
  XInitDevices(0,NULL);
#endif
  return 0;
}

void InputExit()
{
#ifdef _XBOX
  if (GamePad) XInputClose(GamePad);
  GamePad=NULL;
#endif
}

int InputUpdate()
{
  int i=0;
  int push=0x2000;

  DeviceRead(); // Read XBox or PC device 

  // Use left analog for left digital too:
  if (Inp.axis[1]>= push) Inp.button[0]|=0xff; // Up
  if (Inp.axis[1]<=-push) Inp.button[1]|=0xff; // Down
  if (Inp.axis[0]<=-push) Inp.button[2]|=0xff; // Left
  if (Inp.axis[0]>= push) Inp.button[3]|=0xff; // Right

  // Update debounce/time held information:
  for (i=0;i<sizeof(Inp.held);i++)
  {
    if (Inp.held[i]==0)
    {
      if (Inp.button[i]>30) Inp.held[i]=1; // Just pressed
    }
    else
    {
      // Is the button still being held down?
      Inp.held[i]++;
      if (Inp.held[i]>=0x80) Inp.held[i]&=0xbf; // (Keep looping around)

      if (Inp.button[i]<25) Inp.held[i]=0; // No
    }
  }

  // Work out some key repeat values:
  for (i=0;i<sizeof(Inp.repeat);i++)
  {
    char rep=0;
    int held=Inp.held[i];

    if (held==1) rep=1;
    if (held>=0x20 && (held&1)) rep=1;

    Inp.repeat[i]=rep;
  }

  return 0;
}

// Set Lightgun calibration values:
int InputLightCal(int cx,int cy,int ux,int uy)
{
#ifdef _XBOX
  XINPUT_LIGHTGUN_CALIBRATION_OFFSETS cal;

  memset(&cal,0,sizeof(cal));

  cal.wCenterX   =(WORD)cx;
  cal.wCenterY   =(WORD)cy;
  cal.wUpperLeftX=(WORD)ux;
  cal.wUpperLeftY=(WORD)uy;
  XInputSetLightgunCalibration(GamePad,&cal);

#endif

  (void)(cx+cy+ux+uy);

  return 0;
}
