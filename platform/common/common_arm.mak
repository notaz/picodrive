# asm stuff
ifeq "$(asm_render)" "1"
DEFINES += _ASM_DRAW_C
OBJS += pico/draw_arm.o pico/draw2_arm.o
endif
ifeq "$(asm_memory)" "1"
DEFINES += _ASM_MEMORY_C
OBJS += pico/memory_arm.o
endif
ifeq "$(asm_ym2612)" "1"
DEFINES += _ASM_YM2612_C
OBJS += pico/sound/ym2612_arm.o
endif
ifeq "$(asm_misc)" "1"
DEFINES += _ASM_MISC_C
OBJS += pico/misc_arm.o
OBJS += pico/cd/misc_arm.o
endif
ifeq "$(asm_cdpico)" "1"
DEFINES += _ASM_CD_PICO_C
OBJS += pico/cd/pico_arm.o
endif
ifeq "$(asm_cdmemory)" "1"
DEFINES += _ASM_CD_MEMORY_C
OBJS += pico/cd/memory_arm.o
endif


DIRS += cpu/Cyclone cpu/Cyclone/proj cpu/Cyclone/tools cpu/DrZ80


# build helix libs
../common/helix/$(CROSS)helix-mp3.a:
	make -C ../common/helix CROSS=$(CROSS) clean all

