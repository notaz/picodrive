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
OBJS += cpu/sh2/stub_$(ARCH).o
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

