# settings
use_musashi = 1
#use_fame = 1
#use_mz80 = 1
#profile = 1
#fake_in_gp2x = 1

-include Makefile.local

ifeq "$(profile)" "1"
CFLAGS += -O3 -Wall
CFLAGS += -ftracer -fstrength-reduce -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math
CFLAGS += -fprofile-generate
else
CFLAGS = -ggdb -Wall -falign-functions=2
endif
DEFINES = _UNZIP_SUPPORT IO_STATS IN_EVDEV
CFLAGS += -I../.. -I.

# gtk
CFLAGS  += `pkg-config --cflags gtk+-2.0`
LDFLAGS += `pkg-config --libs gtk+-2.0`
CFLAGS  += `pkg-config --cflags gthread-2.0`
LDFLAGS += `pkg-config --libs gthread-2.0`

# frontend
OBJS += platform/gp2x/emu.o blit.o in_evdev.o plat.o sndout_oss.o gp2x.o log_io.o

# common
OBJS += platform/common/main.o platform/common/emu.o platform/common/menu.o \
	platform/common/config.o platform/common/fonts.o platform/common/readpng.o \
	platform/common/input.o

ifeq "$(fake_in_gp2x)" "1"
DEFINES += IN_GP2X FAKE_IN_GP2X
OBJS += platform/gp2x/in_gp2x.o
endif

# Pico
OBJS += pico/area.o pico/cart.o pico/memory.o pico/misc.o pico/pico.o pico/sek.o \
	pico/videoport.o pico/draw2.o pico/draw.o pico/z80if.o pico/patch.o \
	pico/sms.o pico/debug.o
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
DEFINES += EMU_M68K
OBJS += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
OBJS += cpu/musashi/m68kdasm.o
endif
ifeq "$(use_fame)" "1"
DEFINES += EMU_F68K
OBJS += cpu/fame/famec.o
endif
# z80
ifeq "$(use_mz80)" "1"
DEFINES += _USE_MZ80
OBJS += cpu/mz80/mz80.o
else
DEFINES += _USE_CZ80
OBJS += cpu/cz80/cz80.o
endif
# misc
ifeq "$(use_fame)" "1"
ifeq "$(use_musashi)" "1"
OBJS += pico/debugCPU.o
endif
endif

CFLAGS += $(addprefix -D,$(DEFINES))

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
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -lm -lpng -Wl,-Map=PicoDrive.map -o $@

mkdirs:
	mkdir -p $(DIRS)

pico/carthw/svp/compiler.o : ../../pico/carthw/svp/gen_arm.c
pico/pico.o pico/cd/pico.o : ../../pico/pico_cmn.c ../../pico/pico_int.h
pico/memory.o pico/cd/memory.o : ../../pico/memory_cmn.c ../../pico/pico_int.h
platform/common/menu.o : revision.h

revision.h:
	@echo "#define REVISION \"`svn info | grep Revision | cut -c 11-`\"" > /tmp/r.tmp
	@diff -q $@ /tmp/r.tmp > /dev/null 2>&1 || mv -f /tmp/r.tmp $@

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
	$(CC) $(CFLAGS) -c $< -o $@
.s.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@


#pico/sound/ym2612.o : ../../pico/sound/ym2612.c
#	@echo ">>>" $@
#	$(CC) $(CFLAGS) -c $< -o $@

cpu/fame/famec.o : ../../cpu/fame/famec.c ../../cpu/fame/famec_opcodes.h
	@echo ">>>" $<
	$(CC) $(CFLAGS) -Wno-unused -c $< -o $@

