#include <stdio.h>
#include "cd_sys.h"
#include "cd_file.h"

#include "../PicoInt.h"

#define cdprintf dprintf
//#define cdprintf(x...)
#define DEBUG_CD

#define TRAY_OPEN	0x0500		// TRAY OPEN CDD status
#define NOCD		0x0000		// CD removed CDD status
#define STOPPED		0x0900		// STOPPED CDD status (happen after stop or close tray command)
#define READY		0x0400		// READY CDD status (also used for seeking)
#define FAST_FOW	0x0300		// FAST FORWARD track CDD status
#define FAST_REV	0x10300		// FAST REVERSE track CDD status
#define PLAYING		0x0100		// PLAYING audio track CDD status


/*
int CDDA_Enable;

int CD_Audio_Buffer_L[8192];
int CD_Audio_Buffer_R[8192];
int CD_Audio_Buffer_Read_Pos = 0;
int CD_Audio_Buffer_Write_Pos = 2000;
int CD_Audio_Starting;
*/

static int CD_Present = 0;
int CD_Timer_Counter = 0; // TODO: check refs

static int CDD_Complete;

static int File_Add_Delay = 0;



#define CHECK_TRAY_OPEN				\
if (Pico_mcd->scd.Status_CDD == TRAY_OPEN)	\
{									\
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;	\
									\
	Pico_mcd->cdd.Minute = 0;					\
	Pico_mcd->cdd.Seconde = 0;				\
	Pico_mcd->cdd.Frame = 0;					\
	Pico_mcd->cdd.Ext = 0;					\
									\
	CDD_Complete = 1;				\
									\
	return 2;						\
}


#define CHECK_CD_PRESENT			\
if (!CD_Present)					\
{									\
	Pico_mcd->scd.Status_CDD = NOCD;			\
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;	\
									\
	Pico_mcd->cdd.Minute = 0;					\
	Pico_mcd->cdd.Seconde = 0;				\
	Pico_mcd->cdd.Frame = 0;					\
	Pico_mcd->cdd.Ext = 0;					\
									\
	CDD_Complete = 1;				\
									\
	return 3;						\
}


#if 0
static void MSB2DWORD(unsigned int *d, unsigned char *b)
{
	unsigned int  retVal;

	retVal = (unsigned int )b[0];
	retVal = (retVal<<8) + (unsigned int )b[1];
	retVal = (retVal<<8) + (unsigned int )b[2];
	retVal = (retVal<<8) + (unsigned int )b[3];

	*d = retVal;
}
#endif

static int MSF_to_LBA(_msf *MSF)
{
	return (MSF->M * 60 * 75) + (MSF->S * 75) + MSF->F - 150;
}


void LBA_to_MSF(int lba, _msf *MSF)
{
	if (lba < -150) lba = 0;
	else lba += 150;
	MSF->M = lba / (60 * 75);
	MSF->S = (lba / 75) % 60;
	MSF->F = lba % 75;
}


static unsigned int MSF_to_Track(_msf *MSF)
{
	int i, Start, Cur;

	Start = (MSF->M << 16) + (MSF->S << 8) + MSF->F;

	for(i = Pico_mcd->scd.TOC.First_Track; i <= (Pico_mcd->scd.TOC.Last_Track + 1); i++)
	{
		Cur = Pico_mcd->scd.TOC.Tracks[i - Pico_mcd->scd.TOC.First_Track].MSF.M << 16;
		Cur += Pico_mcd->scd.TOC.Tracks[i - Pico_mcd->scd.TOC.First_Track].MSF.S << 8;
		Cur += Pico_mcd->scd.TOC.Tracks[i - Pico_mcd->scd.TOC.First_Track].MSF.F;

		if (Cur > Start) break;
	}

	--i;

	if (i > Pico_mcd->scd.TOC.Last_Track) return 100;
	if (i < Pico_mcd->scd.TOC.First_Track) i = Pico_mcd->scd.TOC.First_Track;

	return (unsigned) i;
}


static unsigned int LBA_to_Track(int lba)
{
	_msf MSF;

	LBA_to_MSF(lba, &MSF);
	return MSF_to_Track(&MSF);
}


static void Track_to_MSF(int track, _msf *MSF)
{
	if (track < Pico_mcd->scd.TOC.First_Track) track = Pico_mcd->scd.TOC.First_Track;
	else if (track > Pico_mcd->scd.TOC.Last_Track) track = Pico_mcd->scd.TOC.Last_Track;

	MSF->M = Pico_mcd->scd.TOC.Tracks[track - Pico_mcd->scd.TOC.First_Track].MSF.M;
	MSF->S = Pico_mcd->scd.TOC.Tracks[track - Pico_mcd->scd.TOC.First_Track].MSF.S;
	MSF->F = Pico_mcd->scd.TOC.Tracks[track - Pico_mcd->scd.TOC.First_Track].MSF.F;
}


int Track_to_LBA(int track)
{
	_msf MSF;

	Track_to_MSF(track, &MSF);
	return MSF_to_LBA(&MSF);
}


void Check_CD_Command(void)
{
	cdprintf("CHECK CD COMMAND");

	// Check CDD

	if (CDD_Complete)
	{
		CDD_Complete = 0;

		CDD_Export_Status();
	}

	// Check CDC

	if (Pico_mcd->scd.Status_CDC & 1)			// CDC is reading data ...
	{
		cdprintf("Got a read command");

		// DATA ?
		if (Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track].Type)
		     Pico_mcd->s68k_regs[0x36] |=  0x01;
		else Pico_mcd->s68k_regs[0x36] &= ~0x01;			// AUDIO

		if (File_Add_Delay == 0)
		{
			FILE_Read_One_LBA_CDC();
		}
		else File_Add_Delay--;
	}

	if (Pico_mcd->scd.Status_CDD == FAST_FOW)
	{
		Pico_mcd->scd.Cur_LBA += 10;
		CDC_Update_Header();

	}
	else if (Pico_mcd->scd.Status_CDD == FAST_REV)
	{
		Pico_mcd->scd.Cur_LBA -= 10;
		if (Pico_mcd->scd.Cur_LBA < -150) Pico_mcd->scd.Cur_LBA = -150;
		CDC_Update_Header();
	}
}


int Init_CD_Driver(void)
{
	FILE_Init();

	return 0;
}


void End_CD_Driver(void)
{
	FILE_End();
}


void Reset_CD(void)
{
	Pico_mcd->scd.Cur_Track = 0;
	Pico_mcd->scd.Cur_LBA = -150;
	Pico_mcd->scd.Status_CDD = READY;
	CDD_Complete = 0;
}


int Insert_CD(char *iso_name, int is_bin)
{
	int ret = 0;

//	memset(CD_Audio_Buffer_L, 0, 4096 * 4);
//	memset(CD_Audio_Buffer_R, 0, 4096 * 4);

	CD_Present = 0;

	if (iso_name != NULL)
	{
		ret = Load_ISO(iso_name, is_bin);
		if (ret == 0)
			CD_Present = 1;
	}

	return ret;
}


void Stop_CD(void)
{
	Unload_ISO();
	CD_Present = 0;
}


void Change_CD(void)
{
	if (Pico_mcd->scd.Status_CDD == TRAY_OPEN) Close_Tray_CDD_cC();
	else Open_Tray_CDD_cD();
}


int Get_Status_CDD_c0(void)
{
	cdprintf("Status command : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	// Clear immediat status
	if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0200)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);
	else if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0700)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);
	else if ((Pico_mcd->cdd.Status & 0x0F00) == 0x0E00)
		Pico_mcd->cdd.Status = (Pico_mcd->scd.Status_CDD & 0xFF00) | (Pico_mcd->cdd.Status & 0x00FF);

	CDD_Complete = 1;

	return 0;
}


int Stop_CDD_c1(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	if (CD_Present) Pico_mcd->scd.Status_CDD = STOPPED;
	else Pico_mcd->scd.Status_CDD = NOCD;
	Pico_mcd->cdd.Status = 0x0000;

	Pico_mcd->s68k_regs[0x36] |= 0x01;			// Data bit set because stopped

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Get_Pos_CDD_c20(void)
{
	_msf MSF;

	cdprintf("command 200 : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	cdprintf("Status CDD = %.4X  Status = %.4X", Pico_mcd->scd.Status_CDD, Pico_mcd->cdd.Status);

	LBA_to_MSF(Pico_mcd->scd.Cur_LBA, &MSF);

	Pico_mcd->cdd.Minute = INT_TO_BCDW(MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(MSF.F);
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Get_Track_Pos_CDD_c21(void)
{
	int elapsed_time;
	_msf MSF;

	cdprintf("command 201 : Cur LBA = %d", Pico_mcd->scd.Cur_LBA);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	elapsed_time = Pico_mcd->scd.Cur_LBA - Track_to_LBA(LBA_to_Track(Pico_mcd->scd.Cur_LBA));
	LBA_to_MSF(elapsed_time - 150, &MSF);

	cdprintf("   elapsed = %d", elapsed_time);

	Pico_mcd->cdd.Minute = INT_TO_BCDW(MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(MSF.F);
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Get_Current_Track_CDD_c22(void)
{
	cdprintf("Status CDD = %.4X  Status = %.4X", Pico_mcd->scd.Status_CDD, Pico_mcd->cdd.Status);

	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->scd.Cur_Track = LBA_to_Track(Pico_mcd->scd.Cur_LBA);

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Get_Total_Lenght_CDD_c23(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.TOC.Last_Track -
				Pico_mcd->scd.TOC.First_Track + 1].MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.TOC.Last_Track -
				Pico_mcd->scd.TOC.First_Track + 1].MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.TOC.Last_Track -
				Pico_mcd->scd.TOC.First_Track + 1].MSF.F);
	Pico_mcd->cdd.Ext = 0;

// FIXME: remove
//Pico_mcd->cdd.Seconde = 2;

	CDD_Complete = 1;

	return 0;
}


int Get_First_Last_Track_CDD_c24(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.TOC.First_Track);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->scd.TOC.Last_Track);
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

// FIXME: remove
//Pico_mcd->cdd.Minute = Pico_mcd->cdd.Seconde = 1;

	CDD_Complete = 1;

	return 0;
}


int Get_Track_Adr_CDD_c25(void)
{
	int track_number;

	CHECK_TRAY_OPEN

	// track number in TC4 & TC5

	track_number = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);

	Pico_mcd->cdd.Status &= 0xFF;
	if (!CD_Present)
	{
		Pico_mcd->scd.Status_CDD = NOCD;
		Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	}
//	else if (!(CDC.CTRL.B.B0 & 0x80)) Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;
	Pico_mcd->cdd.Status |= Pico_mcd->scd.Status_CDD;

	if (track_number > Pico_mcd->scd.TOC.Last_Track) track_number = Pico_mcd->scd.TOC.Last_Track;
	else if (track_number < Pico_mcd->scd.TOC.First_Track) track_number = Pico_mcd->scd.TOC.First_Track;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[track_number - Pico_mcd->scd.TOC.First_Track].MSF.M);
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[track_number - Pico_mcd->scd.TOC.First_Track].MSF.S);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(Pico_mcd->scd.TOC.Tracks[track_number - Pico_mcd->scd.TOC.First_Track].MSF.F);
	Pico_mcd->cdd.Ext = track_number % 10;

	if (Pico_mcd->scd.TOC.Tracks[track_number - Pico_mcd->scd.TOC.First_Track].Type) Pico_mcd->cdd.Frame |= 0x0800;

	CDD_Complete = 1;
	return 0;
}


int Play_CDD_c3(void)
{
	_msf MSF;
	int delay, new_lba;

	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	// MSF of the track to play in TC buffer

	MSF.M = (Pico_mcd->s68k_regs[0x38+10+2] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+3] & 0xF);
	MSF.S = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);
	MSF.F = (Pico_mcd->s68k_regs[0x38+10+6] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+7] & 0xF);

	Pico_mcd->scd.Cur_Track = MSF_to_Track(&MSF);

	new_lba = MSF_to_LBA(&MSF);
	delay = new_lba - Pico_mcd->scd.Cur_LBA;
	if (delay < 0) delay = -delay;
	delay >>= 12;

	Pico_mcd->scd.Cur_LBA = new_lba;
	CDC_Update_Header();

	cdprintf("Read : Cur LBA = %d, M=%d, S=%d, F=%d", Pico_mcd->scd.Cur_LBA, MSF.M, MSF.S, MSF.F);

	if (Pico_mcd->scd.Status_CDD != PLAYING) delay += 20;

	Pico_mcd->scd.Status_CDD = PLAYING;
	Pico_mcd->cdd.Status = 0x0102;
//	Pico_mcd->cdd.Status = COMM_OK;

	if (File_Add_Delay == 0) File_Add_Delay = delay;

	if (Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track].Type)
	{
		Pico_mcd->s68k_regs[0x36] |=  0x01;				// DATA
	}
	else
	{
		Pico_mcd->s68k_regs[0x36] &= ~0x01;				// AUDIO
		//CD_Audio_Starting = 1;
		FILE_Play_CD_LBA();
	}

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.Status_CDC |= 1;			// Read data with CDC

	CDD_Complete = 1;
	return 0;
}


int Seek_CDD_c4(void)
{
	_msf MSF;

	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	// MSF to seek in TC buffer

	MSF.M = (Pico_mcd->s68k_regs[0x38+10+2] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+3] & 0xF);
	MSF.S = (Pico_mcd->s68k_regs[0x38+10+4] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+5] & 0xF);
	MSF.F = (Pico_mcd->s68k_regs[0x38+10+6] & 0xF) * 10 + (Pico_mcd->s68k_regs[0x38+10+7] & 0xF);

	Pico_mcd->scd.Cur_Track = MSF_to_Track(&MSF);
	Pico_mcd->scd.Cur_LBA = MSF_to_LBA(&MSF);
	CDC_Update_Header();

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = 0x0200;

	// DATA ?
	if (Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track].Type)
	     Pico_mcd->s68k_regs[0x36] |=  0x01;
	else Pico_mcd->s68k_regs[0x36] &= ~0x01;		// AUDIO

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Pause_CDD_c6(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read to start a new one if raw data

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->s68k_regs[0x36] |= 0x01;		// Data bit set because stopped

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Resume_CDD_c7(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Cur_Track = LBA_to_Track(Pico_mcd->scd.Cur_LBA);

#ifdef DEBUG_CD
	{
		_msf MSF;
		LBA_to_MSF(Pico_mcd->scd.Cur_LBA, &MSF);
		cdprintf("Resume read : Cur LBA = %d, M=%d, S=%d, F=%d", Pico_mcd->scd.Cur_LBA, MSF.M, MSF.S, MSF.F);
	}
#endif

	Pico_mcd->scd.Status_CDD = PLAYING;
	Pico_mcd->cdd.Status = 0x0102;

	if (Pico_mcd->scd.TOC.Tracks[Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track].Type)
	{
		Pico_mcd->s68k_regs[0x36] |=  0x01;				// DATA
	}
	else
	{
		Pico_mcd->s68k_regs[0x36] &= ~0x01;				// AUDIO
		//CD_Audio_Starting = 1;
		FILE_Play_CD_LBA();
	}

	if (Pico_mcd->scd.Cur_Track == 100) Pico_mcd->cdd.Minute = 0x0A02;
	else Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	Pico_mcd->scd.Status_CDC |= 1;			// Read data with CDC

	CDD_Complete = 1;
	return 0;
}


int Fast_Foward_CDD_c8(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = FAST_FOW;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD | 2;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Fast_Rewind_CDD_c9(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;				// Stop CDC read

	Pico_mcd->scd.Status_CDD = FAST_REV;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD | 2;

	Pico_mcd->cdd.Minute = INT_TO_BCDW(Pico_mcd->scd.Cur_Track);
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int Close_Tray_CDD_cC(void)
{
	//Clear_Sound_Buffer();

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read

	{
#if 0 // TODO
		char new_iso[1024];

		memset(new_iso, 0, 1024);

		while (!Change_File_L(new_iso, Rom_Dir, "Load SegaCD image file", "SegaCD image file\0*.bin;*.iso;*.raw\0All files\0*.*\0\0", ""));
		Reload_SegaCD(new_iso);

		CD_Present = 1;
#else
		CD_Present = 0;
#endif
		Pico_mcd->scd.Status_CDD = STOPPED;
		Pico_mcd->cdd.Status = 0x0000;

		Pico_mcd->cdd.Minute = 0;
		Pico_mcd->cdd.Seconde = 0;
		Pico_mcd->cdd.Frame = 0;
		Pico_mcd->cdd.Ext = 0;
	}

	CDD_Complete = 1;

	return 0;
}


int Open_Tray_CDD_cD(void)
{
	CHECK_TRAY_OPEN

	Pico_mcd->scd.Status_CDC &= ~1;			// Stop CDC read

	Unload_ISO();
	CD_Present = 0;

	Pico_mcd->scd.Status_CDD = TRAY_OPEN;
	Pico_mcd->cdd.Status = 0x0E00;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int CDD_cA(void)
{
	CHECK_TRAY_OPEN
	CHECK_CD_PRESENT

	Pico_mcd->scd.Status_CDC &= ~1;

	Pico_mcd->scd.Status_CDD = READY;
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = INT_TO_BCDW(1);
	Pico_mcd->cdd.Frame = INT_TO_BCDW(1);
	Pico_mcd->cdd.Ext = 0;

	CDD_Complete = 1;

	return 0;
}


int CDD_Def(void)
{
	Pico_mcd->cdd.Status = Pico_mcd->scd.Status_CDD;

	Pico_mcd->cdd.Minute = 0;
	Pico_mcd->cdd.Seconde = 0;
	Pico_mcd->cdd.Frame = 0;
	Pico_mcd->cdd.Ext = 0;

	return 0;
}




/***************************
 *   Others CD functions   *
 **************************/


// do we need them?
#if 0
void Write_CD_Audio(short *Buf, int rate, int channel, int lenght)
{
	unsigned int lenght_src, lenght_dst;
	unsigned int pos_src, pas_src;

	if (rate == 0) return;
	if (Sound_Rate == 0) return;

	if (CD_Audio_Starting)
	{
		CD_Audio_Starting = 0;
		memset(CD_Audio_Buffer_L, 0, 4096 * 4);
		memset(CD_Audio_Buffer_R, 0, 4096 * 4);
		CD_Audio_Buffer_Write_Pos = (CD_Audio_Buffer_Read_Pos + 2000) & 0xFFF;
	}

	lenght_src = rate / 75;				// 75th of a second
	lenght_dst = Sound_Rate / 75;		// 75th of a second

	pas_src = (lenght_src << 16) / lenght_dst;
	pos_src = 0;

#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "\n*********  Write Pos = %d    ", CD_Audio_Buffer_Write_Pos);
#endif

	if (channel == 2)
	{
		__asm
		{
			mov edi, CD_Audio_Buffer_Write_Pos
			mov ebx, Buf
			xor esi, esi
			mov ecx, lenght_dst
			xor eax, eax
			mov edx, pas_src
			dec ecx
			jmp short loop_stereo

align 16

loop_stereo:
			movsx eax, word ptr [ebx + esi * 4]
			mov CD_Audio_Buffer_L[edi * 4], eax
			movsx eax, word ptr [ebx + esi * 4 + 2]
			mov CD_Audio_Buffer_R[edi * 4], eax
			mov esi, dword ptr pos_src
			inc edi
			add esi, edx
			and edi, 0xFFF
			mov dword ptr pos_src, esi
			shr esi, 16
			dec ecx
			jns short loop_stereo

			mov CD_Audio_Buffer_Write_Pos, edi
		}
	}
	else
	{
		__asm
		{
			mov edi, CD_Audio_Buffer_Write_Pos
			mov ebx, Buf
			xor esi, esi
			mov ecx, lenght_dst
			xor eax, eax
			mov edx, pas_src
			dec ecx
			jmp short loop_mono

align 16

loop_mono:
			movsx eax, word ptr [ebx + esi * 2]
			mov CD_Audio_Buffer_L[edi * 4], eax
			mov CD_Audio_Buffer_R[edi * 4], eax
			mov esi, dword ptr pos_src
			inc edi
			add esi, edx
			and edi, 0xFFF
			mov dword ptr pos_src, esi
			shr esi, 16
			dec ecx
			jns short loop_mono

			mov CD_Audio_Buffer_Write_Pos, edi
		}
	}

#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Write Pos 2 = %d\n\n", CD_Audio_Buffer_Write_Pos);
#endif
}


void Update_CD_Audio(int **buf, int lenght)
{
	int *Buf_L, *Buf_R;
	int diff;

	Buf_L = buf[0];
	Buf_R = buf[1];

	if (Pico_mcd->s68k_regs[0x36] & 0x01) return;
	if (!(Pico_mcd->scd.Status_CDC & 1))  return;
	if (CD_Audio_Starting) return;

#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "\n*********  Read Pos Normal = %d     ", CD_Audio_Buffer_Read_Pos);
#endif

	if (CD_Audio_Buffer_Write_Pos < CD_Audio_Buffer_Read_Pos)
	{
		diff = CD_Audio_Buffer_Write_Pos + (4096) - CD_Audio_Buffer_Read_Pos;
	}
	else
	{
		diff = CD_Audio_Buffer_Write_Pos - CD_Audio_Buffer_Read_Pos;
	}

	if (diff < 500) CD_Audio_Buffer_Read_Pos -= 2000;
	else if (diff > 3500) CD_Audio_Buffer_Read_Pos += 2000;

#ifdef DEBUG_CD
	else fprintf(debug_SCD_file, " pas de modifs   ");
#endif

	CD_Audio_Buffer_Read_Pos &= 0xFFF;

#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Read Pos = %d   ", CD_Audio_Buffer_Read_Pos);
#endif

	if (CDDA_Enable)
	{
		__asm
		{
			mov ecx, lenght
			mov esi, CD_Audio_Buffer_Read_Pos
			mov edi, Buf_L
			dec ecx

loop_L:
			mov eax, CD_Audio_Buffer_L[esi * 4]
			add [edi], eax
			inc esi
			add edi, 4
			and esi, 0xFFF
			dec ecx
			jns short loop_L

			mov ecx, lenght
			mov esi, CD_Audio_Buffer_Read_Pos
			mov edi, Buf_R
			dec ecx

loop_R:
			mov eax, CD_Audio_Buffer_R[esi * 4]
			add [edi], eax
			inc esi
			add edi, 4
			and esi, 0xFFF
			dec ecx
			jns short loop_R

			mov CD_Audio_Buffer_Read_Pos, esi
		}
	}
	else
	{
		CD_Audio_Buffer_Read_Pos += lenght;
		CD_Audio_Buffer_Read_Pos &= 0xFFF;
	}

#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Read Pos 2 = %d\n\n", CD_Audio_Buffer_Read_Pos);
#endif
}
#endif

