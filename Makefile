# settings
#use_fbdev = 1
#fake_in_gp2x = 1

use_musashi = 1
#use_fame = 1
use_cz80 = 1
#use_sh2drc = 1
use_sh2mame = 1

#drc_debug = 3
#drc_debug_interp = 1
#profile = 1

-include Makefile.local

ifndef ARCH
ARCH = x86
endif

CC ?= $(CROSS_COMPILE)gcc

DEFINES = _UNZIP_SUPPORT IO_STATS IN_EVDEV
CFLAGS += -ggdb -Wall -falign-functions=2
CFLAGS += -I.
CFLAGS += -Iplatform/linux/
LDLIBS += -lm -lpng

all: PicoDrive

# frontend
OBJS += platform/linux/io.o platform/linux/emu.o platform/linux/blit.o \
	platform/linux/log_io.o

# common
OBJS += platform/common/main.o platform/common/emu.o platform/common/menu_pico.o \
	platform/common/config.o

# libpicofe
OBJS += platform/libpicofe/input.o platform/libpicofe/readpng.o \
	platform/libpicofe/fonts.o platform/libpicofe/linux/in_evdev.o \
	platform/libpicofe/linux/plat.o platform/libpicofe/linux/sndout_oss.o

OBJS += platform/libpicofe/plat_dummy.o

ifeq "$(use_fbdev)" "1"
DEFINES += FBDEV
OBJS += fbdev.o
else
LDLIBS += -lpthread
LDLIBS += -lX11
endif

ifeq "$(ARCH)" "arm"
OBJS += pico/carthw/svp/stub_arm.o
endif
OBJS += pico/sound/mix.o
OBJS += pico/carthw/svp/compiler.o

# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o zlib/uncompr.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o

include platform/common/common.mak

CFLAGS += $(addprefix -D,$(DEFINES))

clean:
	$(RM) PicoDrive $(OBJS)

PicoDrive : $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -Wl,-Map=PicoDrive.map -o $@

pprof: platform/linux/pprof.c
	$(CC) -O2 -ggdb -DPPROF -DPPROF_TOOL -I../../ -I. $^ -o $@
