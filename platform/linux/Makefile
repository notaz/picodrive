
# settings
use_musashi = 1
#use_fame = 1
#use_mz80 = 1

# profile = 1


DEFINC = -I../.. -I. -D__GP2X__ -D_UNZIP_SUPPORT -DIO_STATS # -DBENCHMARK
GCC = gcc
STRIP = strip
AS = gcc

ifeq "$(profile)" "1"
COPT_COMMON = -s -O3 -ftracer -fstrength-reduce -Wall -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math -fprofile-generate
COPT = $(COPT_COMMON)
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
OBJS += platform/gp2x/main.o platform/gp2x/menu.o platform/gp2x/emu.o usbjoy.o blit.o \
		sndout_oss.o gp2x.o 940ctl_ym2612.o log_io.o

# common
OBJS += platform/common/emu.o platform/common/menu.o platform/common/config.o platform/common/fonts.o \
		platform/common/readpng.o platform/common/mp3_helix.o

# Pico
OBJS += pico/area.o pico/cart.o pico/memory.o pico/misc.o pico/pico.o pico/sek.o \
		pico/videoport.o pico/draw2.o pico/draw.o pico/patch.o pico/debug.o
# Pico - CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
		pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
		pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
# Pico - Pico
OBJS += pico/pico/pico.o pico/pico/memory.o pico/pico/xpcm.o
# Pico - sound
OBJS += pico/sound/sound.o pico/sound/sn76496.o pico/sound/ym2612.o pico/sound/mix.o
# Pico - carthw
OBJS += pico/carthw/carthw.o pico/carthw/svp/svp.o pico/carthw/svp/memory.o \
		pico/carthw/svp/ssp16.o pico/carthw/svp/compiler.o
# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o zlib/uncompr.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o
# CPU cores
ifeq "$(use_musashi)" "1"
DEFINC += -DEMU_M68K
OBJS += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
endif
ifeq "$(use_fame)" "1"
DEFINC += -DEMU_F68K
OBJS += cpu/fame/famec.o
endif
# z80
ifeq "$(use_mz80)" "1"
DEFINC += -D_USE_MZ80
OBJS += cpu/mz80/mz80.o
else
DEFINC += -D_USE_CZ80
OBJS += cpu/cz80/cz80.o
endif
# misc
ifeq "$(use_fame)" "1"
ifeq "$(use_musashi)" "1"
OBJS += pico/debugCPU.o
OBJS += cpu/musashi/m68kdasm.o
endif
endif
OBJS += cpu/musashi/m68kdasm.o

vpath %.c = ../..
DIRS = platform platform/gp2x platform/common pico pico/cd pico/pico pico/sound pico/carthw/svp \
	zlib unzip cpu cpu/musashi cpu/fame cpu/mz80 cpu/cz80

all: mkdirs PicoDrive
clean: tidy
	@$(RM) PicoDrive
tidy:
	$(RM) $(OBJS)
	rm -rf $(DIRS)
	@make -C ../../cpu/mz80/ clean
	@make -C ../common/helix/ X86=1 clean

PicoDrive : $(OBJS) ../common/helix/helix_mp3_x86.a
	@echo ">>>" $@
	$(GCC) $(COPT) $^ $(LDFLAGS) -lm -lpng -Wl,-Map=PicoDrive.map -o $@

mkdirs:
	mkdir -p $(DIRS)

pico/carthw/svp/compiler.o : ../../pico/carthw/svp/gen_arm.c
pico/pico.o pico/cd/pico.o : ../../pico/pico_cmn.c ../../pico/pico_int.h
pico/memory.o pico/cd/memory.o : ../../pico/memory_cmn.c ../../pico/pico_int.h

../../cpu/musashi/m68kops.c :
	@make -C ../../cpu/musashi

cpu/mz80/mz80.o : ../../cpu/mz80/mz80.asm
	@echo $@
	@nasm -f elf $< -o $@

../../cpu/mz80/mz80.asm :
	@make -C ../../cpu/mz80/

../common/helix/helix_mp3_x86.a:
	@make -C ../common/helix/ X86=1 clean all

.c.o:
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.s.o:
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@


pico/sound/ym2612.o : ../../pico/sound/ym2612.c
	@echo ">>>" $@
	$(GCC) $(COPT_COMMON) $(DEFINC) -c $< -o $@

cpu/fame/famec.o : ../../cpu/fame/famec.c ../../cpu/fame/famec_opcodes.h
	@echo ">>>" $<
	$(GCC) $(COPT) $(DEFINC) -Wno-unused -c $< -o $@


