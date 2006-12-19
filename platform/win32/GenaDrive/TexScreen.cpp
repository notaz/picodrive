
#include "app.h"

IDirect3DTexture8 *TexScreen=NULL;
int TexWidth=0,TexHeight=0;

// Blank the texture:
static int TexBlank()
{
  D3DLOCKED_RECT lock={0,NULL};
  unsigned char *dest=NULL;
  int y=0,line=0;

  TexScreen->LockRect(0,&lock,NULL,0); if (lock.pBits==NULL) return 1;

  dest=(unsigned char *)lock.pBits;
  for (y=0; y<TexHeight; y++,line+=lock.Pitch)
  {
    memset(dest+line,0,TexWidth<<1);
  }

  TexScreen->UnlockRect(0);
  return 0;
}

int TexScreenInit()
{
  TexWidth =512;
  TexHeight=512;

  Device->CreateTexture(TexWidth,TexHeight,1,0,D3DFMT_R5G6B5,D3DPOOL_MANAGED,&TexScreen);
  if (TexScreen==NULL) return 1;

  TexBlank();
  return 0;
}

void TexScreenExit()
{
  RELEASE(TexScreen)
  TexWidth=TexHeight=0;
}

// Copy screen to a swizzled texture
int TexScreenSwizzle()
{
  D3DLOCKED_RECT lock={0,NULL};
  unsigned char *dest=NULL;
  int y=0,sy=0,mask=0;
  unsigned short *ps=NULL;

  mask=TexWidth*TexHeight-1;

  TexScreen->LockRect(0,&lock,NULL,0); if (lock.pBits==NULL) return 1;

  dest=(unsigned char *)lock.pBits;
  ps=EmuScreen;

  // Write to swizzled locations:
  for (y=0,sy=0; y<EmuHeight; y++,sy++)
  {
    int x=0,sx=0;
    sy|=0x55555555;

    for (x=0,sx=0; x<EmuWidth; x++,sx++)
    {
      int addr=0;

      sx|=0xaaaaaaaa;
      addr=sx&sy&mask; // Calculate swizzled address

      ((unsigned short *)dest)[addr]=*ps++;
    }
  }

  TexScreen->UnlockRect(0);

  return 0;
}

// Copy screen to a linear texture:
int TexScreenLinear()
{
  D3DLOCKED_RECT lock={0,NULL};
  unsigned char *dest=NULL;
  int y=0,line=0;
  unsigned short *ps=NULL;

  TexScreen->LockRect(0,&lock,NULL,0); if (lock.pBits==NULL) return 1;

  dest=(unsigned char *)lock.pBits;
  ps=EmuScreen;

  for (y=0; y<EmuHeight; y++,line+=lock.Pitch)
  {
    int x=0;
    int addr=line;

    for (x=0; x<EmuWidth; x++,addr+=2)
    {
      *(unsigned int *)(dest+addr)=*ps++;
    }
  }

  TexScreen->UnlockRect(0);
  return 0;
}
