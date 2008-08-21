
# you may or may not need to change this
export CROSS = arm-wince-cegcc-

# settings
asm_memory = 1
asm_render = 1
asm_ym2612 = 1
asm_misc = 1
asm_cdpico = 1
asm_cdmemory = 1
amalgamate = 0
#profile = 1



DEFINC = -I../.. -I. -D__GIZ__ -DNO_SYNC
COPT_COMMON = -Wall -Winline
ifeq ($(DEBUG),)
COPT_COMMON += -O2 -ftracer -fomit-frame-pointer -fstrict-aliasing -ffast-math
else
COPT_COMMON += -ggdb
endif
ifeq "$(profile)" "1"
COPT_COMMON += -fprofile-generate
endif
ifeq "$(profile)" "2"
COPT_COMMON += -fprofile-use
endif
CFLAGS = $(COPT_COMMON) -mcpu=arm920t -mtune=arm920t
SFLAGS = $(CFLAGS)
ASFLAGS = -mcpu=arm920t -mfloat-abi=soft
CC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld

# frontend
OBJS += main.o emu.o menu.o giz.o asm_utils.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/fonts.o platform/common/config.o \
	platform/common/arm_utils.o platform/common/readpng.o platform/common/mp3_helix.o

# Pico
ifeq "$(amalgamate)" "1"
OBJS += PicoAll.o
else
OBJS += Pico/Area.o Pico/Cart.o Pico/Memory.o Pico/Misc.o Pico/Pico.o Pico/Sek.o \
		Pico/VideoPort.o Pico/Draw2.o Pico/Draw.o Pico/Patch.o Pico/Debug.o
# Pico - CD
OBJS += Pico/cd/Pico.o Pico/cd/Memory.o Pico/cd/Sek.o Pico/cd/LC89510.o \
		Pico/cd/cd_sys.o Pico/cd/cd_file.o Pico/cd/gfx_cd.o Pico/cd/cue.o \
		Pico/cd/Area.o Pico/cd/Misc.o Pico/cd/pcm.o Pico/cd/buffering.o
endif
# Pico - Pico
OBJS += Pico/Pico/Pico.o Pico/Pico/Memory.o Pico/Pico/xpcm.o
# Pico - carthw
OBJS += Pico/carthw/carthw.o Pico/carthw/svp/svp.o Pico/carthw/svp/Memory.o \
		Pico/carthw/svp/ssp16.o Pico/carthw/svp/compiler.o Pico/carthw/svp/stub_arm.o

# asm stuff
ifeq "$(asm_render)" "1"
DEFINC += -D_ASM_DRAW_C
OBJS += Pico/draw_asm.o Pico/draw2_asm.o
endif
ifeq "$(asm_memory)" "1"
DEFINC += -D_ASM_MEMORY_C
OBJS += Pico/memory_asm.o
endif
ifeq "$(asm_ym2612)" "1"
DEFINC += -D_ASM_YM2612_C
OBJS += Pico/sound/ym2612_asm.o
endif
ifeq "$(asm_misc)" "1"
DEFINC += -D_ASM_MISC_C
OBJS += Pico/misc_asm.o
OBJS += Pico/cd/misc_asm.o
endif
ifeq "$(asm_cdpico)" "1"
DEFINC += -D_ASM_CD_PICO_C
OBJS += Pico/cd/pico_asm.o
endif
ifeq "$(asm_cdmemory)" "1"
DEFINC += -D_ASM_CD_MEMORY_C
OBJS += Pico/cd/memory_asm.o
endif

# Pico - sound
ifneq "$(amalgamate)" "1"
OBJS += Pico/sound/sound.o
endif
OBJS += Pico/sound/mix_asm.o
OBJS += Pico/sound/sn76496.o Pico/sound/ym2612.o
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o
# CPU cores
# Cyclone
DEFINC += -DEMU_C68K
OBJS += cpu/Cyclone/proj/Cyclone.o cpu/Cyclone/tools/idle.o
# drz80
DEFINC += -D_USE_DRZ80
OBJS += cpu/DrZ80/drz80.o

vpath %.c = ../..
vpath %.s = ../..
vpath %.S = ../..
DIRS = platform platform/common Pico Pico/cd Pico/Pico Pico/sound Pico/carthw/svp \
	zlib unzip cpu cpu/musashi cpu/Cyclone/proj cpu/Cyclone/tools cpu/mz80 cpu/DrZ80


all: mkdirs PicoDrive.exe readme.txt

PicoDrive.exe : $(OBJS) ../common/helix/$(CROSS)helix-mp3.a
	@echo ">>>" $@
	$(CC) -o $@ -static $(CFLAGS) $^ -lm -lpng -Lkgsdk/ -lKGSDK -Wl,-Map=PicoDrive.map \
		2>&1 | grep -v ".idata$$4" # | grep -v "supports interworking, whereas"
ifeq ($(DEBUG),)
	$(STRIP) $@
endif
ifeq "$(profile)" "1"
	../../tools/gcda $@
endif


readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ GIZ

include ../common/common_arm.mak


# cleanup
clean: tidy
	@$(RM) PicoDrive.exe
tidy:
	@$(RM) $(OBJS)


up: PicoDrive.exe
	synce-pcp -d 3 PicoDrive.exe ":/SD Card/emus/PicoDrive/PicoDrive.exe"

run: up
	synce-prun "/SD Card/emus/PicoDrive/PicoDrive.exe"

run_only:
	synce-prun "/SD Card/emus/PicoDrive/PicoDrive.exe"

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif

rel: PicoDrive.exe kgsdk/ARMV4IRel/KGSDK.dll ../game_def.cfg readme.txt
	zip -9 -j ../../PicoDrive_giz_$(VER).zip $^
	ln -s ../gp2x/skin skin
	zip -9 -r ../../PicoDrive_giz_$(VER).zip skin -i \*.png -i \*.txt
	rm skin

