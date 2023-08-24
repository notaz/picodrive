
#ifndef _3DSIMPL_TILECACHE_H_
#define _3DSIMPL_TILECACHE_H_

#include "3dsmain.h"
#include "3dsimpl_gpu.h"
#include "3dsdebug.h"

#define COMPOSE_HASH(tileNumber, pal)   ((tileNumber) << 5) + ((pal))

//---------------------------------------------------------
// Initializes the Hash to Texture Position look-up (and
// the reverse look-up table as well)
//---------------------------------------------------------
void cache3dsInit();


//---------------------------------------------------------
// Computes and returns the texture position (Non-Mode 7) 
// given the tile number and 
//---------------------------------------------------------
inline int cache3dsGetTexturePositionFast(int vramAddr, int pal)
{
    int tileNumber = vramAddr / 32;
    int hash = COMPOSE_HASH(tileNumber, pal);
    int pos = GPU3DSExt.vramCacheHashToTexturePosition[hash];

    if (pos == 0)
    {
        pos = GPU3DSExt.newCacheTexturePosition;

        //vramCacheFrameNumber[hash] = 0;

        GPU3DSExt.vramCacheTexturePositionToHash[GPU3DSExt.vramCacheHashToTexturePosition[hash] & 0xFFFE] = 0;

        GPU3DSExt.vramCacheHashToTexturePosition[GPU3DSExt.vramCacheTexturePositionToHash[pos]] = 0;

        GPU3DSExt.vramCacheHashToTexturePosition[hash] = pos;
        GPU3DSExt.vramCacheTexturePositionToHash[pos] = hash;

        GPU3DSExt.newCacheTexturePosition += 2;
        if (GPU3DSExt.newCacheTexturePosition >= MAX_TEXTURE_POSITIONS)
            GPU3DSExt.newCacheTexturePosition = 2;

        // Force this tile to re-decode. This fixes the tile corruption
        // problems when playing a game for too long.
        //
        GPU3DSExt.VRAMPaletteFrame[tileNumber][pal] = 0;
    }


    return pos;
}


//---------------------------------------------------------
// Swaps the texture position for alternate frames. This
// is required to fix sprite flickering problems in games
// that updates the tile bitmaps mid-frame.
//---------------------------------------------------------
inline int cacheGetSwapTexturePositionForAltFrameFast(int vramAddr, int pal)
{
    int tileNumber = vramAddr / 32;
    int hash = COMPOSE_HASH(tileNumber, pal);
    int pos = GPU3DSExt.vramCacheHashToTexturePosition[hash] ^ 1;
    GPU3DSExt.vramCacheHashToTexturePosition[hash] = pos;
    return pos;
}


//---------------------------------------------------------
// Converts the tile format into it's 5551 16-bit 
// representation in 3DS texture format.
//---------------------------------------------------------
void cache3dsCacheTGFX8x8TileToTexturePosition(
    uint8 *tgfxTilePixels,
	uint16 *tgfxPalette,
    uint16 texturePosition);


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
    uint16 texturePosition);



#endif