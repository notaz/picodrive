
#define PCM_STEP_SHIFT 11

PICO_INTERNAL_ASM void pcm_write(unsigned int a, unsigned int d);
PICO_INTERNAL void pcm_set_rate(int rate);
PICO_INTERNAL void pcm_update(int *buffer, int length, int stereo);

