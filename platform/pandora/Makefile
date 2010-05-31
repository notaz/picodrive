#export CROSS = arm-none-linux-gnueabi-
CROSS ?= $(CROSS_COMPILE)
LIBROOT = /home/notaz/dev/pnd/libroot

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
#profile = 1
#drc_debug = 3

-include Makefile.local

ifeq "$(use_musashi)" "1"
# due to CPU stop flag acces
asm_cdpico = 0
asm_cdmemory = 0
endif

ARCH = arm
DEFINES += ARM PANDORA IN_EVDEV
CFLAGS += -I../.. -I. -I$(LIBROOT)/include
CFLAGS += -Wall -Winline -mcpu=cortex-a8 -mtune=cortex-a8 -march=armv7-a
ifeq ($(DEBUG),)
CFLAGS += -O2 -fomit-frame-pointer -fstrict-aliasing -ffast-math
endif
LDFLAGS += -L$(LIBROOT)/lib -Wl,-Bstatic -lpng -Wl,-Bdynamic -lm -lpthread -ldl
ASFLAGS = -mcpu=cortex-a8
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# frontend
OBJS += pandora.o emu.o asm_utils.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/fonts.o platform/common/config.o \
	platform/common/arm_utils.o platform/common/mp3_helix.o platform/common/arm_linux.o \
	platform/common/readpng.o platform/common/input.o platform/common/main.o \
	platform/linux/fbdev.o platform/linux/in_evdev.o platform/linux/sndout_oss.o \
	platform/linux/plat.o platform/linux/oshide.o

# ARM stuff
OBJS += pico/carthw/svp/compiler.o pico/carthw/svp/stub_arm.o
OBJS += pico/sound/mix_arm.o

# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
DIRS += platform/linux zlib unzip

all: mkdirs PicoDrive picorestore

include ../common/common.mak
include ../common/common_arm.mak
include ../common/revision.mak

CFLAGS += $(addprefix -D,$(DEFINES))

PicoDrive : $(OBJS) ../common/helix/$(CROSS)helix-mp3.a
	@echo ">>>" $@
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -Wl,-Map=PicoDrive.map
ifeq ($(DEBUG),)
	$(STRIP) $@
endif

clean: tidy
	$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)

readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ GP2X

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
$(error TODO)
ifeq ($(VER),)
$(error need VER)
endif
endif

