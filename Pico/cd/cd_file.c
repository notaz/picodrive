/***********************************************************
 *                                                         *
 * This source was taken from the Gens project             *
 * Written by Stéphane Dallongeville                       *
 * Copyright (c) 2002 by Stéphane Dallongeville            *
 * Modified/adapted for PicoDrive by notaz, 2007           *
 *                                                         *
 ***********************************************************/

#include "../PicoInt.h"
#include "cd_file.h"

#define cdprintf(x...)
//#define cdprintf(f,...) printf(f "\n",##__VA_ARGS__) // tmp
#define DEBUG_CD

PICO_INTERNAL int Load_ISO(const char *iso_name, int is_bin)
{
	int i, j, num_track, Cur_LBA, index, ret, iso_name_len;
	_scd_track *Tracks = Pico_mcd->TOC.Tracks;
	char tmp_name[1024], tmp_ext[10];
	pm_file *pmf;
	static char *exts[] = {
		"%02d.mp3", " %02d.mp3", "-%02d.mp3", "_%02d.mp3", " - %02d.mp3",
		"%d.mp3", " %d.mp3", "-%d.mp3", "_%d.mp3", " - %d.mp3",
#if CASE_SENSITIVE_FS
		"%02d.MP3", " %02d.MP3", "-%02d.MP3", "_%02d.MP3", " - %02d.MP3",
#endif
	};

	if (PicoCDLoadProgressCB != NULL) PicoCDLoadProgressCB(1);

	Unload_ISO();

	Tracks[0].ftype = is_bin ? TYPE_BIN : TYPE_ISO;

	Tracks[0].F = pmf = pm_open(iso_name);
	if (Tracks[0].F == NULL)
	{
		Tracks[0].ftype = 0;
		Tracks[0].Length = 0;
		return -1;
	}

	if (Tracks[0].ftype == TYPE_ISO)
		Tracks[0].Length = pmf->size >>= 11;	// size in sectors
	else	Tracks[0].Length = pmf->size /= 2352;

	Tracks[0].MSF.M = 0; // minutes
	Tracks[0].MSF.S = 2; // seconds
	Tracks[0].MSF.F = 0; // frames

	cdprintf("Track 0 - %02d:%02d:%02d DATA", Tracks[0].MSF.M, Tracks[0].MSF.S, Tracks[0].MSF.F);

	Cur_LBA = Tracks[0].Length;				// Size in sectors

	iso_name_len = strlen(iso_name);
	if (iso_name_len >= sizeof(tmp_name))
		iso_name_len = sizeof(tmp_name) - 1;

	for (num_track = 2, i = 0; i < 100; i++)
	{
		if (PicoCDLoadProgressCB != NULL && i > 1) PicoCDLoadProgressCB(i);

		for (j = 0; j < sizeof(exts)/sizeof(char *); j++)
		{
			int ext_len;
			FILE *tmp_file;
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
				int fs;
				index = num_track - 1;

				ret = fseek(tmp_file, 0, SEEK_END);
				fs = ftell(tmp_file);				// used to calculate lenght
				fseek(tmp_file, 0, SEEK_SET);

#if DONT_OPEN_MANY_FILES
				// some systems (like PSP) can't have many open files at a time,
				// so we work with their names instead.
				fclose(tmp_file);
				tmp_file = (void *) strdup(tmp_name);
#endif
				Tracks[index].KBtps = (short) mp3_get_bitrate(tmp_file, fs);
				Tracks[index].KBtps >>= 3;
				if (ret != 0 || Tracks[index].KBtps <= 0)
				{
					cdprintf("Error track %i: rate %i", index, Tracks[index].KBtps);
#if !DONT_OPEN_MANY_FILES
					fclose(tmp_file);
#else
					free(tmp_file);
#endif
					continue;
				}

				Tracks[index].F = tmp_file;

				LBA_to_MSF(Cur_LBA, &Tracks[index].MSF);

				// MP3 File
				Tracks[index].ftype = TYPE_MP3;
				fs *= 75;
				fs /= Tracks[index].KBtps * 1000;
				Tracks[index].Length = fs;
				Cur_LBA += Tracks[index].Length;

				cdprintf("Track %i: %s - %02d:%02d:%02d len=%i AUDIO", index, tmp_name, Tracks[index].MSF.M,
					Tracks[index].MSF.S, Tracks[index].MSF.F, fs);

				num_track++;
				break;
			}
		}
	}

	Pico_mcd->TOC.Last_Track = num_track - 1;

	index = num_track - 1;

	LBA_to_MSF(Cur_LBA, &Tracks[index].MSF);

	cdprintf("End CD - %02d:%02d:%02d\n\n", Tracks[index].MSF.M,
		Tracks[index].MSF.S, Tracks[index].MSF.F);

	if (PicoCDLoadProgressCB != NULL) PicoCDLoadProgressCB(100);

	return 0;
}


PICO_INTERNAL void Unload_ISO(void)
{
	int i;

	if (Pico_mcd == NULL) return;

	if (Pico_mcd->TOC.Tracks[0].F) pm_close(Pico_mcd->TOC.Tracks[0].F);

	for(i = 1; i < 100; i++)
	{
		if (Pico_mcd->TOC.Tracks[i].F != NULL)
#if !DONT_OPEN_MANY_FILES
			fclose(Pico_mcd->TOC.Tracks[i].F);
#else
			free(Pico_mcd->TOC.Tracks[i].F);
#endif
	}
	memset(Pico_mcd->TOC.Tracks, 0, sizeof(Pico_mcd->TOC.Tracks));
}


PICO_INTERNAL int FILE_Read_One_LBA_CDC(void)
{
//	static char cp_buf[2560];

	if (Pico_mcd->s68k_regs[0x36] & 1)					// DATA
	{
		if (Pico_mcd->TOC.Tracks[0].F == NULL) return -1;

		// moved below..
		//fseek(Pico_mcd->TOC.Tracks[0].F, where_read, SEEK_SET);
		//fread(cp_buf, 1, 2048, Pico_mcd->TOC.Tracks[0].F);

		cdprintf("Read file CDC 1 data sector :\n");
	}
	else									// AUDIO
	{
		// int rate, channel;

		// if (Pico_mcd->TOC.Tracks[Pico_mcd->scd.Cur_Track - 1].ftype == TYPE_MP3)
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
				int where_read = 0;

				// CAUTION : lookahead bit not implemented

				if (Pico_mcd->scd.Cur_LBA < 0)
					where_read = 0;
				else if (Pico_mcd->scd.Cur_LBA >= Pico_mcd->TOC.Tracks[0].Length)
					where_read = Pico_mcd->TOC.Tracks[0].Length - 1;
				else where_read = Pico_mcd->scd.Cur_LBA;

				Pico_mcd->scd.Cur_LBA++;

				Pico_mcd->cdc.WA.N = (Pico_mcd->cdc.WA.N + 2352) & 0x7FFF;		// add one sector to WA
				Pico_mcd->cdc.PT.N = (Pico_mcd->cdc.PT.N + 2352) & 0x7FFF;

				*(unsigned int *)(Pico_mcd->cdc.Buffer + Pico_mcd->cdc.PT.N) = Pico_mcd->cdc.HEAD.N;
				//memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N + 4], cp_buf, 2048);

				//pm_seek(Pico_mcd->TOC.Tracks[0].F, where_read, SEEK_SET);
				//pm_read(Pico_mcd->cdc.Buffer + Pico_mcd->cdc.PT.N + 4, 2048, Pico_mcd->TOC.Tracks[0].F);
				PicoCDBufferRead(Pico_mcd->cdc.Buffer + Pico_mcd->cdc.PT.N + 4, where_read);

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

				//memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N], cp_buf, 2352);
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
				elprintf(EL_INTS, "cdc dec irq 5");
				SekInterruptS68k(5);
			}

			Pico_mcd->cdc.IFSTAT &= ~0x20;		// DEC interrupt happen
			Pico_mcd->cdc.Decode_Reg_Read = 0;	// Reset read after DEC int
		}
	}


	return 0;
}


PICO_INTERNAL int FILE_Play_CD_LBA(void)
{
	int index = Pico_mcd->scd.Cur_Track - 1;
	Pico_mcd->m.audio_track = index;

	cdprintf("Play track #%i", Pico_mcd->scd.Cur_Track);

	if (Pico_mcd->TOC.Tracks[index].F == NULL)
	{
		return 1;
	}

	if (Pico_mcd->TOC.Tracks[index].ftype == TYPE_MP3)
	{
		int pos1024 = 0;
		int Track_LBA_Pos = Pico_mcd->scd.Cur_LBA - Track_to_LBA(Pico_mcd->scd.Cur_Track);
		if (Track_LBA_Pos < 0) Track_LBA_Pos = 0;
		if (Track_LBA_Pos)
			pos1024 = Track_LBA_Pos * 1024 / Pico_mcd->TOC.Tracks[index].Length;

		mp3_start_play(Pico_mcd->TOC.Tracks[index].F, pos1024);
	}
	else
	{
		return 3;
	}

	return 0;
}

