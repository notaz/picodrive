
# pspdev is expected to be in path
PSPSDK = $(shell psp-config --pspsdk-path)

# settings
#use_musashi = 1
#use_mz80 = 1
amalgamate = 0
#profile = 1
#up = 1


CFLAGS += -I../.. -I. -D_UNZIP_SUPPORT -DNO_SYNC -D_ASM_DRAW_C_MIPS # -DBENCHMARK
CFLAGS += -Wall -Winline -G0
ifeq ($(DEBUG),)
CFLAGS += -O2 -ftracer -fstrength-reduce -ffast-math
else
CFLAGS += -ggdb
endif
ifeq "$(profile)" "1"
CFLAGS += -fprofile-generate
endif
ifeq "$(profile)" "2"
CFLAGS += -fprofile-use
endif


# frontend
OBJS += main.o emu.o mp3.o menu.o psp.o

# common
OBJS += ../common/emu.o ../common/menu.o ../common/fonts.o ../common/readpng.o

# Pico
ifeq "$(amalgamate)" "1"
OBJS += ../../PicoAll.o
else
OBJS += ../../Pico/Area.o ../../Pico/Cart.o ../../Pico/Memory.o ../../Pico/Misc.o \
		../../Pico/Pico.o ../../Pico/Sek.o ../../Pico/VideoPort.o ../../Pico/Draw2.o ../../Pico/Draw.o \
		../../Pico/Patch.o ../../Pico/Draw_amips.o
# Pico - CD
OBJS += ../../Pico/cd/Pico.o ../../Pico/cd/Memory.o ../../Pico/cd/Sek.o ../../Pico/cd/LC89510.o \
		../../Pico/cd/cd_sys.o ../../Pico/cd/cd_file.o ../../Pico/cd/gfx_cd.o \
		../../Pico/cd/Area.o ../../Pico/cd/Misc.o ../../Pico/cd/pcm.o ../../Pico/cd/buffering.o
endif

# Pico - sound
ifneq "$(amalgamate)" "1"
OBJS += ../../Pico/sound/sound.o
endif
OBJS += ../../Pico/sound/mix.o
OBJS += ../../Pico/sound/sn76496.o ../../Pico/sound/ym2612.o
# zlib (hacked)
OBJS += ../../zlib/gzio.o ../../zlib/inffast.o ../../zlib/inflate.o ../../zlib/inftrees.o ../../zlib/trees.o \
	../../zlib/deflate.o ../../zlib/crc32.o ../../zlib/adler32.o ../../zlib/zutil.o ../../zlib/compress.o \
	../../zlib/uncompr.o
# unzip
OBJS += ../../unzip/unzip.o ../../unzip/unzip_stream.o
# CPU cores
ifeq "$(use_musashi)" "1"
CFLAGS += -DEMU_M68K
OBJS += ../../cpu/musashi/m68kops.o ../../cpu/musashi/m68kcpu.o
else
CFLAGS += -DEMU_F68K
OBJS += ../../cpu/fame/famec.o
endif
# z80
ifeq "$(use_mz80)" "1"
CFLAGS += -D_USE_MZ80
OBJS += ../../cpu/mz80/mz80.o
else
CFLAGS += -D_USE_CZ80
OBJS += ../../cpu/cz80/cz80.o
endif
# bg images
OBJS += data/bg32.o data/bg40.o


LIBS += -lpng -lm -lpspgu -lpsppower -Wl,-Map=PicoDrive.map -lpspaudio

# target
TARGET = PicoDrive
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PICODRIVE
#PSP_EBOOT_ICON = .png
#PSP_EBOOT_PIC1 = .png

CUSTOM_CLEAN = myclean

include $(PSPSDK)/lib/build.mak

# some additional rules
.c.o:
	@echo ">>>" $<
	$(CC) $(CFLAGS) -c $< -o $@

AS := psp-as

.s.o:
	@echo ">>>" $<
	$(AS) -march=allegrex -mtune=allegrex $< -o $@

../../cpu/musashi/m68kops.c :
	make -C ../../cpu/musashi

readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ PSP

../../tools/textfilter: ../../tools/textfilter.c
	make -C ../../tools/ textfilter

../../cpu/fame/famec.o : ../../cpu/fame/famec.c
	@echo ">>>" $<
	$(CC) $(CFLAGS) -Wno-unused -c $< -o $@

data/bg32.o: data/bg32.bin
	bin2o -i $< $@ bgdatac32

data/bg40.o: data/bg40.bin
	bin2o -i $< $@ bgdatac40

#
up: EBOOT.PBP
	@cp -v $^ /media/disk/PSP/GAME/PicoDrive/


# cleanup

myclean:
	make -C ../../cpu/musashi clean


clean_prof:
	find ../.. -name '*.gcno' -delete
	find ../.. -name '*.gcda' -delete

# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif

# ?
rel: EBOOT.PBP readme.txt
	zip -9 -j ../../PicoDrive_$(VER).zip $^
#	zip -9 -r ../../PicoDrive_$(VER).zip skin -i \*.png -i \*.txt

