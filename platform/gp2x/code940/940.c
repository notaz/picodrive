#include "940shared.h"

static _940_data_t *shared_data = (_940_data_t *)   0x00100000;
static _940_ctl_t  *shared_ctl  = (_940_ctl_t *)    0x00200000;
static unsigned char *mp3_data  = (unsigned char *) 0x01000000;
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
	// asm volatile ("mcr p15, 0, r0, c7, c10, 4" ::: "r0");


	for (;;)
	{
		int job_num = 0;
/*
		while (!shared_ctl->busy)
		{
			//shared_ctl->waitc++;
			spend_cycles(8*1024);
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
				case JOB940_INITALL:
					/* ym2612 */
					shared_ctl->writebuff0[0] = shared_ctl->writebuff1[0] = 0xffff;
					YM2612Init_(shared_ctl->baseclock, shared_ctl->rate);
					/* Helix mp3 decoder */
					shared_data->mp3dec = MP3InitDecoder();
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

				case JOB940_MP3DECODE: {
					int mp3_offs = shared_ctl->mp3_offs;
					unsigned char *readPtr = mp3_data + mp3_offs;
					int bytesLeft = shared_ctl->mp3_len - mp3_offs;
					int offset; // frame offset from readPtr
					int err;

					if (bytesLeft <= 0) break; // EOF, nothing to do

					offset = MP3FindSyncWord(readPtr, bytesLeft);
					if (offset < 0) {
						shared_ctl->mp3_offs = shared_ctl->mp3_len;
						break; // EOF
					}
					readPtr += offset;
					bytesLeft -= offset;

 					err = MP3Decode(shared_data->mp3dec, &readPtr, &bytesLeft,
						shared_data->mp3_buffer[shared_ctl->mp3_buffsel], 0);
					if (err) {
						if (err == ERR_MP3_INDATA_UNDERFLOW) {
							shared_ctl->mp3_offs = shared_ctl->mp3_len; // EOF
							break;
						} else if (err <= -6 && err >= -12) {
							// ERR_MP3_INVALID_FRAMEHEADER, ERR_MP3_INVALID_*
							// just try to skip the offending frame..
							readPtr++;
						}
						shared_ctl->mp3_errors++;
						shared_ctl->mp3_lasterr = err;
					}
					shared_ctl->mp3_offs = readPtr - mp3_data;
					break;
				}
			}
		}

		cache_clean();
//		asm volatile ("mov r0, #0" ::: "r0");
//		asm volatile ("mcr p15, 0, r0, c7, c10, 4" ::: "r0"); /* drain write buffer, should be done on nonbuffered write */
//		cache_clean_flush();

		shared_ctl->loopc++;
		shared_ctl->busy = 0;
	}
}

