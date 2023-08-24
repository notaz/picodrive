
#include <3ds.h>
#include "3dsgpu.h"
#include "3dsimpl.h"
#include "3dsimpl_gpu.h"

SGPU3DSExtended GPU3DSExt;


void gpu3dsDrawRectangle(int x0, int y0, int x1, int y1, int depth, u32 color)
{
    gpu3dsAddRectangleVertexes (x0, y0, x1, y1, depth, color);
    gpu3dsDrawVertexList(&GPU3DSExt.rectangleVertexes, GPU_TRIANGLES, false, -1, -1);
}


void gpu3dsAddRectangleVertexes(int x0, int y0, int x1, int y1, int depth, u32 color)
{
    /*
    if (emulator.isReal3DS)
    {
        SVertexColor *vertices = &((SVertexColor *) GPU3DSExt.rectangleVertexes.List)[GPU3DSExt.rectangleVertexes.Count];

        vertices[0].Position = (SVector4i){x0, y0, depth, 1};
        vertices[1].Position = (SVector4i){x1, y1, depth, 1};

        u32 swappedColor = ((color & 0xff) << 24) | ((color & 0xff00) << 8) | ((color & 0xff0000) >> 8) | ((color & 0xff000000) >> 24);
        vertices[0].Color = swappedColor;
        vertices[1].Color = swappedColor;

        GPU3DSExt.rectangleVertexes.Count += 2;
    }
    else*/
    {
        SVertexColor *vertices = &((SVertexColor *) GPU3DSExt.rectangleVertexes.List)[GPU3DSExt.rectangleVertexes.Count];

        vertices[0].Position = (SVector4i){x0, y0, depth, 1};
        vertices[1].Position = (SVector4i){x1, y0, depth, 1};
        vertices[2].Position = (SVector4i){x0, y1, depth, 1};
        vertices[3].Position = (SVector4i){x1, y1, depth, 1};
        vertices[4].Position = (SVector4i){x1, y0, depth, 1};
        vertices[5].Position = (SVector4i){x0, y1, depth, 1};

        u32 swappedColor = ((color & 0xff) << 24) | ((color & 0xff00) << 8) | ((color & 0xff0000) >> 8) | ((color & 0xff000000) >> 24);
        vertices[0].Color = swappedColor;
        vertices[1].Color = swappedColor;
        vertices[2].Color = swappedColor;
        vertices[3].Color = swappedColor;
        vertices[4].Color = swappedColor;
        vertices[5].Color = swappedColor;

        GPU3DSExt.rectangleVertexes.Count += 6;
    }
}


void gpu3dsDrawVertexes(bool repeatLastDraw, int storeIndex)
{
    gpu3dsDrawVertexList(&GPU3DSExt.quadVertexes, GPU_TRIANGLES, repeatLastDraw, 0, storeIndex);
    gpu3dsDrawVertexList(&GPU3DSExt.tileVertexes, GPU_GEOMETRY_PRIM, repeatLastDraw, 1, storeIndex);
    gpu3dsDrawVertexList(&GPU3DSExt.rectangleVertexes, GPU_TRIANGLES, repeatLastDraw, 2, storeIndex);
}


void gpu3dsBindTextureMainScreen(SGPUTexture *texture, GPU_TEXUNIT unit)
{
    gpu3dsBindTextureWithParams(texture, unit,
        GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)
        | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
        | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_BORDER)
        | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_BORDER));
}

