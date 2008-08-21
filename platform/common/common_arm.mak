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
Pico/carthw/svp/compiler.o : ../../Pico/carthw/svp/ssp16.o ../../Pico/carthw/svp/gen_arm.c
Pico/Pico.o Pico/cd/Pico.o : ../../Pico/PicoFrameHints.c ../../Pico/PicoInt.h
Pico/Memory.o Pico/cd/Memory.o : ../../Pico/MemoryCmn.c ../../Pico/PicoInt.h

# individual rules
Pico/draw_asm.o : ../../Pico/Draw.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/draw2_asm.o : ../../Pico/Draw2.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/memory_asm.o : ../../Pico/Memory.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/sound/ym2612_asm.o : ../../Pico/sound/ym2612.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/sound/mix_asm.o : ../../Pico/sound/mix.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/misc_asm.o : ../../Pico/Misc.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/cd/pico_asm.o : ../../Pico/cd/Pico.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/cd/memory_asm.o : ../../Pico/cd/Memory.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
Pico/cd/misc_asm.o : ../../Pico/cd/Misc.s
	@echo ">>>" $@
	$(AS) $(ASFLAGS) $< -o $@
#Pico/carthw/svp/stub_arm.o : ../../Pico/carthw/svp/stub_arm.S
#	@echo ">>>" $@
#	$(GCC) $(CFLAGS) $(DEFINC) -c $< -o $@

# build Cyclone
cpu/Cyclone/proj/Cyclone.s:
	@echo building Cyclone...
	@make -C ../../cpu/Cyclone/proj CONFIG_FILE=config_pico.h

# build helix libs
../common/helix/$(CROSS)helix-mp3.a:
	make -C ../common/helix clean all


