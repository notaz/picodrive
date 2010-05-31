#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <linux/kd.h>

int main()
{
	struct fb_var_screeninfo fbvar;
	struct termios kbd_termios;
	int ret, fbdev, kbdfd;
	FILE *tios_f;

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

	tios_f = fopen("/tmp/pico_tios", "rb");
	if (tios_f != NULL) {
		kbdfd = open("/dev/tty", O_RDWR);
		if (kbdfd == -1) {
			perror("open /dev/tty");
			return 1;
		}

		if (fread(&kbd_termios, sizeof(kbd_termios), 1, tios_f) == 1) {
			if (ioctl(kbdfd, KDSETMODE, KD_TEXT) == -1)
				perror("KDSETMODE KD_TEXT");

			printf("restoring termios.. ");
			if (tcsetattr(kbdfd, TCSAFLUSH, &kbd_termios) == -1)
				perror("tcsetattr");
			else
				printf("ok\n");
		}

		close(kbdfd);
		fclose(tios_f);
	}

	return 0;
}
