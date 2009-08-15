/***********************************************************
 *                                                         *
 * This source was taken from the Gens project             *
 * Written by Stéphane Dallongeville                       *
 * Copyright (c) 2002 by Stéphane Dallongeville            *
 * Modified/adapted for PicoDrive by notaz, 2007           *
 *                                                         *
 ***********************************************************/

#include "../pico_int.h"
#include "cd_file.h"
#include "cue.h"

//#define cdprintf(f,...) printf(f "\n",##__VA_ARGS__) // tmp

static int audio_track_mp3(const char *fname, int index)
{
	_scd_track *Tracks = Pico_mcd->TOC.Tracks;
	FILE *tmp_file;
	int fs, ret;

	tmp_file = fopen(fname, "rb");
	if (tmp_file == NULL)
		return -1;

	ret = fseek(tmp_file, 0, SEEK_END);
	fs = ftell(tmp_file);				// used to calculate length
	fseek(tmp_file, 0, SEEK_SET);

#if DONT_OPEN_MANY_FILES
	// some systems (like PSP) can't have many open files at a time,
	// so we work with their names instead.
	fclose(tmp_file);
	tmp_file = (void *) strdup(fname);
#endif
	Tracks[index].KBtps = (short) mp3_get_bitrate(tmp_file, fs);
	Tracks[index].KBtps >>= 3;
	if (ret != 0 || Tracks[index].KBtps <= 0)
	{
		elprintf(EL_STATUS, "track %2i: mp3 bitrate %i", index+1, Tracks[index].KBtps);
#if !DONT_OPEN_MANY_FILES
		fclose(tmp_file);
#else
		free(tmp_file);
#endif
		return -1;
	}

	Tracks[index].F = tmp_file;

	// MP3 File
	Tracks[index].ftype = TYPE_MP3;
	fs *= 75;
	fs /= Tracks[index].KBtps * 1000;
	Tracks[index].Length = fs;
	Tracks[index].Offset = 0;

	return 0;
}

PICO_INTERNAL int Load_CD_Image(const char *cd_img_name, cd_img_type type)
{
	int i, j, num_track, Cur_LBA, index, ret, iso_name_len, missed, cd_img_sectors;
	_scd_track *Tracks = Pico_mcd->TOC.Tracks;
	char tmp_name[1024], tmp_ext[10];
	cue_data_t *cue_data = NULL;
	pm_file *pmf;
	static char *exts[] = {
		"%02d.mp3", " %02d.mp3", "-%02d.mp3", "_%02d.mp3", " - %02d.mp3",
		"%d.mp3", " %d.mp3", "-%d.mp3", "_%d.mp3", " - %d.mp3",
#if CASE_SENSITIVE_FS
		"%02d.MP3", " %02d.MP3", "-%02d.MP3", "_%02d.MP3", " - %02d.MP3",
#endif
	};

	if (PicoCDLoadProgressCB != NULL)
		PicoCDLoadProgressCB(cd_img_name, 1);

	Unload_ISO();

	/* is this .cue? */
	ret = strlen(cd_img_name);
	if (ret >= 3 && strcasecmp(cd_img_name + ret - 3, "cue") == 0)
		cue_data = cue_parse(cd_img_name);
	if (cue_data != NULL)
		cd_img_name = cue_data->tracks[1].fname;

	Tracks[0].ftype = type == CIT_BIN ? TYPE_BIN : TYPE_ISO;

	Tracks[0].F = pmf = pm_open(cd_img_name);
	if (Tracks[0].F == NULL)
	{
		Tracks[0].ftype = 0;
		Tracks[0].Length = 0;
		if (cue_data != NULL)
			cue_destroy(cue_data);
		return -1;
	}

	if (Tracks[0].ftype == TYPE_ISO)
	     cd_img_sectors = pmf->size >>= 11;	// size in sectors
	else cd_img_sectors = pmf->size /= 2352;
	Tracks[0].Offset = 0;

	Tracks[0].MSF.M = 0; // minutes
	Tracks[0].MSF.S = 2; // seconds
	Tracks[0].MSF.F = 0; // frames

	elprintf(EL_STATUS, "Track  1: %02d:%02d:%02d %9i DATA",
		Tracks[0].MSF.M, Tracks[0].MSF.S, Tracks[0].MSF.F, Tracks[0].Length);

	Cur_LBA = Tracks[0].Length = cd_img_sectors;

	if (cue_data != NULL)
	{
		if (cue_data->tracks[2].fname == NULL) { // NULL means track2 is in same file as track1
			Cur_LBA = Tracks[0].Length = cue_data->tracks[2].sector_offset;
		}
		i = 100 / cue_data->track_count+1;
		for (num_track = 2; num_track <= cue_data->track_count; num_track++)
		{
			if (PicoCDLoadProgressCB != NULL)
				PicoCDLoadProgressCB(cd_img_name, i * num_track);
			index = num_track - 1;
			Cur_LBA += cue_data->tracks[num_track].pregap;
			if (cue_data->tracks[num_track].type == CT_MP3) {
				ret = audio_track_mp3(cue_data->tracks[num_track].fname, index);
				if (ret != 0) break;
			}
			else
			{
				Tracks[index].ftype = cue_data->tracks[num_track].type;
				if (cue_data->tracks[num_track].fname != NULL)
				{
					pm_file *pmfn = pm_open(cue_data->tracks[num_track].fname);
					if (pmfn != NULL)
					{
						// addume raw, ignore header for wav..
						Tracks[index].F = pmfn;
						Tracks[index].Length = pmfn->size / 2352;
						Tracks[index].Offset = cue_data->tracks[num_track].sector_offset;
					}
					else
					{
						elprintf(EL_STATUS, "track %2i (%s): can't determine length",
							num_track, cue_data->tracks[num_track].fname);
						Tracks[index].Length = 2*75;
						Tracks[index].Offset = 0;
					}
				}
				else
				{
					if (num_track < cue_data->track_count)
						Tracks[index].Length = cue_data->tracks[num_track+1].sector_offset -
							cue_data->tracks[num_track].sector_offset;
					else
						Tracks[index].Length = cd_img_sectors - cue_data->tracks[num_track].sector_offset;
					Tracks[index].Offset = cue_data->tracks[num_track].sector_offset;
				}
			}

			if (cue_data->tracks[num_track].sector_xlength != 0)
				// overriden by custom cue command
				Tracks[index].Length = cue_data->tracks[num_track].sector_xlength;

			LBA_to_MSF(Cur_LBA, &Tracks[index].MSF);
			Cur_LBA += Tracks[index].Length;

			elprintf(EL_STATUS, "Track %2i: %02d:%02d:%02d %9i AUDIO - %s", num_track, Tracks[index].MSF.M,
				Tracks[index].MSF.S, Tracks[index].MSF.F, Tracks[index].Length,
				cue_data->tracks[num_track].fname);
		}
		cue_destroy(cue_data);
		goto finish;
	}

	/* mp3 track autosearch, Gens-like */
	iso_name_len = strlen(cd_img_name);
	if (iso_name_len >= sizeof(tmp_name))
		iso_name_len = sizeof(tmp_name) - 1;

	for (num_track = 2, i = 0, missed = 0; i < 100 && missed < 4; i++)
	{
		if (PicoCDLoadProgressCB != NULL && i > 1)
			PicoCDLoadProgressCB(cd_img_name, i + (100-i)*missed/4);

		for (j = 0; j < sizeof(exts)/sizeof(char *); j++)
		{
			int ext_len;
			sprintf(tmp_ext, exts[j], i);
			ext_len = strlen(tmp_ext);

			memcpy(tmp_name, cd_img_name, iso_name_len + 1);
			tmp_name[iso_name_len - 4] = 0;
			strcat(tmp_name, tmp_ext);

			index = num_track - 1;
			ret = audio_track_mp3(tmp_name, index);
			if (ret != 0 && i > 1 && iso_name_len > ext_len) {
				tmp_name[iso_name_len - ext_len] = 0;
				strcat(tmp_name, tmp_ext);
				ret = audio_track_mp3(tmp_name, index);
			}

			if (ret == 0)
			{
				LBA_to_MSF(Cur_LBA, &Tracks[index].MSF);
				Cur_LBA += Tracks[index].Length;

				elprintf(EL_STATUS, "Track %2i: %02d:%02d:%02d %9i AUDIO - %s", num_track, Tracks[index].MSF.M,
					Tracks[index].MSF.S, Tracks[index].MSF.F, Tracks[index].Length, tmp_name);

				num_track++;
				missed = 0;
				break;
			}
		}
		if (ret != 0 && i > 1) missed++;
	}

finish:
	Pico_mcd->TOC.Last_Track = num_track - 1;

	index = num_track - 1;

	LBA_to_MSF(Cur_LBA, &Tracks[index].MSF);

	elprintf(EL_STATUS, "End CD -  %02d:%02d:%02d\n", Tracks[index].MSF.M,
		Tracks[index].MSF.S, Tracks[index].MSF.F);

	if (PicoCDLoadProgressCB != NULL)
		PicoCDLoadProgressCB(cd_img_name, 100);

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
		{
			if (Pico_mcd->TOC.Tracks[i].ftype == TYPE_MP3)
#if DONT_OPEN_MANY_FILES
				free(Pico_mcd->TOC.Tracks[i].F);
#else
				fclose(Pico_mcd->TOC.Tracks[i].F);
#endif
			else
				pm_close(Pico_mcd->TOC.Tracks[i].F);
		}
	}
	memset(Pico_mcd->TOC.Tracks, 0, sizeof(Pico_mcd->TOC.Tracks));
}


PICO_INTERNAL int FILE_Read_One_LBA_CDC(void)
{
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

				// this is pretty rough, but oh well - not much depends on this anyway
				memcpy(&Pico_mcd->cdc.Buffer[Pico_mcd->cdc.PT.N], cdda_out_buffer, 2352);
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

