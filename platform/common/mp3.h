
int mp3_find_sync_word(const unsigned char *buf, int size);

#ifdef __GP2X__
void mp3_update_local(int *buffer, int length, int stereo);
void mp3_start_play_local(void *f, int pos);
#endif

