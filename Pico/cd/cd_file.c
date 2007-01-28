#include <sys/stat.h>
#include "cd_file.h"

#include "../PicoInt.h"

#define cdprintf dprintf
//#define cdprintf(x...)
#define DEBUG_CD

// TODO: check refs, move 2 context
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
	int i, j, num_track, Cur_LBA, index, ret, iso_name_len;
	_scd_track *SCD_TOC_Tracks = Pico_mcd->scd.TOC.Tracks;
	FILE *tmp_file;
	char tmp_name[1024], tmp_ext[10];
	static char *exts[] = {
		"%02d.mp3", " %02d.mp3", "-%02d.mp3", "_%02d.mp3", " - %02d.mp3",
		"%d.mp3", " %d.mp3", "-%d.mp3", "_%d.mp3", " - %d.mp3",
		"%02d.MP3", " %02d.MP3", "-%02d.MP3", "_%02d.MP3", " - %02d.MP3",
		/* "%02d.wav", " %02d.wav", "-%02d.wav", "_%02d.wav", " - %02d.wav",
		"%d.wav", " %d.wav", "-%d.wav", "_%d.wav", " - %2d.wav" */
	};

	Unload_ISO();

	Tracks[0].Type = is_bin ? TYPE_BIN : TYPE_ISO;

	ret = stat(iso_name, &file_stat);
	if (ret != 0) return -1;

	Tracks[0].Length = file_stat.st_size;

	if (Tracks[0].Type == TYPE_ISO) Tracks[0].Length >>= 11;	// size in sectors
	else Tracks[0].Length /= 2352;					// size in sectors


	Tracks[0].F = fopen(iso_name, "rb");
	if (Tracks[0].F == NULL)
	{
		Tracks[0].Type = 0;
		Tracks[0].Length = 0;
		return -1;
	}

	if (Tracks[0].Type == TYPE_ISO) fseek(Tracks[0].F, 0x100, SEEK_SET);
	else fseek(Tracks[0].F, 0x110, SEEK_SET);

	// fread(buf, 1, 0x200, Tracks[0].F);
	fseek(Tracks[0].F, 0, SEEK_SET);

	Pico_mcd->scd.TOC.First_Track = 1;

	SCD_TOC_Tracks[0].Num = 1;
	SCD_TOC_Tracks[0].Type = 1;				// DATA

	SCD_TOC_Tracks[0].MSF.M = 0; // minutes
	SCD_TOC_Tracks[0].MSF.S = 2; // seconds
	SCD_TOC_Tracks[0].MSF.F = 0; // frames

	cdprintf("Track 0 - %02d:%02d:%02d %s", SCD_TOC_Tracks[0].MSF.M, SCD_TOC_Tracks[0].MSF.S, SCD_TOC_Tracks[0].MSF.F,
		SCD_TOC_Tracks[0].Type ? "DATA" : "AUDIO");

	Cur_LBA = Tracks[0].Length;				// Size in sectors

	strcpy(tmp_name, iso_name);
	iso_name_len = strlen(iso_name);

	for (num_track = 2, i = 0; i < 100; i++)
	{
		for(j = 0; j < sizeof(exts)/sizeof(char *); j++)
		{
			int ext_len;
			sprintf(tmp_ext, exts[j], i);
			ext_len = strlen(tmp_ext);

			memcpy(tmp_name, iso_name, iso_name_len + 1);
			tmp_name[iso_name_len - 4] = 0;
			strcat(tmp_name, tmp_ext);

			tmp_file = fopen(tmp_name, "rb");
			if (!tmp_file && i > 1 && iso_name_len > ext_len) {
				tmp_name[iso_name_len - ext_len] = 0;
				strcat(tmp_name, tmp_ext);
				tmp_file = fopen(tmp_name, "rb");
			}

			if (tmp_file)
			{
				// float fs;
				int fs;
				index = num_track - Pico_mcd->scd.TOC.First_Track;

				ret = stat(tmp_name, &file_stat);
				fs = file_stat.st_size;				// used to calculate lenght

				Tracks[index].KBtps = (short) mp3_get_bitrate(tmp_file, fs);
				Tracks[index].KBtps >>= 3;
				if (ret != 0 || Tracks[index].KBtps <= 0)
				{
					cdprintf("Error track %i: stat %i, rate %i", index, ret, Tracks[index].KBtps);
					fclose(tmp_file);
					continue;
				}

				Tracks[index].F = tmp_file;

				SCD_TOC_Tracks[index].Num = num_track;
				SCD_TOC_Tracks[index].Type = 0;				// AUDIO

				LBA_to_MSF(Cur_LBA, &(SCD_TOC_Tracks[index].MSF));

				// MP3 File
				Tracks[index].Type = TYPE_MP3;
				fs *= 75;
				fs /= Tracks[index].KBtps * 1000;
				Tracks[index].Length = fs;
				Cur_LBA += Tracks[index].Length;

				cdprintf("Track %i: %s - %02d:%02d:%02d len=%i %s", index, tmp_name, SCD_TOC_Tracks[index].MSF.M,
					SCD_TOC_Tracks[index].MSF.S, SCD_TOC_Tracks[index].MSF.F, fs,
					SCD_TOC_Tracks[index].Type ? "DATA" : "AUDIO");

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
		Tracks[i].Length = 0;
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
		else if (Pico_mcd->scd.Cur_LBA >= Tracks[0].Length) where_read = Tracks[0].Length - 1;
		else where_read = Pico_mcd->scd.Cur_LBA;

		if (Tracks[0].Type == TYPE_ISO) where_read <<= 11;
		else where_read = (where_read * 2352 + 16);

		fseek(Tracks[0].F, where_read, SEEK_SET);
		fread(cp_buf, 1, 2048, Tracks[0].F);

		cdprintf("Read file CDC 1 data sector :\n");
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

		cdprintf("Read file CDC 1 audio sector :\n");
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
				cdprintf("Read -> WA = %d  Buffer[%d] =", Pico_mcd->cdc.WA.N, Pico_mcd->cdc.PT.N & 0x3FFF);
				cdprintf("Header 1 = %.2X %.2X %.2X %.2X", Pico_mcd->cdc.HEAD.B.B0,
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

	cdprintf("Play FILE Comp");

	if (Tracks[index].F == NULL)
	{
		return 1;
	}

	if (Tracks[index].Type == TYPE_MP3)
	{
		int pos1000 = 0;
		int Track_LBA_Pos = Pico_mcd->scd.Cur_LBA - Track_to_LBA(Pico_mcd->scd.Cur_Track);
		if (Track_LBA_Pos < 0) Track_LBA_Pos = 0;
		if (Track_LBA_Pos)
			pos1000 = Track_LBA_Pos * 1024 / Tracks[index].Length;

		mp3_start_play(Tracks[index].F, pos1000);
	}
	else
	{
		return 3;
	}

	return 0;
}

