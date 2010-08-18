struct vout_fbdev;

struct vout_fbdev *vout_fbdev_init(const char *fbdev_name, int *w, int *h, int no_dblbuf);
void *vout_fbdev_flip(struct vout_fbdev *fbdev);
void  vout_fbdev_wait_vsync(struct vout_fbdev *fbdev);
void  vout_fbdev_clear(struct vout_fbdev *fbdev);
void  vout_fbdev_finish(struct vout_fbdev *fbdev);
