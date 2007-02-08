
#define PCM_STEP_SHIFT 11

void pcm_write(unsigned int a, unsigned int d);
void pcm_set_rate(int rate);
void pcm_update(int *buffer, int length, int stereo);

