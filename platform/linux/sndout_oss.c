/* sound output via OSS */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

#include "sndout_oss.h"

static int sounddev = -1, mixerdev = -1;

int sndout_oss_init(void)
{
	if (mixerdev >= 0) close(mixerdev);
  	mixerdev = open("/dev/mixer", O_RDWR);
	if (mixerdev == -1)
	{
		perror("open(\"/dev/mixer\")");
	}

	return 0;
}


int sndout_oss_start(int rate, int bits, int stereo)
{
	static int s_oldrate = 0, s_oldbits = 0, s_oldstereo = 0;
	int frag = 0, bsize, buffers, ret;

	// if no settings change, we don't need to do anything,
	// since audio is never stopped
	if (rate == s_oldrate && s_oldbits == bits && s_oldstereo == stereo)
		return 0;

	if (sounddev >= 0) close(sounddev);
	sounddev = open("/dev/dsp", O_WRONLY|O_ASYNC);
	if (sounddev == -1)
	{
		perror("open(\"/dev/dsp\")");
		return -1;
	}

	// calculate buffer size
	// this is tuned for GP2X
	buffers = 16;
	bsize = rate / 32;
	if (rate > 22050) { bsize*=4; buffers*=2; }
	while ((bsize>>=1)) frag++;
	frag |= buffers<<16; // 16 buffers
	ret = ioctl(sounddev, SNDCTL_DSP_SETFRAGMENT, &frag);
	if (ret) perror("SNDCTL_DSP_SETFRAGMENT failed");

	ret  = ioctl(sounddev, SNDCTL_DSP_STEREO, &stereo);
	ret |= ioctl(sounddev, SNDCTL_DSP_SETFMT, &bits);
	ret |= ioctl(sounddev, SNDCTL_DSP_SPEED,  &rate);
	if (ret) printf("failed to set audio format\n");
	usleep(192*1024);

	printf("gp2x_set_sound: %i/%ibit/%s, %i buffers of %i bytes\n",
		rate, bits, stereo?"stereo":"mono", frag>>16, 1<<(frag&0xffff));

	s_oldrate = rate; s_oldbits = bits; s_oldstereo = stereo;
	return 0;
}


int sndout_oss_write(const void *buff, int len)
{
	return write(sounddev, buff, len);
}


void sndout_oss_sync(void)
{
	ioctl(sounddev, SOUND_PCM_SYNC, 0);
}


void sndout_oss_setvol(int l, int r)
{
	if (mixerdev < 0) return;

 	l=l<0?0:l; l=l>255?255:l; r=r<0?0:r; r=r>255?255:r;
 	l<<=8; l|=r;
 	ioctl(mixerdev, SOUND_MIXER_WRITE_PCM, &l); /*SOUND_MIXER_WRITE_VOLUME*/
}


void sndout_oss_exit(void)
{
	if (sounddev >= 0) close(sounddev); sounddev = -1;
	if (mixerdev >= 0) close(mixerdev); mixerdev = -1;
}

