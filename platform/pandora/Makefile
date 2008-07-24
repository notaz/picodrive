export CROSS = arm-none-linux-gnueabi-

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
COPT_COMMON = -Wall -Winline -mcpu=cortex-a8 -mtune=cortex-a8 -march=armv7-a
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
COPT = $(COPT_COMMON)
ASOPT =
GCC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# frontend
OBJS += pandora.o main.o emu.o asm_utils.o platform/gp2x/menu.o platform/gp2x/usbjoy.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/fonts.o platform/common/config.o \
	platform/common/arm_utils.o platform/common/mp3_helix.o # platform/common/readpng.o

# Pico
ifeq "$(amalgamate)" "1"
OBJS += ../../PicoAll.o
else
OBJS += Pico/Area.o Pico/Cart.o Pico/Memory.o Pico/Misc.o Pico/Pico.o Pico/Sek.o \
		Pico/VideoPort.o Pico/Draw2.o Pico/Draw.o Pico/Patch.o Pico/Debug.o
# Pico - CD
OBJS += Pico/cd/Pico.o Pico/cd/Memory.o Pico/cd/Sek.o Pico/cd/LC89510.o \
		Pico/cd/cd_sys.o Pico/cd/cd_file.o Pico/cd/cue.o Pico/cd/gfx_cd.o \
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
# debug
ifeq "$(debug_cyclone)" "1"
OBJS += Pico/DebugCPU.o cpu/musashi/m68kdasm.o
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
DIRS = platform platform/gp2x platform/common Pico Pico/cd Pico/Pico Pico/sound Pico/carthw/svp \
	zlib unzip cpu cpu/musashi cpu/Cyclone/proj cpu/Cyclone/tools cpu/mz80 cpu/DrZ80


all: mkdirs PicoDrive

PicoDrive : $(OBJS) ../common/helix/$(CROSS)helix-mp3.a
	@echo ">>>" $@
	$(GCC) -o $@ $(COPT) $^ -lm -Wl,-Map=PicoDrive.map # -lpng
ifeq ($(DEBUG),)
	$(STRIP) $@
endif


# cleanup
clean: tidy
	$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)
#	rm -rf $(DIRS) # don't clean, gcda may be there
#	@make -C ../../cpu/Cyclone/proj -f Makefile.linux clean


clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete


mkdirs:
	mkdir -p $(DIRS)

.c.o:
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.s.o:
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.S.o:
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@

Pico/carthw/svp/compiler.o : ../../Pico/carthw/svp/ssp16.o ../../Pico/carthw/svp/gen_arm.c

Pico/draw_asm.o : ../../Pico/Draw.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/draw2_asm.o : ../../Pico/Draw2.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/memory_asm.o : ../../Pico/Memory.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/sound/ym2612_asm.o : ../../Pico/sound/ym2612.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/sound/mix_asm.o : ../../Pico/sound/mix.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/misc_asm.o : ../../Pico/Misc.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/cd/pico_asm.o : ../../Pico/cd/Pico.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/cd/memory_asm.o : ../../Pico/cd/Memory.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@
Pico/cd/misc_asm.o : ../../Pico/cd/Misc.s
	@echo ">>>" $<
	$(AS) $(ASOPT) $< -o $@

# build Cyclone
../../cpu/Cyclone/proj/Cyclone.s :
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj CONFIG_FILE=config_pico.h

../../cpu/musashi/m68kops.c :
	@make -C ../../cpu/musashi

Pico/Pico.o Pico/cd/Pico.o: ../../Pico/PicoFrameHints.c ../../Pico/PicoInt.h
Pico/Memory.o Pico/cd/Memory.o : ../../Pico/MemoryCmn.c ../../Pico/PicoInt.h

# build helix libs
../common/helix/$(CROSS)helix-mp3.a:
	make -C ../common/helix clean all

readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ GP2X

../../tools/textfilter: ../../tools/textfilter.c
	make -C ../../tools/ textfilter


# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
$(error TODO)
ifeq ($(VER),)
$(error need VER)
endif
endif

