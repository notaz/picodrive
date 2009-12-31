# settings
#use_fbdev = 1
#fake_in_gp2x = 1

use_musashi = 1
#use_fame = 1
use_cz80 = 1
use_sh2drc = 1
#use_sh2mame = 1

#drc_debug = 3
#drc_debug_interp = 1
#profile = 1

-include Makefile.local

ifndef ARCH
ARCH = x86
endif

DEFINES = _UNZIP_SUPPORT IO_STATS IN_EVDEV
CFLAGS += -ggdb -Wall -falign-functions=2
CFLAGS += -I../.. -I.
LDFLAGS += -lm -lpng
ifeq "$(ARCH)" "arm"
CFLAGS += -mcpu=arm920t
DEFINES += ARM
endif

CC = $(CROSS)gcc

# frontend
OBJS += io.o emu.o blit.o in_evdev.o plat.o sndout_oss.o log_io.o

# common
OBJS += platform/common/main.o platform/common/emu.o platform/common/menu.o \
	platform/common/config.o platform/common/fonts.o platform/common/readpng.o \
	platform/common/input.o

ifeq "$(use_fbdev)" "1"
DEFINES += FBDEV
OBJS += fbdev.o
else
LDFLAGS += -lpthread
LDFLAGS += -lX11
endif

ifeq "$(fake_in_gp2x)" "1"
DEFINES += IN_GP2X FAKE_IN_GP2X
OBJS += platform/gp2x/in_gp2x.o
DIRS += platform/gp2x
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

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
vpath %.asm = ../..

DIRS += platform/linux zlib unzip

all: mkdirs PicoDrive

include ../common/common.mak
include ../common/revision.mak

CFLAGS += $(addprefix -D,$(DEFINES))

clean: tidy
	@$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)
	rm -rf $(DIRS)
	@make -C ../../cpu/mz80/ clean

PicoDrive : $(OBJS)
	@echo ">>>" $@
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=PicoDrive.map -o $@

pprof: pprof.c
	$(CROSS)gcc -O2 -ggdb -DPPROF -DPPROF_TOOL -I../../ -I. $^ -o $@ -lrt

%.o : %.asm
	@echo ">>>" $<
	nasm -f elf $< -o $@

