
# you may or may not need to change this
#devkit_path = x:/stuff/dev/devkitgp2x/
devkit_path = /usr/local/devkitPro/devkitGP2X/
lgcc_path = $(devkit_path)lib/gcc/arm-linux/4.0.3/
CROSS = arm-linux-
#CROSS = $(devkit_path)bin/arm-linux-

# settings
dprint = 1
#mz80 = 1
#debug_cyclone = 1
asm_memory = 1
asm_render = 1
asm_ym2612 = 1
#profile = 1
#use_musashi = 1
#up = 1

DEFINC = -I../.. -I. -D__GP2X__ -D_UNZIP_SUPPORT # -DBENCHMARK
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
OBJS += 940ctl_ym2612.o
# Pico
OBJS += ../../Pico/Area.o ../../Pico/Cart.o ../../Pico/Utils.o ../../Pico/Memory.o ../../Pico/Misc.o \
		../../Pico/Pico.o ../../Pico/Sek.o ../../Pico/VideoPort.o ../../Pico/Draw2.o ../../Pico/Draw.o
# Pico - CD
OBJS += ../../Pico/cd/Pico.o ../../Pico/cd/Memory.o ../../Pico/cd/Sek.o ../../Pico/cd/LC89510.o \
		../../Pico/cd/cd_sys.o
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
# Pico - sound
OBJS += ../../Pico/sound/sound.o ../../Pico/sound/sn76496.o ../../Pico/sound/ym2612.o
# zlib
OBJS += ../../zlib/gzio.o ../../zlib/inffast.o ../../zlib/inflate.o ../../zlib/inftrees.o ../../zlib/trees.o \
	../../zlib/deflate.o ../../zlib/crc32.o ../../zlib/adler32.o ../../zlib/zutil.o ../../zlib/compress.o
# unzip
OBJS += ../../unzip/unzip.o
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

all: PicoDrive.gpe code940.bin

PicoDrive.gpe : $(OBJS)
	@echo $@
	@$(GCC) $(COPT) $(OBJS) $(PRELIBS) -lm -o $@
	@$(STRIP) $@
#	@$(GCC) $(COPT) $(OBJS) $(PRELIBS) -lm -o PicoDrive_.gpe
#	@gpecomp PicoDrive_.gpe $@
ifeq "$(up)" "1"
	@cmd //C copy $@ \\\\10.0.1.2\\gp2x\\mnt\\sd\\games\\PicoDrive\\
endif

up: up940
	@cmd //C copy PicoDrive.gpe \\\\10.0.1.2\\gp2x\\mnt\\sd\\games\\PicoDrive\\
up940:
	@cmd //C copy code940.bin \\\\10.0.1.2\\gp2x\\mnt\\sd\\games\\PicoDrive\\

testrefr.gpe : test.o gp2x.o asmutils.o
	@echo $@
	@$(GCC) $(COPT) $^ $(PRELIBS) -o $@
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

# build Cyclone
../../cpu/Cyclone/proj/Cyclone.s :
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj -f Makefile.linux


# stuff for 940 core

# init, emu_control, emu
OBJS940 += 940init.o 940.o 940ym2612.o
# the asm code seems to be faster when run on 920, but not on 940 for some reason
# OBJS940 += ../../Pico/sound/ym2612_asm.o

# uClibc library code
OBJS940 += uClibc/memset.o uClibc/s_floor.o uClibc/e_pow.o uClibc/e_sqrt.o uClibc/s_fabs.o
OBJS940 += uClibc/s_scalbn.o uClibc/s_copysign.o uClibc/k_sin.o uClibc/k_cos.o uClibc/s_sin.o
OBJS940 += uClibc/e_rem_pio2.o uClibc/k_rem_pio2.o uClibc/e_log.o uClibc/wrappers.o

code940.bin : code940.gpe
	@echo $@
	@$(OBJCOPY) -O binary $< $@

code940.gpe : $(OBJS940)
	@echo $@
	@$(LD) -static -e code940 -Ttext 0x0 $^ -L$(lgcc_path) -lgcc -o $@

940ym2612.o : ../../Pico/sound/ym2612.c
	@echo $@
	@$(GCC) $(COPT_COMMON) -mtune=arm940t $(DEFINC) -DEXTERNAL_YM2612 -c $< -o $@


# cleanup
clean: clean_pd clean_940
tidy: tidy_pd tidy_940

clean_pd: tidy_pd
	@$(RM) PicoDrive.gpe
tidy_pd:
	@$(RM) $(OBJS)
#	@make -C ../../cpu/Cyclone/proj -f Makefile.linux clean

clean_940: tidy_940
	@$(RM) code940.bin
tidy_940:
	@$(RM) code940.gpe $(OBJS940)

clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete

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

uClibc/e_pow.o : uClibc/e_pow.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@

uClibc/e_sqrt.o : uClibc/e_sqrt.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@
