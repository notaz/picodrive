#export CROSS = arm-none-linux-gnueabi-
LIBROOT = /home/notaz/dev/pnd/libroot

# settings
#mz80 = 1
#cz80 = 1
#debug_cyclone = 1
asm_memory = 1
asm_render = 1
asm_ym2612 = 1
asm_misc = 1
asm_cdpico = 1
asm_cdmemory = 1
amalgamate = 0
#profile = 1
#use_musashi = 1
#use_famec = 1


ifeq "$(debug_cyclone)" "1"
use_cyclone = 1
use_musashi = 1
endif
ifeq "$(use_musashi)" "1"
asm_cdpico = 0
asm_memory = 0
asm_cdmemory = 0
else
use_cyclone = 1
endif

DEFINC = -I../.. -I. -DARM -DPANDORA
COPT_COMMON = -Wall -Winline -mcpu=cortex-a8 -mtune=cortex-a8 -march=armv7-a -I$(LIBROOT)/include
ifeq ($(DEBUG),)
COPT_COMMON += -O2 # -ftracer -fstrength-reduce -fomit-frame-pointer -fstrict-aliasing -ffast-math
else
COPT_COMMON += -ggdb
endif
ifeq "$(profile)" "1"
COPT_COMMON += -fprofile-generate
endif
ifeq "$(profile)" "2"
COPT_COMMON += -fprofile-use
endif
CFLAGS = $(COPT_COMMON)
SFLAGS = $(CFLAGS)
ASFLAGS = -mcpu=cortex-a8
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# frontend
OBJS += pandora.o main.o emu.o asm_utils.o platform/gp2x/menu.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/fonts.o platform/common/config.o \
	platform/common/arm_utils.o platform/common/mp3_helix.o platform/common/arm_linux.o \
	platform/common/readpng.o platform/linux/usbjoy.o platform/linux/sndout_oss.o

# Pico
ifeq "$(amalgamate)" "1"
OBJS += ../../picoAll.o
else
OBJS += pico/area.o pico/cart.o pico/memory.o pico/misc.o pico/pico.o pico/sek.o \
		pico/videoport.o pico/draw2.o pico/draw.o pico/patch.o pico/debug.o
# Pico - CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
		pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
		pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
endif
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
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o
# debug
ifeq "$(debug_cyclone)" "1"
OBJS += pico/DebugCPU.o cpu/musashi/m68kdasm.o
endif
# CPU cores
ifeq "$(use_musashi)" "1"
DEFINC += -DEMU_M68K
OBJS += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
endif
ifeq "$(use_cyclone)" "1"
DEFINC += -DEMU_C68K
OBJS += cpu/Cyclone/proj/Cyclone.o cpu/Cyclone/tools/idle.o
endif
ifeq "$(use_famec)" "1"
DEFINC += -DEMU_F68K
OBJS += cpu/fame/famec.o
endif
# drz80/mz80
ifeq "$(mz80)" "1"
DEFINC += -D_USE_MZ80
OBJS += cpu/mz80/mz80.o
else
ifeq "$(cz80)" "1"
DEFINC += -D_USE_CZ80
OBJS += cpu/cz80/cz80.o
else
DEFINC += -D_USE_DRZ80
OBJS += cpu/DrZ80/drz80.o
endif
endif

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
DIRS = platform platform/gp2x platform/linux platform/common pico pico/cd pico/pico pico/sound pico/carthw/svp \
	zlib unzip cpu cpu/musashi cpu/Cyclone/proj cpu/Cyclone/tools cpu/mz80 cpu/DrZ80


all: mkdirs PicoDrive

include ../common/common_arm.mak

PicoDrive : $(OBJS) ../common/helix/$(CROSS)helix-mp3.a
	@echo ">>>" $@
	$(CC) -o $@ $(CFLAGS) $^ -lm -Wl,-Map=PicoDrive.map -lpng -L$(LIBROOT)/lib
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

