/*
#include <stdio.h>
#include <string.h>
#if defined(__WIN__)
#include <windows.h>
#else
#include "port.h"
#endif
#include "cd_sys.h"
#include "cd_file.h"
#include "lc89510.h"
#include "cdda_mp3.h"
#include "star_68k.h"
#include "rom.h"
#include "mem_s68k.h"
*/

#include <sys/stat.h>
#include "cd_file.h"

#include "../PicoInt.h"

#define cdprintf printf
//#define cdprintf(x...)

struct _file_track Tracks[100];
char Track_Played;


int FILE_Init(void)
{
//	MP3_Init(); // TODO
	Unload_ISO();

	return 0;
}


void FILE_End(void)
{
	Unload_ISO();
}


int Load_ISO(const char *iso_name, int is_bin)
{
	struct stat file_stat;
	int i, j, num_track, Cur_LBA, index, ret;
	_scd_track *SCD_TOC_Tracks = Pico_mcd->scd.TOC.Tracks;
	FILE *tmp_file;
	char tmp_name[1024], tmp_ext[10];
	static char *exts[] = {
		"%02d.mp3", " %02d.mp3", "-%02d.mp3", "_%02d.mp3", " - %02d.mp3",
		"%d.mp3", " %d.mp3", "-%d.mp3", "_%d.mp3", " - %d.mp3",
		/* "%02d.wav", " %02d.wav", "-%02d.wav", "_%02d.wav", " - %02d.wav",
		"%d.wav", " %d.wav", "-%d.wav", "_%d.wav", " - %2d.wav" */
	};

	Unload_ISO();

	Tracks[0].Type = is_bin ? TYPE_BIN : TYPE_ISO;

	ret = stat(iso_name, &file_stat);
	if (ret != 0) return -1;

	Tracks[0].Lenght = file_stat.st_size;

	if (Tracks[0].Type == TYPE_ISO) Tracks[0].Lenght >>= 11;	// size in sectors
	else Tracks[0].Lenght /= 2352;					// size in sectors


	Tracks[0].F = fopen(iso_name, "rb");
	if (Tracks[0].F == NULL)
	{
		Tracks[0].Type = 0;
		Tracks[0].Lenght = 0;
		return -1;
	}

	if (Tracks[0].Type == TYPE_ISO) fseek(Tracks[0].F, 0x100, SEEK_SET);
	else fseek(Tracks[0].F, 0x110, SEEK_SET);

	// fread(buf, 1, 0x200, Tracks[0].F);
	fseek(Tracks[0].F, 0, SEEK_SET);

	Pico_mcd->scd.TOC.First_Track = 1;

	SCD_TOC_Tracks[0].Num = 1;
	SCD_TOC_Tracks[0].Type = 1;				// DATA

	SCD_TOC_Tracks[0].MSF.M = 0;
	SCD_TOC_Tracks[0].MSF.S = 2;
	SCD_TOC_Tracks[0].MSF.F = 0;

	cdprintf("\nTrack 0 - %02d:%02d:%02d %s\n", SCD_TOC_Tracks[0].MSF.M, SCD_TOC_Tracks[0].MSF.S, SCD_TOC_Tracks[0].MSF.F,
		SCD_TOC_Tracks[0].Type ? "DATA" : "AUDIO");

	Cur_LBA = Tracks[0].Lenght;				// Size in sectors

	strcpy(tmp_name, iso_name);

	for(num_track = 2, i = 0; i < 100; i++)
	{
		if (sizeof(exts)/sizeof(char *) != 10) { printf("eee"); exit(1); }

		for(j = 0; j < sizeof(exts)/sizeof(char *); j++)
		{
			tmp_name[strlen(iso_name) - 4] = 0;
			sprintf(tmp_ext, exts[j], i);
			strcat(tmp_name, tmp_ext);

			tmp_file = fopen(tmp_name, "rb");

			if (tmp_file)
			{
				float fs;
				index = num_track - Pico_mcd->scd.TOC.First_Track;

				stat(tmp_name, &file_stat);

				fs = (float) file_stat.st_size;				// used to calculate lenght

				Tracks[index].F = tmp_file;

				SCD_TOC_Tracks[index].Num = num_track;
				SCD_TOC_Tracks[index].Type = 0;				// AUDIO

				LBA_to_MSF(Cur_LBA, &(SCD_TOC_Tracks[index].MSF));

				cdprintf("\nTrack %i - %02d:%02d:%02d %s\n", index, SCD_TOC_Tracks[index].MSF.M,
					SCD_TOC_Tracks[index].MSF.S, SCD_TOC_Tracks[index].MSF.F,
					SCD_TOC_Tracks[index].Type ? "DATA" : "AUDIO");

				// MP3 File
				Tracks[index].Type = TYPE_MP3;
				fs /= (128>>3); // (float) (MP3_Get_Bitrate(Tracks[num_track - 1].F) >> 3);
				fs *= 75;
				Tracks[index].Lenght = (int) fs;
				Cur_LBA += Tracks[index].Lenght;

				num_track++;
				break;
			}
		}
	}

	Pico_mcd->scd.TOC.Last_Track = num_track - 1;

	index = num_track - Pico_mcd->scd.TOC.First_Track;
	SCD_TOC_Tracks[index].Num = num_track;
	SCD_TOC_Tracks[index].Type = 0;

	LBA_to_MSF(Cur_LBA, &(SCD_TOC_Tracks[index].MSF));

	cdprintf("End CD - %02d:%02d:%02d\n\n", SCD_TOC_Tracks[index].MSF.M,
		SCD_TOC_Tracks[index].MSF.S, SCD_TOC_Tracks[index].MSF.F);

	return 0;
}


void Unload_ISO(void)
{
	int i;

	Track_Played = 99;

	for(i = 0; i < 100; i++)
	{
		if (Tracks[i].F) fclose(Tracks[i].F);
		Tracks[i].F = NULL;
		Tracks[i].Lenght = 0;
		Tracks[i].Type = 0;
	}
}


int FILE_Read_One_LBA_CDC(void)
{
	int where_read;
	static char cp_buf[2560];

	if (Pico_mcd->s68k_regs[0x36] & 1)					// DATA
	{
		if (Tracks[0].F == NULL) return -1;

		if (Pico_mcd->scd.Cur_LBA < 0) where_read = 0;
		else if (Pico_mcd->scd.Cur_LBA >= Tracks[0].Lenght) where_read = Tracks[0].Lenght - 1;
		else where_read = Pico_mcd->scd.Cur_LBA;

		if (Tracks[0].Type == TYPE_ISO) where_read <<= 11;
		else where_read = (where_read * 2352 + 16);

		fseek(Tracks[0].F, where_read, SEEK_SET);
		fread(cp_buf, 1, 2048, Tracks[0].F);

		cdprintf("\n\nRead file CDC 1 data sector :\n");
	}
	else									// AUDIO
	{
		// int rate, channel;

		if (Tracks[Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track].Type == TYPE_MP3)
		{
			// TODO
			// MP3_Update(cp_buf, &rate, &channel, 0);
			// Write_CD_Audio((short *) cp_buf, rate, channel, 588);
		}

		cdprintf("\n\nRead file CDC 1 audio sector :\n");
	}

	// Update CDC stuff

	CDC_Update_Header();

	if (Pico_mcd->s68k_regs[0x36] & 1)		// DATA track
	{
		if (Pico_mcd->cdc.CTRL.B.B0 & 0x80)		// DECEN = decoding enable
		{
			if (Pico_mcd->cdc.CTRL.B.B0 & 0x04)	// WRRQ : this bit enable write to buffer
			{
				// CAUTION : lookahead bit not implemented

				Pico_mcd->scd.Cur_LBA++;

				Pico_mcd->cdc.WA.N = (Pico_mcd->cdc.WA.N + 2352) & 0x7FFF;		// add one sector to WA
				Pico_mcd->cdc.PT.N = (Pico_mcd->cdc.PT.N + 2352) & 0x7FFF;

				memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N + 4], cp_buf, 2048);
				memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N], &Pico_mcd->cdc.HEAD, 4);

#ifdef DEBUG_CD
				cdprintf("\nRead -> WA = %d  Buffer[%d] =\n", Pico_mcd->cdc.WA.N, Pico_mcd->cdc.PT.N & 0x3FFF);
				cdprintf("Header 1 = %.2X %.2X %.2X %.2X\n", Pico_mcd->cdc.HEAD.B.B0,
					Pico_mcd->cdc.HEAD.B.B1, Pico_mcd->cdc.HEAD.B.B2, Pico_mcd->cdc.HEAD.B.B3);
				cdprintf("Header 2 = %.2X %.2X %.2X %.2X --- %.2X %.2X\n\n",
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 0) & 0x3FFF],
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 1) & 0x3FFF],
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 2) & 0x3FFF],
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 3) & 0x3FFF],
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 4) & 0x3FFF],
					Pico_mcd->cdc.Buffer[(Pico_mcd->cdc.PT.N + 5) & 0x3FFF]);
#endif
			}

		}
	}
	else		// music track
	{
		Pico_mcd->scd.Cur_LBA++;

		Pico_mcd->cdc.WA.N = (Pico_mcd->cdc.WA.N + 2352) & 0x7FFF;		// add one sector to WA
		Pico_mcd->cdc.PT.N = (Pico_mcd->cdc.PT.N + 2352) & 0x7FFF;

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x80)		// DECEN = decoding enable
		{
			if (Pico_mcd->cdc.CTRL.B.B0 & 0x04)	// WRRQ : this bit enable write to buffer
			{
				// CAUTION : lookahead bit not implemented

				memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N], cp_buf, 2352);
			}
		}
	}

	if (Pico_mcd->cdc.CTRL.B.B0 & 0x80)		// DECEN = decoding enable
	{
		Pico_mcd->cdc.STAT.B.B0 = 0x80;

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x10)	// determine form bit form sub header ?
		{
			Pico_mcd->cdc.STAT.B.B2 = Pico_mcd->cdc.CTRL.B.B1 & 0x08;
		}
		else
		{
			Pico_mcd->cdc.STAT.B.B2 = Pico_mcd->cdc.CTRL.B.B1 & 0x0C;
		}

		if (Pico_mcd->cdc.CTRL.B.B0 & 0x02) Pico_mcd->cdc.STAT.B.B3 = 0x20;	// ECC done
		else Pico_mcd->cdc.STAT.B.B3 = 0x00;	// ECC not done

		if (Pico_mcd->cdc.IFCTRL & 0x20)
		{
			if (Pico_mcd->s68k_regs[0x33] & (1<<5))
			{
				dprintf("cdc dec irq 5");
				SekInterruptS68k(5);
			}

			Pico_mcd->cdc.IFSTAT &= ~0x20;	// DEC interrupt happen
			CDC_Decode_Reg_Read = 0;	// Reset read after DEC int
		}
	}


	return 0;
}


int FILE_Play_CD_LBA(void)
{
	int index = Pico_mcd->scd.Cur_Track - Pico_mcd->scd.TOC.First_Track;

	cdprintf("Play FILE Comp\n");

	if (Tracks[index].F == NULL)
	{
		return 1;
	}

	if (Tracks[index].Type == TYPE_MP3)
	{
		int Track_LBA_Pos = Pico_mcd->scd.Cur_LBA - Track_to_LBA(Pico_mcd->scd.Cur_Track);
		if (Track_LBA_Pos < 0) Track_LBA_Pos = 0;

		// MP3_Play(index, Track_LBA_Pos); // TODO
	}
	else
	{
		return 3;
	}

	return 0;
}

