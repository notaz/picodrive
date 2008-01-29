
#include "app.h"

// d3d
static IDirect3D8 *Direct3D=NULL;
IDirect3DDevice8 *Device=NULL;
IDirect3DSurface8 *DirectBack=NULL; // Back Buffer

static IDirect3DVertexBuffer8 *VertexBuffer=NULL;

struct CustomVertex
{
  float x,y,z; // Vertex cordinates
  unsigned int colour;
  float u,v; // Texture coordinates
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

static CustomVertex VertexList[4];

// ddraw
#include <ddraw.h>

LPDIRECTDRAW7        m_pDD;
LPDIRECTDRAWSURFACE7 m_pddsFrontBuffer;
LPDIRECTDRAWSURFACE7 m_pddsBackBuffer;

// quick and dirty stuff..
static int DirectDrawInit()
{
  HRESULT ret;

  ret = DirectDrawCreateEx(NULL, (VOID**)&m_pDD, IID_IDirectDraw7, NULL);
  if (ret) { LOGFAIL(); return 1; }

  // Set cooperative level
  ret = m_pDD->SetCooperativeLevel( FrameWnd, DDSCL_NORMAL );
  if (ret) { LOGFAIL(); return 1; }

  // Create the primary surface
  DDSURFACEDESC2 ddsd;
  ZeroMemory( &ddsd, sizeof( ddsd ) );
  ddsd.dwSize         = sizeof( ddsd );
  ddsd.dwFlags        = DDSD_CAPS;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

  ret = m_pDD->CreateSurface( &ddsd, &m_pddsFrontBuffer, NULL );
  if (ret) { LOGFAIL(); return 1; }

  // Create the backbuffer surface
  ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;    
  ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
  ddsd.dwWidth        = 320;
  ddsd.dwHeight       = 240;

  ret = m_pDD->CreateSurface( &ddsd, &m_pddsBackBuffer, NULL );
  if (ret) { LOGFAIL(); return 1; }

  // clipper
  LPDIRECTDRAWCLIPPER pcClipper = NULL;
  ret = m_pDD->CreateClipper( 0, &pcClipper, NULL );
  if (ret) { LOGFAIL(); return 1; }

  ret = pcClipper->SetHWnd( 0, FrameWnd );
  if (ret) { LOGFAIL(); return 1; }

  ret = m_pddsFrontBuffer->SetClipper( pcClipper );
  if (ret) { LOGFAIL(); return 1; }

  RELEASE(pcClipper);

#if 0
  DDSURFACEDESC2 sd;
  memset(&sd, 0, sizeof(sd));
  sd.dwSize = sizeof(sd);
  ret = m_pddsBackBuffer->Lock(NULL, &sd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT|DDLOCK_WRITEONLY, NULL);
  if (ret) { LOGFAIL(); return 1; }

  memset(sd.lpSurface, 0xcc, 200*200);

  ret = m_pddsBackBuffer->Unlock(NULL);
  if (ret) { LOGFAIL(); return 1; }
#else
    DDBLTFX ddbltfx;
    ZeroMemory( &ddbltfx, sizeof(ddbltfx) );
    ddbltfx.dwSize      = sizeof(ddbltfx);
    ddbltfx.dwFillColor = 0xff00;

    ret = m_pddsBackBuffer->Blt( NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx );
#endif

  ret = m_pddsFrontBuffer->Blt(NULL, m_pddsBackBuffer, NULL, DDBLT_WAIT, NULL);
  if (ret) { LOGFAIL(); return 1; }
Sleep(2000);
/*   Sleep(500);
  ret = m_pddsFrontBuffer->Blt(NULL, m_pddsBackBuffer, NULL, DDBLT_WAIT, NULL);
  if (ret) { LOGFAIL(); return 1; }
   Sleep(500);
  ret = m_pddsFrontBuffer->Blt(NULL, m_pddsBackBuffer, NULL, DDBLT_WAIT, NULL);
  if (ret) { LOGFAIL(); return 1; }
*/
  return 0;
}


int DirectInit()
{
  D3DPRESENT_PARAMETERS d3dpp; 
  D3DDISPLAYMODE mode;
  int i,u,ret=0;

  memset(&d3dpp,0,sizeof(d3dpp));
  memset(&mode,0,sizeof(mode));

  Direct3D=Direct3DCreate8(D3D_SDK_VERSION); if (Direct3D==NULL) return 1;

  // Set up the structure used to create the D3D device:
  d3dpp.BackBufferWidth =MainWidth;
  d3dpp.BackBufferHeight=MainHeight;
  d3dpp.BackBufferCount =1;
  d3dpp.SwapEffect=D3DSWAPEFFECT_DISCARD;

#ifdef _XBOX
  d3dpp.BackBufferFormat=D3DFMT_X8R8G8B8;
  d3dpp.FullScreen_RefreshRateInHz=60;
#else
  Direct3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&mode);
  d3dpp.BackBufferFormat=mode.Format;
  d3dpp.Windowed=1;
#endif

  // Try to create a device with hardware vertex processing:
  for (i=0;i<3;i++)
  {
    int behave=D3DCREATE_HARDWARE_VERTEXPROCESSING;

    // Try software vertex processing:
    if (i==1) behave=D3DCREATE_MIXED_VERTEXPROCESSING;
    if (i==2) behave=D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    Direct3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,FrameWnd,
        behave|D3DCREATE_MULTITHREADED,&d3dpp,&Device);
    if (Device) break;
  }

  if (Device==NULL)
  {
#if 0
    // try ref
    Direct3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_REF,FrameWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING|D3DCREATE_MULTITHREADED,&d3dpp,&Device);
    if (Device==NULL) goto fail0;
    HMODULE test = LoadLibrary("d3d8d.dll");
    if (test != NULL) FreeLibrary(test);
    else {
      error("Sorry, but this program requires Direct3D with hardware acceleration.\n\n"
            "You can try using Direct3D software emulation, but you have to install "
            "DirectX SDK for it to work\n(it seems to be missing now).");
      goto fail1;
    }
#else
    goto fail1;
#endif
  }

  Device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&DirectBack);
  if (DirectBack==NULL) goto fail1;

  Device->CreateVertexBuffer(sizeof(VertexList),0,D3DFVF_CUSTOMVERTEX,D3DPOOL_DEFAULT,&VertexBuffer);
  if (VertexBuffer==NULL) goto fail2;

  ret=TexScreenInit(); if (ret) goto fail3;

  //FontInit();

  Device->SetRenderState(D3DRS_LIGHTING,0); // Turn off lighting

  // Set up texture modes:
  Device->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);
  Device->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);

  return 0;

fail3:
  RELEASE(VertexBuffer)
fail2:
  RELEASE(DirectBack)
fail1:
  RELEASE(Device)
fail0:
  RELEASE(Direct3D)

  // error("Failed to use Direct3D, trying DirectDraw..");

  // try DirectDraw
  return DirectDrawInit();
}

void DirectExit()
{
  //FontExit();
  TexScreenExit();

  RELEASE(VertexBuffer)
  RELEASE(DirectBack)
  RELEASE(Device)
  RELEASE(Direct3D)
}


static int MakeVertexList()
{
  struct CustomVertex *vert=NULL,*pv=NULL;
  float dist=0.0f;
  float scalex=0.0f,scaley=0.0f;
  unsigned int colour=0xffffff;
  float right=0.0f,bottom=0.0f;

  if (LoopMode!=8) colour=0x102040;

  dist=10.0f; scalex=dist*1.3333f; scaley=dist;

  scalex*=640.0f/(float)MainWidth;
  scaley*=448.0f/(float)MainHeight;

  vert=VertexList;

  // Put the vertices for the corners of the screen:
  pv=vert;
  pv->z=dist;
  pv->x=-scalex; pv->y=scaley;
  pv->colour=colour; pv++;

  *pv=vert[0]; pv->x= scalex; pv->y= scaley; pv++;
  *pv=vert[0]; pv->x=-scalex; pv->y=-scaley; pv++;
  *pv=vert[0]; pv->x= scalex; pv->y=-scaley; pv++;

  // Find where the screen images ends on the texture
  right =(float)EmuWidth /(float)TexWidth;
  bottom=(float)EmuHeight/(float)TexHeight;

  // Write texture coordinates:
  pv=vert;
  pv->u=0.0f;  pv->v=0.00f;  pv++;
  pv->u=right; pv->v=0.00f;  pv++;
  pv->u=0.0f;  pv->v=bottom; pv++;
  pv->u=right; pv->v=bottom; pv++;

  return 0;
}

int DirectClear(unsigned int colour)
{
  Device->Clear(0,NULL,D3DCLEAR_TARGET,colour,1.0f,0);
  return 0;
}

int DirectPresent()
{
  Device->Present(NULL,NULL,NULL,NULL);
  return 0;
}

static int SetupMatrices()
{
  D3DXVECTOR3 eye ( 0.0f, 0.0f, 0.0f );
  D3DXVECTOR3 look( 0.0f, 0.0f, 0.0f );
  D3DXVECTOR3 up  ( 0.0f, 1.0f, 0.0f );
  D3DXMATRIX mat;
  float nudgex=0.0f,nudgey=0.0f;

  memset(&mat,0,sizeof(mat));
  
  mat.m[0][0]=mat.m[1][1]=mat.m[2][2]=mat.m[3][3]=1.0f;
  Device->SetTransform(D3DTS_WORLD,&mat);

  look.x=(float)Inp.axis[2]/2457.6f;
  look.y=(float)Inp.axis[3]/2457.6f;
  look.z=10.0f;

  // Nudge pixels to the centre of each screen pixel:
  nudgex=13.3333f/(float)(MainWidth <<1);
  nudgey=10.0000f/(float)(MainHeight<<1);
  eye.x +=nudgex; eye.y +=nudgey;
  look.x+=nudgex; look.y+=nudgey;

  D3DXMatrixLookAtLH(&mat,&eye,&look,&up);
  Device->SetTransform(D3DTS_VIEW,&mat);

  D3DXMatrixPerspectiveFovLH(&mat, 0.5f*PI, 1.3333f, 0.2f, 1000.0f);
  Device->SetTransform(D3DTS_PROJECTION,&mat);
  return 0;
}

int DirectScreen()
{
  unsigned char *lock=NULL;
  int ret;

  // Copy the screen to the screen texture:
#ifdef _XBOX
  TexScreenSwizzle();
#else
  ret=TexScreenLinear();
  if (ret) dprintf2("TexScreenLinear failed\n");
#endif

  SetupMatrices();

  MakeVertexList();

  // Copy vertices in:
  VertexBuffer->Lock(0,sizeof(VertexList),&lock,0);
  if (lock==NULL) { dprintf2("VertexBuffer->Lock failed\n"); return 1; }
  memcpy(lock,VertexList,sizeof(VertexList));
  VertexBuffer->Unlock();

  ret=Device->BeginScene();
  if (ret) dprintf2("BeginScene failed\n");
  ret=Device->SetTexture(0,TexScreen);
  if (ret) dprintf2("SetTexture failed\n");
  ret=Device->SetStreamSource(0,VertexBuffer,sizeof(CustomVertex));
  if (ret) dprintf2("SetStreamSource failed\n");
  ret=Device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
  if (ret) dprintf2("SetVertexShader failed\n");
  ret=Device->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);
  if (ret) dprintf2("DrawPrimitive failed\n");
  ret=Device->EndScene();
  if (ret) dprintf2("EndScene failed\n");

  return 0;
}
