
# you may or may not need to change this
#devkit_path = x:/stuff/dev/devkitgp2x/
devkit_path = /usr/local/devkitPro/devkitGP2X/
lgcc_path = $(devkit_path)lib/gcc/arm-linux/4.0.3/
CROSS = arm-linux-
#CROSS = $(devkit_path)bin/arm-linux-

# settings
#up = 1

DEFINC = -I../.. -I. -D__GP2X__ -DARM # -DBENCHMARK
COPT_COMMON = -static -s -O3 -ftracer -fstrength-reduce -Wall -funroll-loops -fomit-frame-pointer -fstrict-aliasing -ffast-math
COPT = $(COPT_COMMON) -mtune=arm940t
GCC = $(CROSS)gcc
STRIP = $(CROSS)strip
AS = $(CROSS)as
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

all: code940.bin


.c.o:
	@echo $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@
.s.o:
	@echo $<
	$(GCC) $(COPT) $(DEFINC) -c $< -o $@


# stuff for 940 core

# init, emu_control, emu
OBJS940 += 940init.o 940.o 940ym2612.o memcpy.o mix.o misc.o
# the asm code seems to be faster when run on 920, but not on 940 for some reason
# OBJS940 += ../../Pico/sound/ym2612_asm.o

# uClibc library code
OBJS940 += uClibc/memset.o uClibc/s_floor.o uClibc/e_pow.o uClibc/e_sqrt.o uClibc/s_fabs.o
OBJS940 += uClibc/s_scalbn.o uClibc/s_copysign.o uClibc/k_sin.o uClibc/k_cos.o uClibc/s_sin.o
OBJS940 += uClibc/e_rem_pio2.o uClibc/k_rem_pio2.o uClibc/e_log.o uClibc/wrappers.o

code940.bin : code940.gpe
	@echo $@
	@$(OBJCOPY) -O binary $< $@

code940.gpe : $(OBJS940) ../helix/helix_mp3.a
	@echo $@
	@$(LD) -static -e code940 -Ttext 0x0 $^ -L$(lgcc_path) -lgcc -o $@ -Map code940.map

940ym2612.o : ../../../Pico/sound/ym2612.c
	@echo $@
	@$(GCC) $(COPT) $(DEFINC) -DEXTERNAL_YM2612 -c $< -o $@

mix.o : ../../../Pico/sound/mix.s
	@echo $@
	@$(GCC) $(COPT) $(DEFINC) -DEXTERNAL_YM2612 -c $< -o $@
misc.o : ../../../Pico/misc.s
	@echo $@
	@$(GCC) $(COPT) $(DEFINC) -DEXTERNAL_YM2612 -c $< -o $@

../helix/helix_mp3.a:
	@make -C ../helix/


up: code940.bin
	@cp -v code940.bin /mnt/gp2x/mnt/sd/emus/PicoDrive/


# cleanup
clean: tidy
	@$(RM) code940.bin
tidy:
	@$(RM) code940.gpe $(OBJS940) code940.map


OBJSMP3T = mp3test.o ../gp2x.o ../asmutils.o ../usbjoy.o

mp3test.gpe : $(OBJSMP3T) ../helix/helix_mp3.a
	$(GCC) -static -o $@ $^
	$(STRIP) $@
	@cp -v $@ /mnt/gp2x/mnt/sd

cleanmp3test:
	$(RM) $(OBJSMP3T) mp3test.gpe

# uClibc/e_pow.o : uClibc/e_pow.c
#	@echo $<
#	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@

# uClibc/e_sqrt.o : uClibc/e_sqrt.c
#	@echo $<
#	@$(GCC) $(COPT) $(DEFINC) -fno-profile-generate -c $< -o $@
