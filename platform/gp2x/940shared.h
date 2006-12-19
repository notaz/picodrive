#include "../../Pico/sound/ym2612.h"

enum _940_job_t {
	JOB940_YM2612INIT = 1,
	JOB940_YM2612RESETCHIP,
	JOB940_YM2612UPDATEONE,
	JOB940_PICOSTATELOAD,
	JOB940_NUMJOBS
};


typedef struct
{
	YM2612	ym2612;					/* current state of the emulated YM2612 */
	int		mix_buffer[44100/50*2];	/* this is where the YM2612 samples will be mixed to */
} _940_data_t;


typedef struct
{
	int		job;					/* a job for second core */
	int		busy;					/* busy status of the 940 core */
	int		length;					/* number of samples to mix (882 max) */
	int		stereo;					/* mix samples as stereo, doubles sample count automatically */
	int		baseclock;				/* ym2612 settings */
	int		rate;
	int		writebuffsel;			/* which write buffer to use (from 940 side) */
	UINT16  writebuff0[2048];		/* 1024 for savestates, 1024 extra */
	UINT16  writebuff1[2048];
	int		vstarts[8];				/* debug: number of starts from each of 8 vectors */
	int		loopc;					/* debug: main loop counter */
	int		waitc;					/* debug: wait loop counter */
} _940_ctl_t;
