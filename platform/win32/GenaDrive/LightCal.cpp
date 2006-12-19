
#include "app.h"

struct Target
{
  int sx,sy; // Onscreen coordinates
  int dx,dy; // Device values
};

struct Target Targ[2]=
{
  {0,0, 0,0},
  {0,0, 0,0}
};
static int LightState=0;

struct Calib
{
  float ax,bx;
  float ay,by;
};
static struct Calib Cal={0.0f,0.0f,0.0f,0.0f};

int LightCalReset()
{
  LightState=0;

  memset(Targ,0,sizeof(Targ));
  Targ[0].sx=MainWidth >>1;
  Targ[0].sy=MainHeight>>1;
  Targ[1].sy=Targ[0].sy-MainHeight*61/160;
  Targ[1].sx=Targ[0].sx-MainWidth *61/160;
  return 0;
}

int LightCalUpdate()
{
  int i=0;
  struct Target *pt=NULL;

  if (Inp.held[4]==1) LoopMode=3;

  if (Inp.held[8]==1)
  {
    i=LightState&1;
    pt=Targ+i;

    pt->dx=Inp.axis[0];
    pt->dy=Inp.axis[1];

    if (i==1)
    {
      int num=0,den=0;

      // rx= a + b*x - work out a and b:
      num=Targ[0].sx-Targ[1].sx;
      den=Targ[0].dx-Targ[1].dx;
      if (den) Cal.bx=(float)num/(float)den;
      Cal.ax=(float)Targ[0].sx-Cal.bx*(float)Targ[0].dx;

      num=Targ[0].sy-Targ[1].sy;
      den=Targ[0].dy-Targ[1].dy;
      if (den) Cal.by=(float)num/(float)den;
      Cal.ay=(float)Targ[0].sy-Cal.by*(float)Targ[0].dy;
    }

    LightState++;
  }

  return 0;
}

int LightCalRender()
{
  int i=0;
  struct Target *pt=NULL;
  float fx=0.0f,fy=0.0f;

  DirectClear(0xffffff);

  WCHAR text[80]={0};
  wsprintfW(text,L"LightGun Calibration");
  FontSetColour(0x0000ff);
  FontText(text,240,48);

  wsprintfW(text,L"Start to quit, B to call InputLightCal");
  FontSetColour(0x004000);
  FontText(text,64,120);

  i=LightState&1;
  pt=Targ+i;
  FontSetColour(0);
  FontText(L"X", pt->sx-8, pt->sy-12);

  fx=Cal.ax+Cal.bx*(float)Inp.axis[0];
  fy=Cal.ay+Cal.by*(float)Inp.axis[1];

  FontSetColour(0xff0000);
  FontText(L"+", (int)fx-8,(int)fy-12);

  return 0;
}
