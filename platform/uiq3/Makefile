# makefile for uiq3_patcher_0_2.tar.gz setup, modified
export CROSS = arm-none-symbianelf-
APPNAME = PicoDrive
VER_MAJ = 1
VER_MIN = 51
VENDOR = notaz
UID3 = A00010F3
EPOCROOT = /opt/uiq3/
EPOCLIBS = qikdlg.lib etext.lib bafl.lib efsrv.lib eikctl.lib ws32.lib \
	eikdlg.lib gdi.lib estor.lib hal.lib mediaclient.lib mediaclientaudiostream.lib
STACK = 0x3000
HEAP = 0x10,0x1000000


# settings
asm_memory = 1
asm_render = 1
asm_ym2612 = 1
asm_misc = 1
asm_cdpico = 1
asm_cdmemory = 1
asm_blit = 1
use_cyclone = 1
#use_musashi = 1


# objects

# launcher
OBJS += App.o Engine.o Dialogs.o CSimpleTextParser.o emu.o
# engine
OBJS += engine/main.o engine/vid.o engine/polledas.o engine/audio_mediaserver.o engine/debug.o
ifeq "$(asm_blit)" "1"
OBJS += engine/blit_asm.o
else
OBJS += engine/blit.o
endif
# common
OBJS += ../common/emu.o ../common/config.o ../common/menu.o ../common/mp3_helix.o
# Pico
OBJS += pico/area.o pico/cart.o pico/memory.o pico/misc.o pico/pico.o pico/sek.o \
		pico/videoport.o pico/draw2.o pico/draw.o pico/patch.o pico/debug.o
# Pico - CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
		pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
		pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
# Pico - Pico
OBJS += pico/pico/pico.o pico/pico/memory.o pico/pico/xpcm.o
# Pico - carthw
OBJS += pico/carthw/carthw.o pico/carthw/svp/svp.o pico/carthw/svp/memory.o \
		pico/carthw/svp/ssp16.o pico/carthw/svp/compiler.o pico/carthw/svp/stub_arm.o

# Pico - sound
OBJS += pico/sound/sound.o
OBJS += pico/sound/mix_arm.o
OBJS += pico/sound/sn76496.o pico/sound/ym2612.o
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o
# CPU cores
ifeq "$(use_musashi)" "1"
CFLAGS += -DEMU_M68K
OBJS += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
endif
ifeq "$(use_cyclone)" "1"
CFLAGS += -DEMU_C68K
OBJS += cpu/Cyclone/proj/Cyclone.o cpu/Cyclone/tools/idle.o
endif
# drz80
CFLAGS += -D_USE_DRZ80
OBJS += cpu/DrZ80/drz80.o
# helix
OBJS += ../common/helix/$(CROSS)helix-mp3.a


vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
DIRS = platform platform/common pico pico/cd pico/pico pico/sound pico/carthw/svp \
	zlib unzip cpu cpu/musashi cpu/Cyclone/proj cpu/Cyclone/tools cpu/mz80 cpu/DrZ80

ICONS := $(shell echo rsc/*.bmp)
APPICON = $(NAME).mbm
RSCDIR = rsc
REGDIR = rsc
CFLAGS += -I./ -I../../
CFLAGS += -DUIQ3 -DARM -DNO_SYNC
CFLAGS += -D__DEBUG_PRINT
CFLAGS += -mcpu=arm926ej-s -mtune=arm926ej-s -O3 -ftracer \
		-fstrength-reduce -fomit-frame-pointer -fstrict-aliasing -ffast-math
SFLAGS =  -march=armv5t -msoft-float -nostdinc
ASFLAGS = -mcpu=arm926ej-s -mfloat-abi=soft
export CFLAGS
SRCRES := $(shell echo rsc/*.rss)
EPOCRCFLAGS += -I./

all: mkdirs sis

include ../common/common_arm.mak
include uiq3.mak


$(NAME).mbg $(NAME).mbm : $(ICONS)
	@echo "Creating multibitmap file..."
	$(BMCONV) /h$(NAME).mbg $(NAME).mbm \
		/c24rsc/pico18x18.bmp /8rsc/pico18x18m.bmp \
		/c24rsc/pico40x40.bmp /8rsc/pico40x40m.bmp \
		/c24rsc/pico64x64.bmp /8rsc/pico64x64m.bmp


engine/blit_asm.o : engine/blit.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@


readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ UIQ

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif

rel: picodrive.sis readme.txt
	zip -9 -j ../../PicoDrive_uiq3_$(VER).zip $^
	mkdir bin_to_cso_mp3
	cp ../../tools/bin_to_cso_mp3/* bin_to_cso_mp3/
	zip -9 -r ../../PicoDrive_uiq3_$(VER).zip bin_to_cso_mp3
	rm -rf bin_to_cso_mp3
