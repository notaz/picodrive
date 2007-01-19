#include "940shared.h"

static _940_data_t *shared_data = (_940_data_t *) 0x100000;
static _940_ctl_t  *shared_ctl  = (_940_ctl_t *)  0x200000;
YM2612 *ym2612_940;
int *mix_buffer;

// from init.s
void wait_irq(void);
void spend_cycles(int c);
void cache_clean(void);
void cache_clean_flush(void);

//	asm volatile ("mov r0, #0" ::: "r0");
//	asm volatile ("mcr p15, 0, r0, c7, c6,  0" ::: "r0"); /* flush dcache */
//	asm volatile ("mcr p15, 0, r0, c7, c10, 4" ::: "r0"); /* drain write buffer */

void Main940(int startvector)
{
	ym2612_940 = &shared_data->ym2612;
	mix_buffer = shared_data->mix_buffer;

	// debug
	shared_ctl->vstarts[startvector]++;
	asm volatile ("mcr p15, 0, r0, c7, c10, 4" ::: "r0");


	for (;; shared_ctl->loopc++)
	{
		int job_num;
/*
		while (!shared_ctl->busy)
		{
			//shared_ctl->waitc++;
			spend_cycles(256);
		}
*/
		if (!shared_ctl->busy)
		{
			wait_irq();
		}

		for (job_num = 0; job_num < MAX_940JOBS; job_num++)
		{
			switch (shared_ctl->jobs[job_num])
			{
				case JOB940_YM2612INIT:
					shared_ctl->writebuff0[0] = shared_ctl->writebuff1[0] = 0xffff;
					YM2612Init_(shared_ctl->baseclock, shared_ctl->rate);
					break;

				case JOB940_YM2612RESETCHIP:
					YM2612ResetChip_();
					break;

				case JOB940_PICOSTATELOAD:
					YM2612PicoStateLoad_();
					break;

				case JOB940_YM2612UPDATEONE: {
					int i, dw, *wbuff;
					if (shared_ctl->writebuffsel == 1) {
						wbuff = (int *) shared_ctl->writebuff1;
					} else {
						wbuff = (int *) shared_ctl->writebuff0;
					}

					/* playback all writes */
					for (i = 2048/2; i > 0; i--) {
						UINT16 d;
						dw = *wbuff++;
						d = dw;
						if (d == 0xffff) break;
						YM2612Write_(d >> 8, d);
						d = (dw>>16);
						if (d == 0xffff) break;
						YM2612Write_(d >> 8, d);
					}

					YM2612UpdateOne_(0, shared_ctl->length, shared_ctl->stereo);
					break;
				}
			}
		}

		shared_ctl->busy = 0;
//		cache_clean_flush();
		cache_clean();
//		asm volatile ("mov r0, #0" ::: "r0");
//		asm volatile ("mcr p15, 0, r0, c7, c10, 4" ::: "r0"); /* drain write buffer, should be done on nonbuffered write */
	}
}

