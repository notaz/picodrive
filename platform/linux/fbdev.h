int  vout_fbdev_init(int *w, int *h);
void vout_fbdev_finish(void);

extern void *fbdev_buffers[];
extern int fbdev_buffer_count;
