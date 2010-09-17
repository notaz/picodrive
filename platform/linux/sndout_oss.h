int  sndout_oss_init(void);
int  sndout_oss_start(int rate, int stereo, int frames_in_frag);
void sndout_oss_stop(void);
int  sndout_oss_write(const void *buff, int len);
int  sndout_oss_can_write(int bytes);
void sndout_oss_sync(void);
void sndout_oss_setvol(int l, int r);
void sndout_oss_exit(void);
