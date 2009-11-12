CROSS ?= arm-linux-

# settings
use_cyclone = 1
#use_musashi = 1
use_drz80 = 1
use_sh2drc = 1
#use_sh2mame = 1

asm_memory = 1
asm_render = 1
asm_ym2612 = 1
asm_misc = 1
asm_cdpico = 1
asm_cdmemory = 1
amalgamate = 0
#profile = 1
#drc_debug = 3

-include Makefile.local

ifeq "$(debug_cyclone)" "1"
use_cyclone = 1
use_musashi = 1
endif
ifeq "$(use_musashi)" "1"
# due to CPU stop flag acces
asm_cdpico = 0
asm_cdmemory = 0
endif

ARCH = arm
DEFINES += ARM __GP2X__ IN_GP2X IN_EVDEV # BENCHMARK
CFLAGS += -Wall -Winline -I../.. -I.
ifeq ($(DEBUG),)
CFLAGS += -O3 -fomit-frame-pointer -fstrict-aliasing -ffast-math
else
CFLAGS += -ggdb
endif
ifeq "$(profile)" "1"
CFLAGS += -fprofile-generate
endif
ifeq "$(profile)" "2"
CFLAGS += -fprofile-use
endif
CFLAGS += -mcpu=arm920t -mtune=arm920t
ASFLAGS = -mcpu=arm920t -mfloat-abi=soft
LDFLAGS += -lm -lpng

CC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# frontend
OBJS += plat.o warm.o pollux_set.o soc.o soc_mmsp2.o soc_pollux.o soc_dummy.o emu.o in_gp2x.o
# 940 core control
OBJS += 940ctl.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/fonts.o platform/common/config.o \
	platform/common/arm_utils.o platform/common/arm_linux.o platform/common/readpng.o \
	platform/common/mp3_helix.o platform/common/input.o platform/common/main.o \
	platform/linux/sndout_oss.o platform/linux/plat.o platform/linux/in_evdev.o

# Pico
ifeq "$(amalgamate)" "1"
OBJS += ../../picoAll.o
else
OBJS += pico/area.o pico/cart.o pico/memory.o pico/pico.o pico/sek.o pico/z80if.o \
	pico/videoport.o pico/draw2.o pico/draw.o pico/mode4.o pico/sms.o \
	pico/misc.o pico/eeprom.o pico/patch.o pico/debug.o
# Pico - CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
		pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
		pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
endif
# Pico - 32X
OBJS += pico/32x/32x.o pico/32x/memory.o pico/32x/draw.o pico/32x/pwm.o
# Pico - Pico
OBJS += pico/pico/pico.o pico/pico/memory.o pico/pico/xpcm.o
# Pico - carthw
OBJS += pico/carthw/carthw.o pico/carthw/svp/svp.o pico/carthw/svp/memory.o \
		pico/carthw/svp/ssp16.o pico/carthw/svp/compiler.o pico/carthw/svp/stub_arm.o

# Pico - sound
ifneq "$(amalgamate)" "1"
OBJS += pico/sound/sound.o
endif
OBJS += pico/sound/mix_arm.o
OBJS += pico/sound/sn76496.o pico/sound/ym2612.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o

CFLAGS += $(addprefix -D,$(DEFINES))

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..

DIRS = platform platform/gp2x platform/linux platform/common pico pico/cd pico/pico pico/32x \
	pico/sound pico/carthw/svp zlib unzip cpu cpu/musashi cpu/Cyclone/proj cpu/Cyclone/tools \
	cpu/mz80 cpu/DrZ80 cpu/sh2/mame cpu/drc


all: mkdirs PicoDrive

include ../common/common.mak
include ../common/common_arm.mak
include ../common/revision.mak

# partial linking helps profiled builds due to section merging
PicoDrive.o : $(OBJS) ../common/helix/$(CROSS)helix-mp3.a
	$(LD) -r -o $@ $^

# still using static, dynamic linking slows Wiz 1-10%
# also libm on F100 is not compatible
PicoDrive : PicoDrive.o
	@echo ">>>" $@
	$(CC) -static -o $@ $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=$@.map
ifeq ($(DEBUG),)
	$(STRIP) $@
endif

up: PicoDrive
	@cp -v PicoDrive /mnt/gp2x/mnt/sd/emus/PicoDrive/

clean: tidy
	$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)

readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ GP2X

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif
CODE940 = code940/pico940_v3.bin

rel: PicoDrive PicoDrive.gpe $(CODE940) readme.txt PicoDrive.png ../game_def.cfg \
		warm_2.4.25.o warm_2.4.26-open2x.o warm_2.6.24.ko
	zip -9 -j ../../PicoDrive_$(VER).zip $^
	zip -9 -r ../../PicoDrive_$(VER).zip skin -i \*.png -i \*.txt
	mkdir bin_to_cso_mp3
	cp ../../tools/bin_to_cso_mp3/* bin_to_cso_mp3/
	zip -9 -r ../../PicoDrive_$(VER).zip bin_to_cso_mp3
	rm -rf bin_to_cso_mp3

$(CODE940):
	make -C code940/

