
#ifndef _3DSIMPL_H_
#define _3DSIMPL_H_

#include "3dsgpu.h"
#include "3dsinterface.h"

#define BTN3DS_A        0
#define BTN3DS_B        1
#define BTN3DS_X        2
#define BTN3DS_Y        3
#define BTN3DS_L        4
#define BTN3DS_R        5
#define BTN3DS_ZL       6
#define BTN3DS_ZR       7
#define BTN3DS_SELECT   8
#define BTN3DS_START    9


extern SGPUTexture *emuMainScreenTarget[2];
extern SGPUTexture *emuTileCacheTexture;
extern SGPUTexture *emuDepthForScreens;
//extern SGPUTexture *snesDepthForOtherTextures;

extern SSettings3DS settings3DS;


#endif