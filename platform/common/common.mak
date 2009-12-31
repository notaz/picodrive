ifneq ($(DEBUG),)
CFLAGS += -ggdb
endif
ifeq "$(profile)" "1"
CFLAGS += -fprofile-generate
endif
ifeq "$(profile)" "2"
CFLAGS += -fprofile-use
endif
ifeq "$(pprof)" "1"
DEFINES += PPROF
OBJS += platform/linux/pprof.o
endif

# === Pico core ===
# Pico
OBJS += pico/area.o pico/cart.o pico/memory.o pico/pico.o pico/sek.o pico/z80if.o \
	pico/videoport.o pico/draw2.o pico/draw.o pico/mode4.o pico/sms.o \
	pico/misc.o pico/eeprom.o pico/patch.o pico/debug.o
# CD
OBJS += pico/cd/pico.o pico/cd/memory.o pico/cd/sek.o pico/cd/LC89510.o \
	pico/cd/cd_sys.o pico/cd/cd_file.o pico/cd/cue.o pico/cd/gfx_cd.o \
	pico/cd/area.o pico/cd/misc.o pico/cd/pcm.o pico/cd/buffering.o
# 32X
OBJS += pico/32x/32x.o pico/32x/memory.o pico/32x/draw.o pico/32x/pwm.o
# Pico
OBJS += pico/pico/pico.o pico/pico/memory.o pico/pico/xpcm.o
# carthw
OBJS += pico/carthw/carthw.o
# SVP
OBJS += pico/carthw/svp/svp.o pico/carthw/svp/memory.o \
	pico/carthw/svp/ssp16.o
# sound
OBJS += pico/sound/sound.o
OBJS += pico/sound/sn76496.o pico/sound/ym2612.o

# === CPU cores ===
# --- M68k ---
ifeq "$(use_musashi)" "1"
DEFINES += EMU_M68K
OBJS += cpu/musashi/m68kops.o cpu/musashi/m68kcpu.o
#OBJS += cpu/musashi/m68kdasm.o
endif
ifeq "$(use_cyclone)" "1"
DEFINES += EMU_C68K
OBJS += pico/m68kif_cyclone.o cpu/Cyclone/proj/Cyclone.o cpu/Cyclone/tools/idle.o
endif
ifeq "$(use_fame)" "1"
DEFINES += EMU_F68K
OBJS += cpu/fame/famec.o
endif

# --- Z80 ---
ifeq "$(use_mz80)" "1"
DEFINES += _USE_MZ80
OBJS += cpu/mz80/mz80.o
endif
#
ifeq "$(use_drz80)" "1"
DEFINES += _USE_DRZ80
OBJS += cpu/DrZ80/drz80.o
endif
#
ifeq "$(use_cz80)" "1"
DEFINES += _USE_CZ80
OBJS += cpu/cz80/cz80.o
endif

# --- SH2 ---
OBJS += cpu/sh2/sh2.o
OBJS += cpu/drc/cmn.o
#
ifeq "$(use_sh2drc)" "1"
DEFINES += DRC_SH2
OBJS += cpu/sh2/compiler.o
ifdef drc_debug
DEFINES += DRC_DEBUG=$(drc_debug)
OBJS += cpu/sh2/mame/sh2dasm.o
OBJS += platform/linux/host_dasm.o
LDFLAGS += -lbfd -lopcodes -liberty
endif
ifeq "$(drc_debug_interp)" "1"
DEFINES += DRC_DEBUG_INTERP
use_sh2mame = 1
endif
endif
#
ifeq "$(use_sh2mame)" "1"
OBJS += cpu/sh2/mame/sh2pico.o
endif


DIRS += platform platform/common pico pico/cd pico/pico pico/32x pico/sound pico/carthw/svp \
	cpu cpu/musashi cpu/cz80 cpu/fame cpu/sh2/mame cpu/drc


# common rules
.c.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@
.s.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@

clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete

mkdirs:
	mkdir -p $(DIRS)

../../tools/textfilter: ../../tools/textfilter.c
	make -C ../../tools/ textfilter


# random deps
pico/carthw/svp/compiler.o : ../../cpu/drc/emit_$(ARCH).c
cpu/sh2/compiler.o : ../../cpu/drc/emit_$(ARCH).c
cpu/sh2/mame/sh2pico.o : ../../cpu/sh2/mame/sh2.c
pico/pico.o pico/cd/pico.o : ../../pico/pico_cmn.c ../../pico/pico_int.h
pico/memory.o pico/cd/memory.o : ../../pico/pico_int.h ../../pico/memory.h

../../cpu/musashi/m68kops.c :
	@make -C ../../cpu/musashi

../../cpu/mz80/mz80.asm :
	@make -C ../../cpu/mz80/

cpu/fame/famec.o : ../../cpu/fame/famec.c ../../cpu/fame/famec_opcodes.h
	@echo ">>>" $<
	$(CC) $(CFLAGS) -Wno-unused -c $< -o $@

../../cpu/Cyclone/proj/Cyclone.s:
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj CONFIG_FILE=config_pico.h

