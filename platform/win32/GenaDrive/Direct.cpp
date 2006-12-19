
#include "app.h"

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

int DirectInit()
{
  D3DPRESENT_PARAMETERS d3dpp; 
  D3DDISPLAYMODE mode;
  int i=0,ret=0;

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
  for (i=0;i<4;i++)
  {
    int behave=D3DCREATE_HARDWARE_VERTEXPROCESSING;

#ifdef _XBOX
    if (i==1)
    {
      // If 60Hz didn't work, try PAL 50Hz instead:
      d3dpp.FullScreen_RefreshRateInHz=0;
      d3dpp.BackBufferHeight=MainHeight=576;
    }
#endif

    // Try software vertex processing:
    if (i==2) behave=D3DCREATE_MIXED_VERTEXPROCESSING;
    if (i==3) behave=D3DCREATE_SOFTWARE_VERTEXPROCESSING;

    Direct3D->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,FrameWnd,behave,&d3dpp,&Device);
    if (Device) break;
  }

  if (Device==NULL) return 1;

  Device->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&DirectBack);
  if (DirectBack==NULL) return 1;

  Device->CreateVertexBuffer(sizeof(VertexList),0,D3DFVF_CUSTOMVERTEX,D3DPOOL_DEFAULT,&VertexBuffer);
  if (VertexBuffer==NULL) return 1;

  ret=TexScreenInit(); if (ret) return 1;

  FontInit();

  Device->SetRenderState(D3DRS_LIGHTING,0); // Turn off lighting

  // Set up texture modes:
  Device->SetTextureStageState(0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);
  Device->SetTextureStageState(0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
  return 0;
}

void DirectExit()
{
  FontExit();
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

  // Copy the screen to the screen texture:
#ifdef _XBOX
  TexScreenSwizzle();
#else
  TexScreenLinear();
#endif

  SetupMatrices();

  MakeVertexList();

  // Copy vertices in:
  VertexBuffer->Lock(0,sizeof(VertexList),&lock,0); if (lock==NULL) return 1;
  memcpy(lock,VertexList,sizeof(VertexList));
  VertexBuffer->Unlock();

  Device->SetTexture(0,TexScreen);
  Device->SetStreamSource(0,VertexBuffer,sizeof(CustomVertex));
  Device->SetVertexShader(D3DFVF_CUSTOMVERTEX);
  Device->DrawPrimitive(D3DPT_TRIANGLESTRIP,0,2);
  
  return 0;
}
