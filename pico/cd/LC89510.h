/***********************************************************
 *                                                         *
 * This source was taken from the Gens project             *
 * Written by Stéphane Dallongeville                       *
 * Copyright (c) 2002 by Stéphane Dallongeville            *
 * Modified/adapted for PicoDrive by notaz, 2007           *
 *                                                         *
 ***********************************************************/

#ifndef _LC89510_H
#define _LC89510_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
//	unsigned short Fader;	// 34
//	unsigned short Control;	// 36
//	unsigned short Cur_Comm;// unused

	// "Receive status"
	unsigned short Status;
	unsigned short Minute;
	unsigned short Seconde;
	unsigned short Frame;
	unsigned char  Ext;
	unsigned char  pad[3];
} CDD;


PICO_INTERNAL void CDD_Export_Status(void);
PICO_INTERNAL void CDD_Import_Command(void);

void CDD_Reset(void);

#ifdef __cplusplus
};
#endif

#endif

