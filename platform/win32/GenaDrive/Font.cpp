
#include "app.h"

// ----------------------------------------------------------------------------------
#ifdef _XBOX

#include <xfont.h>
static XFONT *Font=NULL;

int FontInit()
{
  XFONT_OpenDefaultFont(&Font); if (Font==NULL) return 1;

  return 0;
}

void FontExit()
{
}

int FontSetColour(unsigned int colour)
{
  Font->SetTextColor(colour);
  return 0;
}

int FontText(WCHAR *text,int dx,int dy)
{
  if (Font==NULL || DirectBack==NULL) return 1;

  Font->TextOut(DirectBack,text,~0U,dx,dy);
  return 0;
}

#endif


// ----------------------------------------------------------------------------------
#ifndef _XBOX

static ID3DXFont *Font=NULL;
static unsigned int FontColour=0;

int FontInit()
{
  LOGFONT lf;

  memset(&lf,0,sizeof(lf));
  strcpy(lf.lfFaceName,"Arial");
  lf.lfHeight=24;
  D3DXCreateFontIndirect(Device,&lf,&Font);

  return 0;
}

void FontExit()
{
  RELEASE(Font);
}

int FontSetColour(unsigned int colour)
{
  FontColour=0xff000000|colour;
  return 0;
}

int FontText(WCHAR *text,int dx,int dy)
{
  RECT rect={0,0,0,0};

  if (Font==NULL || DirectBack==NULL) return 1;

  Font->Begin();
  rect.left=dx;
  rect.top=dy;
  rect.right=MainWidth;
  rect.bottom=MainHeight;

  Font->DrawTextW(text,-1,&rect,DT_LEFT,FontColour);
  Font->End();

  return 0;
}

#endif
