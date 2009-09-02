# asm stuff
ifeq "$(asm_render)" "1"
DEFINC += -D_ASM_DRAW_C
OBJS += pico/draw_arm.o pico/draw2_arm.o
endif
ifeq "$(asm_memory)" "1"
DEFINC += -D_ASM_MEMORY_C
OBJS += pico/memory_arm.o
endif
ifeq "$(asm_ym2612)" "1"
DEFINC += -D_ASM_YM2612_C
OBJS += pico/sound/ym2612_arm.o
endif
ifeq "$(asm_misc)" "1"
DEFINC += -D_ASM_MISC_C
OBJS += pico/misc_arm.o
OBJS += pico/cd/misc_arm.o
endif
ifeq "$(asm_cdpico)" "1"
DEFINC += -D_ASM_CD_PICO_C
OBJS += pico/cd/pico_arm.o
endif
ifeq "$(asm_cdmemory)" "1"
DEFINC += -D_ASM_CD_MEMORY_C
OBJS += pico/cd/memory_arm.o
endif


.c.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) $(DEFINC) -c $< -o $@

.S.o:
	@echo ">>>" $<
	$(CC) $(SFLAGS) $(DEFINC) -c $< -o $@


../../tools/textfilter: ../../tools/textfilter.c
	make -C ../../tools/ textfilter

clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete

mkdirs:
	mkdir -p $(DIRS)

# deps
pico/carthw/svp/compiler.o : ../../pico/carthw/svp/ssp16.o ../../pico/carthw/svp/gen_arm.c
pico/pico.o pico/cd/pico.o : ../../pico/pico_cmn.c ../../pico/pico_int.h
pico/memory.o pico/cd/memory.o : ../../pico/pico_int.h ../../pico/memory.h

# build Cyclone
../../cpu/Cyclone/proj/Cyclone.s:
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj CONFIG_FILE=config_pico.h

../../cpu/musashi/m68kops.c :
	@make -C ../../cpu/musashi

# build helix libs
../common/helix/$(CROSS)helix-mp3.a:
	make -C ../common/helix clean all


