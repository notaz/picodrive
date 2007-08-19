
# settings
dprint = 1
# profile = 1


DEFINC = -I../.. -I. -D__GP2X__ -D_UNZIP_SUPPORT # -DBENCHMARK
GCC = gcc
STRIP = strip
AS = gcc

ifeq "$(profile)" "1"
COPT_COMMON = -s -O3 -ftracer -fstrength-reduce -Wall -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math -fprofile-generate # -static
COPT = $(COPT_COMMON) # -mtune=arm920t
else
COPT = -ggdb -Wall -fno-strict-aliasing # -pg -O3  -ftracer -fstrength-reduce -funroll-loops -fomit-frame-pointer -ffast-math
COPT_COMMON = $(COPT)
endif

# gtk
COPT    += `pkg-config --cflags gtk+-2.0`
LDFLAGS += `pkg-config --libs gtk+-2.0`
COPT    += `pkg-config --cflags gthread-2.0`
LDFLAGS += `pkg-config --libs gthread-2.0`

# frontend
OBJS += ../gp2x/main.o ../gp2x/menu.o ../gp2x/fonts.o ../gp2x/emu.o ../gp2x/usbjoy.o blit.o gp2x.o 940ctl_ym2612.o
# Pico
OBJS += ../../Pico/Area.o ../../Pico/Cart.o ../../Pico/Utils.o ../../Pico/Memory.o ../../Pico/Misc.o \
		../../Pico/Pico.o ../../Pico/Sek.o ../../Pico/VideoPort.o ../../Pico/Draw2.o ../../Pico/Draw.o \
		../../Pico/Patch.o
# Pico - CD
OBJS += ../../Pico/cd/Pico.o ../../Pico/cd/Memory.o ../../Pico/cd/Sek.o ../../Pico/cd/LC89510.o \
		../../Pico/cd/cd_sys.o ../../Pico/cd/cd_file.o ../../Pico/cd/gfx_cd.o \
		../../Pico/cd/Area.o ../../Pico/cd/Misc.o ../../Pico/cd/pcm.o ../../Pico/cd/buffering.o
# Pico - sound
OBJS += ../../Pico/sound/sound.o ../../Pico/sound/sn76496.o ../../Pico/sound/ym2612.o ../../Pico/sound/mix.o
# zlib
OBJS += ../../zlib/gzio.o ../../zlib/inffast.o ../../zlib/inflate.o ../../zlib/inftrees.o ../../zlib/trees.o \
	../../zlib/deflate.o ../../zlib/crc32.o ../../zlib/adler32.o ../../zlib/zutil.o ../../zlib/compress.o
# unzip
OBJS += ../../unzip/unzip.o ../../unzip/unzip_stream.o
# mp3
OBJS += ../gp2x/mp3.o
# CPU cores
DEFINC += -DEMU_M68K
OBJS += ../../cpu/musashi/m68kops.o ../../cpu/musashi/m68kcpu.o
# mz80
DEFINC += -D_USE_MZ80
OBJS += ../../cpu/mz80/mz80.o

# faked asm
#DEFINC += -D_ASM_DRAW_C
#DEFINC += -D_ASM_MEMORY_C
#DEFINC += -D_ASM_YM2612_C
OBJS += fakedasm.o


all: PicoDrive
clean: tidy
	@$(RM) PicoDrive
tidy:
	@$(RM) $(OBJS)
	@make -C ../../cpu/mz80/ clean
	@make -C ../gp2x/helix/ X86=1 clean

PicoDrive : $(OBJS) ../gp2x/helix/helix_mp3_x86.a
	@echo $@
	@$(GCC) $(COPT) $^ $(LDFLAGS) -lm -Wl,-Map=PicoDrive.map -o $@


../../cpu/mz80/mz80.o : ../../cpu/mz80/mz80.asm
	@echo $@
	@nasm -f elf $< -o $@

../../cpu/mz80/mz80.asm :
	@make -C ../../cpu/mz80/

../gp2x/helix/helix_mp3_x86.a:
	@make -C ../gp2x/helix/ X86=1 clean all

.c.o:
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.s.o:
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -c $< -o $@


../../Pico/sound/ym2612.o : ../../Pico/sound/ym2612.c
	@echo $@
	@$(GCC) $(COPT_COMMON) $(DEFINC) -c $< -o $@ # -mtune=arm940t -DEXTERNAL_YM2612

# faked asm
../../Pico/Draw.o : ../../Pico/Draw.c
	@echo $<
	@$(GCC) $(COPT) $(DEFINC) -D_ASM_DRAW_C -c $< -o $@

