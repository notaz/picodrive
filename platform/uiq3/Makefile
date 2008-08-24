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
OBJ += App.o Engine.o Dialogs.o CSimpleTextParser.o emu.o
# engine
OBJ += engine/main.o engine/vid.o engine/polledas.o engine/audio_mediaserver.o engine/debug.o
ifeq "$(asm_blit)" "1"
OBJ += engine/blit_asm.o
else
OBJ += engine/blit.o
endif
# common
OBJ += ../common/emu.o ../common/config.o ../common/menu.o ../common/mp3_helix.o
# Pico
OBJ += Pico/Area.o Pico/Cart.o Pico/Memory.o Pico/Misc.o Pico/Pico.o Pico/Sek.o \
		Pico/VideoPort.o Pico/Draw2.o Pico/Draw.o Pico/Patch.o Pico/Debug.o
# Pico - CD
OBJ += Pico/cd/Pico.o Pico/cd/Memory.o Pico/cd/Sek.o Pico/cd/LC89510.o \
		Pico/cd/cd_sys.o Pico/cd/cd_file.o Pico/cd/cue.o Pico/cd/gfx_cd.o \
		Pico/cd/Area.o Pico/cd/Misc.o Pico/cd/pcm.o Pico/cd/buffering.o
# Pico - Pico
OBJ += Pico/Pico/Pico.o Pico/Pico/Memory.o Pico/Pico/xpcm.o
# Pico - carthw
OBJ += Pico/carthw/carthw.o Pico/carthw/svp/svp.o Pico/carthw/svp/Memory.o \
		Pico/carthw/svp/ssp16.o Pico/carthw/svp/compiler.o Pico/carthw/svp/stub_arm.o

# asm stuff
ifeq "$(asm_render)" "1"
CFLAGS += -D_ASM_DRAW_C
OBJ += Pico/draw_asm.o Pico/draw2_asm.o
endif
ifeq "$(asm_memory)" "1"
CFLAGS += -D_ASM_MEMORY_C
OBJ += Pico/memory_asm.o
endif
ifeq "$(asm_ym2612)" "1"
CFLAGS += -D_ASM_YM2612_C
OBJ += Pico/sound/ym2612_asm.o
endif
ifeq "$(asm_misc)" "1"
CFLAGS += -D_ASM_MISC_C
OBJ += Pico/misc_asm.o
OBJ += Pico/cd/misc_asm.o
endif
ifeq "$(asm_cdpico)" "1"
CFLAGS += -D_ASM_CD_PICO_C
OBJ += Pico/cd/pico_asm.o
endif
ifeq "$(asm_cdmemory)" "1"
CFLAGS += -D_ASM_CD_MEMORY_C
OBJ += Pico/cd/memory_asm.o
endif

# Pico - sound
OBJ += Pico/sound/sound.o
OBJ += Pico/sound/mix_asm.o
OBJ += Pico/sound/sn76496.o Pico/sound/ym2612.o
# zlib
OBJ += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o
# unzip
OBJ += unzip/unzip.o unzip/unzip_stream.o
# CPU cores
ifeq "$(use_musashi)" "1"
CFLAGS += -DEMU_M68K
OBJ += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
endif
ifeq "$(use_cyclone)" "1"
CFLAGS += -DEMU_C68K
OBJ += cpu/Cyclone/proj/Cyclone.o cpu/Cyclone/tools/idle.o
endif
# drz80
CFLAGS += -D_USE_DRZ80
OBJ += cpu/DrZ80/drz80.o
# helix
OBJ += ../common/helix/$(CROSS)helix-mp3.a


vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
DIRS = platform platform/common Pico Pico/cd Pico/Pico Pico/sound Pico/carthw/svp \
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

include uiq3.mak
include ../common/common_arm.mak


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
