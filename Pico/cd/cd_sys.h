/***********************************************************
 *                                                         *
 * This source was taken from the Gens project             *
 * Written by Stéphane Dallongeville                       *
 * Copyright (c) 2002 by Stéphane Dallongeville            *
 * Modified/adapted for PicoDrive by notaz, 2007           *
 *                                                         *
 ***********************************************************/

#ifndef _CD_SYS_H
#define _CD_SYS_H

#include "cd_file.h"

#ifdef __cplusplus
extern "C" {
#endif


#define INT_TO_BCDB(c)										\
((c) > 99)?(0x99):((((c) / 10) << 4) + ((c) % 10));

#define INT_TO_BCDW(c)										\
((c) > 99)?(0x0909):((((c) / 10) << 8) + ((c) % 10));

#define BCDB_TO_INT(c)										\
(((c) >> 4) * 10) + ((c) & 0xF);

#define BCDW_TO_INT(c)										\
(((c) >> 8) * 10) + ((c) & 0xF);


typedef struct
{
  unsigned char M;
  unsigned char S;
  unsigned char F;
} _msf;

typedef struct
{
//	unsigned char Type; // always 1 (data) for 1st track, 0 (audio) for others
//	unsigned char Num; // unused
	_msf MSF;
	//
	char ftype; // TYPE_ISO, TYPE_BIN, TYPE_MP3
	void *F;
	int Length;
	short KBtps; // kbytes per sec for mp3s (bitrate / 1000 / 8)
	short pad;
} _scd_track;

typedef struct
{
//	unsigned char First_Track; // always 1
	_scd_track Tracks[100];
	unsigned int Last_Track;
} _scd_toc;

typedef struct {
	unsigned int Status_CDD;
	unsigned int Status_CDC;
	int Cur_LBA;
	unsigned int Cur_Track;
	int File_Add_Delay;
	char CDD_Complete;
	int pad[6];
} _scd;


void LBA_to_MSF(int lba, _msf *MSF);
int Track_to_LBA(int track);


void Check_CD_Command(void);

int  Init_CD_Driver(void);
void End_CD_Driver(void);
int  Insert_CD(char *iso_name, int is_bin);
void Stop_CD(void);
void Change_CD(void);
void Reset_CD(void);

int Get_Status_CDD_c0(void);
int Stop_CDD_c1(void);
int Get_Pos_CDD_c20(void);
int Get_Track_Pos_CDD_c21(void);
int Get_Current_Track_CDD_c22(void);
int Get_Total_Lenght_CDD_c23(void);
int Get_First_Last_Track_CDD_c24(void);
int Get_Track_Adr_CDD_c25(void);
int Play_CDD_c3(void);
int Seek_CDD_c4(void);
int Pause_CDD_c6(void);
int Resume_CDD_c7(void);
int Fast_Foward_CDD_c8(void);
int Fast_Rewind_CDD_c9(void);
int CDD_cA(void);
int Close_Tray_CDD_cC(void);
int Open_Tray_CDD_cD(void);

int CDD_Def(void);


#ifdef __cplusplus
};
#endif

#endif

