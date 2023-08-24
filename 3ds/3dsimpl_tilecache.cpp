
#include <stdio.h>
#include <string.h>


#include "3dsimpl.h"
#include "3dsimpl_gpu.h"
#include "3dsimpl_tilecache.h"

//---------------------------------------------------------
// Initializes the Hash to Texture Position look-up (and
// the reverse look-up table as well)
//---------------------------------------------------------
void cache3dsInit()
{
    memset(&GPU3DSExt.vramCacheHashToTexturePosition, 0, (MAX_HASH + 1) * 2);

    GPU3DSExt.vramCacheTexturePositionToHash[0] = 0;
    for (int i = 1; i < MAX_TEXTURE_POSITIONS; i++)
        GPU3DSExt.vramCacheTexturePositionToHash[i] = MAX_HASH;
    GPU3DSExt.newCacheTexturePosition = 2;
}


//---------------------------------------------------------
// Converts the tile format into it's 5551 16-bit 
// representation in 3DS texture format.
//---------------------------------------------------------
void cache3dsCacheTGFX8x8TileToTexturePosition(
    uint8 *tgfxTilePixels,
	uint16 *tgfxPalette,
    uint16 texturePosition)
{
    int tx = texturePosition % 128;
    int ty = (texturePosition / 128) & 0x7f;
    texturePosition = (127 - ty) * 128 + tx;    // flip vertically.
    uint32 base = texturePosition * 64;

    uint16 *tileTexture = (uint16 *)emuTileCacheTexture->PixelData;

    #define GET_TILE_PIXEL(x, y)  \
         (tgfxTilePixels[y * 8 + x] == 0 ? 0 : tgfxPalette[tgfxTilePixels[y * 8 + x]])

    tileTexture [base + 0] = GET_TILE_PIXEL(0, 7);
    tileTexture [base + 1] = GET_TILE_PIXEL(1, 7);
    tileTexture [base + 4] = GET_TILE_PIXEL(2, 7);
    tileTexture [base + 5] = GET_TILE_PIXEL(3, 7);
    tileTexture [base + 16] = GET_TILE_PIXEL(4, 7);
    tileTexture [base + 17] = GET_TILE_PIXEL(5, 7);
    tileTexture [base + 20] = GET_TILE_PIXEL(6, 7);
    tileTexture [base + 21] = GET_TILE_PIXEL(7, 7);

    tileTexture [base + 2] = GET_TILE_PIXEL(0, 6);
    tileTexture [base + 3] = GET_TILE_PIXEL(1, 6);
    tileTexture [base + 6] = GET_TILE_PIXEL(2, 6);
    tileTexture [base + 7] = GET_TILE_PIXEL(3, 6);
    tileTexture [base + 18] = GET_TILE_PIXEL(4, 6);
    tileTexture [base + 19] = GET_TILE_PIXEL(5, 6);
    tileTexture [base + 22] = GET_TILE_PIXEL(6, 6);
    tileTexture [base + 23] = GET_TILE_PIXEL(7, 6);

    tileTexture [base + 8] = GET_TILE_PIXEL(0, 5);
    tileTexture [base + 9] = GET_TILE_PIXEL(1, 5);
    tileTexture [base + 12] = GET_TILE_PIXEL(2, 5);
    tileTexture [base + 13] = GET_TILE_PIXEL(3, 5);
    tileTexture [base + 24] = GET_TILE_PIXEL(4, 5);
    tileTexture [base + 25] = GET_TILE_PIXEL(5, 5);
    tileTexture [base + 28] = GET_TILE_PIXEL(6, 5);
    tileTexture [base + 29] = GET_TILE_PIXEL(7, 5);

    tileTexture [base + 10] = GET_TILE_PIXEL(0, 4);
    tileTexture [base + 11] = GET_TILE_PIXEL(1, 4);
    tileTexture [base + 14] = GET_TILE_PIXEL(2, 4);
    tileTexture [base + 15] = GET_TILE_PIXEL(3, 4);
    tileTexture [base + 26] = GET_TILE_PIXEL(4, 4);
    tileTexture [base + 27] = GET_TILE_PIXEL(5, 4);
    tileTexture [base + 30] = GET_TILE_PIXEL(6, 4);
    tileTexture [base + 31] = GET_TILE_PIXEL(7, 4);

    tileTexture [base + 32] = GET_TILE_PIXEL(0, 3);
    tileTexture [base + 33] = GET_TILE_PIXEL(1, 3);
    tileTexture [base + 36] = GET_TILE_PIXEL(2, 3);
    tileTexture [base + 37] = GET_TILE_PIXEL(3, 3);
    tileTexture [base + 48] = GET_TILE_PIXEL(4, 3);
    tileTexture [base + 49] = GET_TILE_PIXEL(5, 3);
    tileTexture [base + 52] = GET_TILE_PIXEL(6, 3);
    tileTexture [base + 53] = GET_TILE_PIXEL(7, 3);

    tileTexture [base + 34] = GET_TILE_PIXEL(0, 2);
    tileTexture [base + 35] = GET_TILE_PIXEL(1, 2);
    tileTexture [base + 38] = GET_TILE_PIXEL(2, 2);
    tileTexture [base + 39] = GET_TILE_PIXEL(3, 2);
    tileTexture [base + 50] = GET_TILE_PIXEL(4, 2);
    tileTexture [base + 51] = GET_TILE_PIXEL(5, 2);
    tileTexture [base + 54] = GET_TILE_PIXEL(6, 2);
    tileTexture [base + 55] = GET_TILE_PIXEL(7, 2);

    tileTexture [base + 40] = GET_TILE_PIXEL(0, 1);
    tileTexture [base + 41] = GET_TILE_PIXEL(1, 1);
    tileTexture [base + 44] = GET_TILE_PIXEL(2, 1);
    tileTexture [base + 45] = GET_TILE_PIXEL(3, 1);
    tileTexture [base + 56] = GET_TILE_PIXEL(4, 1);
    tileTexture [base + 57] = GET_TILE_PIXEL(5, 1);
    tileTexture [base + 60] = GET_TILE_PIXEL(6, 1);
    tileTexture [base + 61] = GET_TILE_PIXEL(7, 1);

    tileTexture [base + 42] = GET_TILE_PIXEL(0, 0);
    tileTexture [base + 43] = GET_TILE_PIXEL(1, 0);
    tileTexture [base + 46] = GET_TILE_PIXEL(2, 0);
    tileTexture [base + 47] = GET_TILE_PIXEL(3, 0);
    tileTexture [base + 58] = GET_TILE_PIXEL(4, 0);
    tileTexture [base + 59] = GET_TILE_PIXEL(5, 0);
    tileTexture [base + 62] = GET_TILE_PIXEL(6, 0);
    tileTexture [base + 63] = GET_TILE_PIXEL(7, 0);

}


//---------------------------------------------------------
// Converts the tile format into it's 5551 16-bit 
// representation in 3DS texture format.
//
// The quarter indicates the quarter of the tile
// that will be cached.
//
//      0 | 1
//     ---+---
//      2 | 3
//
//---------------------------------------------------------
void cache3dsCacheTGFX16x16TileToTexturePosition(
    uint8 *tgfxTilePixels,
	uint16 *tgfxPalette,
    int quarter,
    uint16 texturePosition)
{
    
    int tx = texturePosition % 128;
    int ty = (texturePosition / 128) & 0x7f;
    texturePosition = (127 - ty) * 128 + tx;    // flip vertically.
    uint32 base = texturePosition * 64;

    uint16 *tileTexture = (uint16 *)emuTileCacheTexture->PixelData;

    switch (quarter)
    {
        case 0 : break;
        case 1 : quarter = 8; break;
        case 2 : quarter = 128; break;
        case 3 : quarter = 136; break;
    }

    #define GET_TILE_PIXEL(x, y)  \
         (tgfxTilePixels[quarter + y * 16 + x] == 0 ? 0 : tgfxPalette[tgfxTilePixels[quarter + y * 16 + x]])

    tileTexture [base + 0] = GET_TILE_PIXEL(0, 7);
    tileTexture [base + 1] = GET_TILE_PIXEL(1, 7);
    tileTexture [base + 4] = GET_TILE_PIXEL(2, 7);
    tileTexture [base + 5] = GET_TILE_PIXEL(3, 7);
    tileTexture [base + 16] = GET_TILE_PIXEL(4, 7);
    tileTexture [base + 17] = GET_TILE_PIXEL(5, 7);
    tileTexture [base + 20] = GET_TILE_PIXEL(6, 7);
    tileTexture [base + 21] = GET_TILE_PIXEL(7, 7);

    tileTexture [base + 2] = GET_TILE_PIXEL(0, 6);
    tileTexture [base + 3] = GET_TILE_PIXEL(1, 6);
    tileTexture [base + 6] = GET_TILE_PIXEL(2, 6);
    tileTexture [base + 7] = GET_TILE_PIXEL(3, 6);
    tileTexture [base + 18] = GET_TILE_PIXEL(4, 6);
    tileTexture [base + 19] = GET_TILE_PIXEL(5, 6);
    tileTexture [base + 22] = GET_TILE_PIXEL(6, 6);
    tileTexture [base + 23] = GET_TILE_PIXEL(7, 6);

    tileTexture [base + 8] = GET_TILE_PIXEL(0, 5);
    tileTexture [base + 9] = GET_TILE_PIXEL(1, 5);
    tileTexture [base + 12] = GET_TILE_PIXEL(2, 5);
    tileTexture [base + 13] = GET_TILE_PIXEL(3, 5);
    tileTexture [base + 24] = GET_TILE_PIXEL(4, 5);
    tileTexture [base + 25] = GET_TILE_PIXEL(5, 5);
    tileTexture [base + 28] = GET_TILE_PIXEL(6, 5);
    tileTexture [base + 29] = GET_TILE_PIXEL(7, 5);

    tileTexture [base + 10] = GET_TILE_PIXEL(0, 4);
    tileTexture [base + 11] = GET_TILE_PIXEL(1, 4);
    tileTexture [base + 14] = GET_TILE_PIXEL(2, 4);
    tileTexture [base + 15] = GET_TILE_PIXEL(3, 4);
    tileTexture [base + 26] = GET_TILE_PIXEL(4, 4);
    tileTexture [base + 27] = GET_TILE_PIXEL(5, 4);
    tileTexture [base + 30] = GET_TILE_PIXEL(6, 4);
    tileTexture [base + 31] = GET_TILE_PIXEL(7, 4);

    tileTexture [base + 32] = GET_TILE_PIXEL(0, 3);
    tileTexture [base + 33] = GET_TILE_PIXEL(1, 3);
    tileTexture [base + 36] = GET_TILE_PIXEL(2, 3);
    tileTexture [base + 37] = GET_TILE_PIXEL(3, 3);
    tileTexture [base + 48] = GET_TILE_PIXEL(4, 3);
    tileTexture [base + 49] = GET_TILE_PIXEL(5, 3);
    tileTexture [base + 52] = GET_TILE_PIXEL(6, 3);
    tileTexture [base + 53] = GET_TILE_PIXEL(7, 3);

    tileTexture [base + 34] = GET_TILE_PIXEL(0, 2);
    tileTexture [base + 35] = GET_TILE_PIXEL(1, 2);
    tileTexture [base + 38] = GET_TILE_PIXEL(2, 2);
    tileTexture [base + 39] = GET_TILE_PIXEL(3, 2);
    tileTexture [base + 50] = GET_TILE_PIXEL(4, 2);
    tileTexture [base + 51] = GET_TILE_PIXEL(5, 2);
    tileTexture [base + 54] = GET_TILE_PIXEL(6, 2);
    tileTexture [base + 55] = GET_TILE_PIXEL(7, 2);

    tileTexture [base + 40] = GET_TILE_PIXEL(0, 1);
    tileTexture [base + 41] = GET_TILE_PIXEL(1, 1);
    tileTexture [base + 44] = GET_TILE_PIXEL(2, 1);
    tileTexture [base + 45] = GET_TILE_PIXEL(3, 1);
    tileTexture [base + 56] = GET_TILE_PIXEL(4, 1);
    tileTexture [base + 57] = GET_TILE_PIXEL(5, 1);
    tileTexture [base + 60] = GET_TILE_PIXEL(6, 1);
    tileTexture [base + 61] = GET_TILE_PIXEL(7, 1);

    tileTexture [base + 42] = GET_TILE_PIXEL(0, 0);
    tileTexture [base + 43] = GET_TILE_PIXEL(1, 0);
    tileTexture [base + 46] = GET_TILE_PIXEL(2, 0);
    tileTexture [base + 47] = GET_TILE_PIXEL(3, 0);
    tileTexture [base + 58] = GET_TILE_PIXEL(4, 0);
    tileTexture [base + 59] = GET_TILE_PIXEL(5, 0);
    tileTexture [base + 62] = GET_TILE_PIXEL(6, 0);
    tileTexture [base + 63] = GET_TILE_PIXEL(7, 0);

}

