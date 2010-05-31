#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/kd.h>

int main()
{
	struct fb_var_screeninfo fbvar;
	int ret, fbdev, kbdfd;

	fbdev = open("/dev/fb0", O_RDWR);
	if (fbdev == -1) {
		perror("open");
		return 1;
	}

	ret = ioctl(fbdev, FBIOGET_VSCREENINFO, &fbvar);
	if (ret == -1) {
		perror("FBIOGET_VSCREENINFO ioctl");
		goto end_fb;
	}

	if (fbvar.yoffset != 0) {
		printf("fixing yoffset.. ");
		fbvar.yoffset = 0;
		ret = ioctl(fbdev, FBIOPAN_DISPLAY, &fbvar);
		if (ret < 0)
			perror("ioctl FBIOPAN_DISPLAY");
		else
			printf("ok\n");
	}

end_fb:
	close(fbdev);

	kbdfd = open("/dev/tty", O_RDWR);
	if (kbdfd == -1) {
		perror("open /dev/tty");
		return 1;
	}

	if (ioctl(kbdfd, KDSETMODE, KD_TEXT) == -1)
		perror("KDSETMODE KD_TEXT");

	close(kbdfd);

	return 0;
}
