# settings
use_musashi = 1
#use_fame = 1
use_cz80 = 1
use_sh2drc = 1
#use_sh2mame = 1

#drc_debug = 3
#drc_debug_interp = 1
#profile = 1
#fake_in_gp2x = 1

-include Makefile.local

ifndef ARCH
ARCH = x86
endif

ifeq "$(profile)" "1"
CFLAGS += -O3 -Wall
CFLAGS += -ftracer -fstrength-reduce -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math
CFLAGS += -fprofile-generate
else
CFLAGS += -ggdb -Wall -falign-functions=2
endif
DEFINES = _UNZIP_SUPPORT IO_STATS IN_EVDEV
CFLAGS += -I../.. -I.
LDFLAGS += -lm -lpng
ifeq "$(ARCH)" "arm"
CFLAGS += -mcpu=arm920t
DEFINES += ARM
endif
LDFLAGS += -lpthread
LDFLAGS += -lX11

CC = $(CROSS)gcc

# frontend
OBJS += platform/gp2x/emu.o blit.o in_evdev.o plat.o sndout_oss.o gp2x.o log_io.o

# common
OBJS += platform/common/main.o platform/common/emu.o platform/common/menu.o \
	platform/common/config.o platform/common/fonts.o platform/common/readpng.o \
	platform/common/input.o

ifeq "$(fake_in_gp2x)" "1"
DEFINES += IN_GP2X FAKE_IN_GP2X
OBJS += platform/gp2x/in_gp2x.o
endif

# Pico
OBJS += pico/area.o pico/cart.o pico/memory.o pico/pico.o pico/sek.o \
	pico/videoport.o pico/draw2.o pico/draw.o pico/z80if.o pico/patch.o \
	pico/mode4.o pico/sms.o pico/misc.o pico/eeprom.o pico/debug.o
# Pico - CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
	pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
	pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
# Pico - 32X
OBJS += pico/32x/32x.o pico/32x/memory.o pico/32x/draw.o pico/32x/pwm.o
# Pico - Pico
OBJS += pico/pico/pico.o pico/pico/memory.o pico/pico/xpcm.o
# Pico - sound
OBJS += pico/sound/sound.o pico/sound/sn76496.o pico/sound/ym2612.o pico/sound/mix.o
# Pico - carthw
OBJS += pico/carthw/carthw.o pico/carthw/svp/svp.o pico/carthw/svp/memory.o \
	pico/carthw/svp/ssp16.o pico/carthw/svp/compiler.o
ifeq "$(ARCH)" "arm"
OBJS += pico/carthw/svp/stub_arm.o
endif
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o zlib/uncompr.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o

CFLAGS += $(addprefix -D,$(DEFINES))

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
vpath %.asm = ../..

DIRS = platform/linux platform/gp2x platform/common pico pico/cd pico/pico pico/sound pico/carthw/svp \
	pico/32x zlib unzip cpu cpu/musashi cpu/fame cpu/mz80 cpu/cz80 cpu/sh2/mame cpu/drc

all: mkdirs PicoDrive

include ../common/common.mak

clean: tidy
	@$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)
	rm -rf $(DIRS)
	@make -C ../../cpu/mz80/ clean

PicoDrive : $(OBJS)
	@echo ">>>" $@
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=PicoDrive.map -o $@

mkdirs:
	@mkdir -p $(DIRS)

include ../common/revision.mak

.c.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@
.s.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@
%.o : %.asm
	@echo ">>>" $<
	nasm -f elf $< -o $@

