#ifndef _CD_SYS_H
#define _CD_SYS_H

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
	unsigned char Type;
	unsigned char Num;
	_msf MSF;
} _scd_track;

typedef struct
{
	unsigned char First_Track;
	unsigned char Last_Track;
	_scd_track Tracks[100];
} _scd_toc;

typedef struct {
	unsigned int Status_CDD;
	unsigned int Status_CDC;
	_scd_toc TOC;
	int Cur_LBA;
	unsigned int Cur_Track;
} _scd;


extern int CD_Timer_Counter;


void LBA_to_MSF(int lba, _msf *MSF);
int Track_to_LBA(int track);


void Check_CD_Command(void);

int  Init_CD_Driver(void);
void End_CD_Driver(void);
int  Insert_CD(char *buf, char *iso_name);
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

//void Write_CD_Audio(short *Buf, int rate, int channel, int lenght);
//void Update_CD_Audio(int **Buf, int lenght);


#ifdef __cplusplus
};
#endif

#endif

