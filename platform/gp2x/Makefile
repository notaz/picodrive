
# you may or may not need to change this
#devkit_path = x:/stuff/dev/devkitgp2x/
CROSS = arm-linux-
#CROSS = $(devkit_path)bin/arm-linux-

# settings
dprint = 1
#mz80 = 1
#debug_cyclone = 1
asm_memory = 0	# TODO
asm_render = 1
asm_ym2612 = 1
asm_misc = 1
#profile = 1
#use_musashi = 1
#up = 1

DEFINC = -I../.. -I. -DARM -D__GP2X__ -D_UNZIP_SUPPORT # -DBENCHMARK
COPT_COMMON = -static -s -O3 -ftracer -fstrength-reduce -Wall -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math
ifeq "$(profile)" "1"
COPT_COMMON += -fprofile-generate
endif
ifeq "$(profile)" "2"
COPT_COMMON += -fprofile-use
endif
COPT = $(COPT_COMMON) -mtune=arm920t
ASOPT = -mcpu=arm920t -mfloat-abi=soft
GCC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# frontend
OBJS += main.o menu.o gp2x.o usbjoy.o emu.o squidgehack.o asmutils.o cpuctrl.o
# 940 core control
OBJS += 940ctl.o
# Pico
OBJS += ../../Pico/Area.o ../../Pico/Cart.o ../../Pico/Utils.o ../../Pico/Memory.o ../../Pico/Misc.o \
		../../Pico/Pico.o ../../Pico/Sek.o ../../Pico/VideoPort.o ../../Pico/Draw2.o ../../Pico/Draw.o
# Pico - CD
OBJS += ../../Pico/cd/Pico.o ../../Pico/cd/Memory.o ../../Pico/cd/Sek.o ../../Pico/cd/LC89510.o \
		../../Pico/cd/cd_sys.o ../../Pico/cd/cd_file.o ../../Pico/cd/gfx_cd.o \
		../../Pico/cd/Area.o ../../Pico/cd/Misc.o ../../Pico/cd/pcm.o
# asm stuff
ifeq "$(asm_render)" "1"
DEFINC += -D_ASM_DRAW_C
OBJS += ../../Pico/draw_asm.o ../../Pico/draw2_asm.o
endif
ifeq "$(asm_memory)" "1"
DEFINC += -D_ASM_MEMORY_C
OBJS += ../../Pico/memory_asm.o
endif
ifeq "$(asm_ym2612)" "1"
DEFINC += -D_ASM_YM2612_C
OBJS += ../../Pico/sound/ym2612_asm.o
endif
ifeq "$(asm_misc)" "1"
DEFINC += -D_ASM_MISC_C
OBJS += ../../Pico/misc_asm.o
endif
# Pico - sound
OBJS += ../../Pico/sound/mix_asm.o
OBJS += ../../Pico/sound/sound.o ../../Pico/sound/sn76496.o ../../Pico/sound/ym2612.o
# zlib
OBJS += ../../zlib/gzio.o ../../zlib/inffast.o ../../zlib/inflate.o ../../zlib/inftrees.o ../../zlib/trees.o \
	../../zlib/deflate.o ../../zlib/crc32.o ../../zlib/adler32.o ../../zlib/zutil.o ../../zlib/compress.o
# unzip
OBJS += ../../unzip/unzip.o
# mp3
OBJS += mp3.o
# CPU cores
ifeq "$(use_musashi)" "1"
DEFINC += -DEMU_M68K
OBJS += _build\m68kcpu.o _build\m68kopac.o _build\m68kopdm.o _build\m68kopnz.o _build\m68kops.o
else
DEFINC += -DEMU_C68K
OBJS += ../../cpu/Cyclone/proj/Cyclone.o
endif
# drz80/mz80
ifeq "$(mz80)" "1"
DEFINC += -D_USE_MZ80
OBJS += ../../cpu/mz80/mz80.o
else
DEFINC += -D_USE_DRZ80
OBJS += ../../cpu/DrZ80/drz80.o
endif


all: PicoDrive.gpe

PicoDrive.gpe : $(OBJS) helix/helix_mp3.a
	@echo $@
	@$(GCC) -o $@ $(COPT) $^ -lm
	@$(STRIP) $@
#	@$(GCC) $(COPT) $(OBJS) -lm -o PicoDrive_.gpe
#	@gpecomp PicoDrive_.gpe $@
ifeq "$(up)" "1"
	@cmd //C copy $@ \\\\10.0.1.2\\gp2x\\mnt\\sd\\games\\PicoDrive\\
endif

up: PicoDrive.gpe
	@cp -v PicoDrive.gpe /mnt/gp2x/mnt/sd/games/PicoDrive/

#	@cmd //C copy PicoDrive.gpe \\\\10.0.1.2\\gp2x\\mnt\\sd\\games\\PicoDrive\\


testrefr.gpe : test.o gp2x.o asmutils.o
	@echo $@
	@$(GCC) $(COPT) $^ -o $@
	@$(STRIP) $@

.c.o:
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.s.o:
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -c $< -o $@

../../Pico/draw_asm.o : ../../Pico/Draw.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@
../../Pico/draw2_asm.o : ../../Pico/Draw2.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@
../../Pico/memory_asm.o : ../../Pico/Memory.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@
../../Pico/sound/ym2612_asm.o : ../../Pico/sound/ym2612.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@
../../Pico/sound/mix_asm.o : ../../Pico/sound/mix.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@
../../Pico/misc_asm.o : ../../Pico/misc.s
	@echo $<
	@$(AS) $(ASOPT) $< -o $@

# build Cyclone
../../cpu/Cyclone/proj/Cyclone.s :
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj -f Makefile.linux

# build helix libs
helix/helix_mp3.a:
	make -C helix


# cleanup
clean: tidy
	@$(RM) PicoDrive.gpe
tidy:
	@$(RM) $(OBJS)
#	@make -C ../../cpu/Cyclone/proj -f Makefile.linux clean


clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif

rel: PicoDrive.gpe code940/code940.bin ../readme.txt config.txt
	zip -9 -j ../../PicoDrive_$(VER).zip $^ mmuhack.o

code940/code940.bin:
	make -C code940/


# test
usbjoy.o : usbjoy.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@

../../Pico/Cart.o : ../../Pico/Cart.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@

../../zlib/trees.o : ../../zlib/trees.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@

