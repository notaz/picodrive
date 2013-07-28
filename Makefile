TARGET ?= PicoDrive
CFLAGS += -Wall -ggdb -falign-functions=2
CFLAGS += -I.
ifndef DEBUG
CFLAGS += -O2 -DNDEBUG
endif
#CFLAGS += -DEVT_LOG
#CFLAGS += -DDRC_CMP
#drc_debug = 4
#profile = 1


all: config.mak target_

ifndef NO_CONFIG_MAK
ifneq ($(wildcard config.mak),)
config.mak: ./configure
	@echo $@ is out-of-date, running configure
	@sed -n "/.*Configured with/s/[^:]*: //p" $@ | sh
include config.mak
else
config.mak:
	@echo "Please run ./configure before running make!"
	@exit 1
endif
else # NO_CONFIG_MAK
config.mak:
endif

# default settings
ifeq "$(ARCH)" "arm"
use_cyclone ?= 1
use_drz80 ?= 1
use_sh2drc ?= 1
use_svpdrc ?= 1

asm_memory ?= 1
asm_render ?= 1
asm_ym2612 ?= 1
asm_misc ?= 1
asm_cdpico ?= 1
asm_cdmemory ?= 1
asm_mix ?= 1
else # if not arm
use_fame ?= 1
use_cz80 ?= 1
use_sh2mame ?= 1
endif

-include Makefile.local

ifneq "$(use_cyclone)" "1"
# due to CPU stop flag access
asm_cdpico = 0
asm_cdmemory = 0
endif

# frontend
ifeq "$(PLATFORM)" "generic"
OBJS += platform/linux/emu.o platform/linux/blit.o # FIXME
OBJS += platform/common/plat_sdl.o
OBJS += platform/libpicofe/plat_sdl.o platform/libpicofe/in_sdl.o
OBJS += platform/libpicofe/plat_dummy.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "pandora"
platform/common/menu_pico.o: CFLAGS += -DPANDORA
OBJS += platform/pandora/plat.o
OBJS += platform/pandora/asm_utils.o
OBJS += platform/common/arm_utils.o 
OBJS += platform/libpicofe/linux/fbdev.o 
OBJS += platform/libpicofe/linux/xenv.o
OBJS += platform/libpicofe/pandora/plat.o
USE_FRONTEND = 1
endif
ifeq "$(PLATFORM)" "libretro"
OBJS += platform/libretro.o 
endif

ifeq "$(USE_FRONTEND)" "1"

# common
OBJS += platform/common/main.o platform/common/emu.o \
	platform/common/menu_pico.o platform/common/config_file.o

# libpicofe
OBJS += platform/libpicofe/input.o platform/libpicofe/readpng.o \
	platform/libpicofe/fonts.o platform/libpicofe/linux/in_evdev.o \
	platform/libpicofe/linux/plat.o

# libpicofe - sound
OBJS += platform/libpicofe/sndout.o
ifneq ($(findstring oss,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_OSS
OBJS += platform/libpicofe/linux/sndout_oss.o
endif
ifneq ($(findstring alsa,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_ALSA
OBJS += platform/libpicofe/linux/sndout_alsa.o
endif
ifneq ($(findstring sdl,$(SOUND_DRIVERS)),)
platform/libpicofe/sndout.o: CFLAGS += -DHAVE_SDL
OBJS += platform/libpicofe/sndout_sdl.o
endif

ifeq "$(ARCH)" "arm"
OBJS += platform/libpicofe/arm_linux.o
endif

endif # USE_FRONTEND

OBJS += platform/common/mp3.o
ifeq "$(HAVE_LIBAVCODEC)" "1"
OBJS += platform/common/mp3_libavcodec.o
else
OBJS += platform/common/mp3_dummy.o
endif

# zlib
OBJS += zlib/gzio.o zlib/inffast.o zlib/inflate.o zlib/inftrees.o zlib/trees.o \
	zlib/deflate.o zlib/crc32.o zlib/adler32.o zlib/zutil.o zlib/compress.o zlib/uncompr.o
# unzip
OBJS += unzip/unzip.o unzip/unzip_stream.o


include platform/common/common.mak

OBJS += $(OBJS_COMMON)
CFLAGS += $(addprefix -D,$(DEFINES))

ifneq ($(findstring gcc,$(CC)),)
LDFLAGS += -Wl,-Map=$(TARGET).map
endif


target_: $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS)

pprof: platform/linux/pprof.c
	$(CC) -O2 -ggdb -DPPROF -DPPROF_TOOL -I../../ -I. $^ -o $@

tools/textfilter: tools/textfilter.c
	make -C tools/ textfilter

.s.o:
	$(CC) $(CFLAGS) -c $< -o $@

# random deps
pico/carthw/svp/compiler.o : cpu/drc/emit_$(ARCH).c
cpu/sh2/compiler.o : cpu/drc/emit_$(ARCH).c
cpu/sh2/mame/sh2pico.o : cpu/sh2/mame/sh2.c
pico/pico.o pico/cd/pico.o pico/32x/32x.o : pico/pico_cmn.c pico/pico_int.h
pico/memory.o pico/cd/memory.o : pico/pico_int.h pico/memory.h
cpu/fame/famec.o: cpu/fame/famec.c cpu/fame/famec_opcodes.h

# ----------- release -----------

VER ?= $(shell head -n 1 platform/common/version.h | \
	sed 's/.*"\(.*\)\.\(.*\)".*/\1\2/g')

ifeq "$(PLATFORM)" "pandora"

PND_MAKE ?= $(HOME)/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

/tmp/readme.txt: tools/textfilter platform/base_readme.txt
	tools/textfilter platform/base_readme.txt $@ PANDORA

/tmp/PicoDrive.pxml: platform/pandora/PicoDrive.pxml.template
	platform/pandora/make_pxml.sh $^ $@

rel: PicoDrive platform/pandora/PicoDrive.run platform/pandora/picorestore \
	pico/carthw.cfg /tmp/readme.txt platform/pandora/skin \
	platform/pandora/PicoDrive.png platform/pandora/PicoDrive_p.png \
	/tmp/PicoDrive.pxml
	rm -rf out
	mkdir out
	cp -r $^ out/
	$(PND_MAKE) -p PicoDrive_$(VER).pnd -d out -x out/PicoDrive.pxml -i out/PicoDrive.png -c

endif
