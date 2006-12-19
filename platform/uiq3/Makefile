# makefile for GCCE

# settings
#dprint = 1
asm_memory = 1
asm_render = 1
asm_blit = 1
#use_musashi = 1
#up = 1
#sis = 1

# targets
all: $(EPOCROOT2)epoc32 MAKEDIRS RESOURCES PicoDrive.exe

clean :
	@perl -S ermdir.pl _build
	@erase 2>>nul rsc\*.rsc
	@erase 2>>nul rsc\*.rsg
	@erase 2>>nul rsc\PicoDrive.mb?


# paths
$(EPOCROOT2)epoc32 :
	@echo Please set EPOCROOT2 environmental variable to full path to your SDK
	@echo with ending slash (something like C:\Uiq_21\)
	@cd : 2> NUL # do something stupid to make it silently fail

# resource compiler hates drive lettered paths
EPOCROOT2_NODRV = $(filter \\%,$(subst :, ,$(EPOCROOT2)))
EPOCLIB = $(EPOCROOT2)EPOC32\RELEASE\ARMV5

# C/C++ Compiler
CC=arm-none-symbianelf-gcc

# Linker
LD=arm-none-symbianelf-ld

# Assembler
ASM=arm-none-symbianelf-as

# Archiver
AR=arm-none-symbianelf-ar

# Strip
STRIP=arm-none-symbianelf-strip

# gcc config
GCCDEFINES = -DNDEBUG -D_UNICODE -D__GCCE__ -D__SYMBIAN32__ -D__EPOC32__ -D__MARM__ \
		-D__EABI__ -D__MARM_ARMV5__ -D__EXE__ -D__SUPPORT_CPP_EXCEPTIONS__ \
		-D__MARM_ARMV5__ -D__PRODUCT_INCLUDE__=\"$(EPOCROOT2)epoc32/include/variant/UIQ_3.0.hrh\"

GCCDEFINES += -D_UNZIP_SUPPORT -D__BROKEN_FWRITE

# 'CSL Arm Toolchain' stuff must be specified after Symbian includes
GCCINCLUDES = -I "$(EPOCROOT2)epoc32\include\variant" -I "$(EPOCROOT2)EPOC32\INCLUDE" -I "$(EPOCROOT2)EPOC32\INCLUDE\LIBC" \
		-I "$(EPOCROOT2)\CSL Arm Toolchain\lib\gcc\arm-none-symbianelf\3.4.3\include" -I.

# -funit-at-a-time is not compatible with SDK, it either has linker problems or does not start on device
GCCCOMMFLAGS = -Wall -Wno-unknown-pragmas -fexceptions -march=armv5t -mapcs -pipe -nostdinc -msoft-float \
		$(GCCINCLUDES) -include "$(EPOCROOT2)EPOC32/INCLUDE/GCCE/GCCE.h" -marm

GCCCPPFLAGS = -x c++ -Wno-ctor-dtor-privacy -O3 -fno-unit-at-a-time
GCCCFLAGS = -x c -O3 -fno-unit-at-a-time

GCCLDFLAGS = -L"$(EPOCROOT2)CSL Arm Toolchain\arm-none-symbianelf\lib" \
		-L"$(EPOCROOT2)CSL Arm Toolchain\lib\gcc\arm-none-symbianelf\3.4.3" \
		--target1-abs --no-undefined -nostdlib -shared -Ttext 0x8000 -Tdata 0x400000 --default-symver

# libs
LIBS = \
	$(EPOCLIB)\LIB\ESTLIB.dso \
	$(EPOCLIB)\urel\qikalloc.lib \
	$(EPOCLIB)\LIB\euser.dso \
	$(EPOCLIB)\LIB\apparc.dso \
	$(EPOCLIB)\LIB\cone.dso \
	$(EPOCLIB)\LIB\eikcore.dso \
	$(EPOCLIB)\LIB\eikcoctl.dso \
	$(EPOCLIB)\LIB\qikcore.dso \
	$(EPOCLIB)\LIB\qikdlg.dso \
	$(EPOCLIB)\LIB\etext.dso \
	$(EPOCLIB)\LIB\bafl.dso \
	$(EPOCLIB)\LIB\efsrv.dso \
	$(EPOCLIB)\LIB\eikctl.dso \
	$(EPOCLIB)\LIB\WS32.dso \
	$(EPOCLIB)\LIB\EIKDLG.dso \
	$(EPOCLIB)\LIB\GDI.dso \
	$(EPOCLIB)\LIB\estor.dso \
	$(EPOCLIB)\LIB\EZLIB.dso \
	$(EPOCLIB)\LIB\HAL.dso \
	$(EPOCLIB)\LIB\mediaclient.dso \
	$(EPOCLIB)\LIB\mediaclientaudiostream.dso

LIBS += \
	$(EPOCLIB)\LIB\qikallocdll.dso \
	$(EPOCLIB)\UREL\usrt2_2.lib \
	$(EPOCLIB)\LIB\dfpaeabi.dso \
	$(EPOCLIB)\LIB\dfprvct2_2.dso \
	$(EPOCLIB)\LIB\drtaeabi.dso \
	$(EPOCLIB)\LIB\scppnwdl.dso \
	$(EPOCLIB)\LIB\drtrvct2_2.dso


# objects

# launcher
OBJECTS += _build\App.o _build\Engine.o _build\Dialogs.o _build\CSimpleTextParser.o
# engine
OBJECTS += _build\main.o _build\vid.o _build\polledas.o _build\audio_mediaserver.o _build\debug.o

# Pico
OBJECTS += _build\Area.o _build\Cart.o _build\Utils.o _build\Memory.o _build\Misc.o \
		_build\Pico.o _build\Sek.o _build\VideoPort.o _build\Draw2.o _build\Draw.o
# asm stuff
ifeq "$(asm_render)" "1"
GCCDEFINES += -D_ASM_DRAW_C
OBJECTS += _build\draw_asm.o _build\draw2_asm.o
endif
ifeq "$(asm_memory)" "1"
GCCDEFINES += -D_ASM_MEMORY_C
OBJECTS += _build\memory_asm.o
endif
# Pico - sound
OBJECTS += _build\sound.o _build\sn76496.o _build\ym2612.o
# misc
OBJECTS += _build\unzip.o _build\gzio_symb.o
# CPU cores
ifeq "$(use_musashi)" "1"
GCCDEFINES += -DEMU_M68K
OBJECTS += _build\m68kcpu.o _build\m68kopac.o _build\m68kopdm.o _build\m68kopnz.o _build\m68kops.o
else
GCCDEFINES += -DEMU_C68K
OBJECTS += _build\Cyclone.o
endif
ifeq "$(asm_blit)" "1"
OBJECTS += _build\blit_asm.o
else
OBJECTS += _build\blit.o
endif
GCCDEFINES += -D_USE_DRZ80
OBJECTS += _build\DrZ80.o
GCCDEFINES += -D_ASM_YM2612_C
OBJECTS += _build\ym2612_asm.o



# dprint
ifeq "$(dprint)" "1"
GCCDEFINES += -D__DEBUG_PRINT
endif


define crule
	@echo * $<
	@$(CC) -c $(GCCCOMMFLAGS) $(GCCDEFINES) $(GCCCFLAGS) $< -o $@
endef

define cpprule
	@echo * $<
	@$(CC) -c $(GCCCOMMFLAGS) $(GCCDEFINES) $(GCCCPPFLAGS) $< -o $@
endef

define asmrule
	@echo * $<
	@$(ASM) -marmv4t -mthumb-interwork -o $@ $^
endef

# object making rules
_build\App.o     : App.cpp
	$(cpprule)
_build\Engine.o  : Engine.cpp
	$(cpprule)
_build\Dialogs.o : Dialogs.cpp
	$(cpprule)
_build\CSimpleTextParser.o : CSimpleTextParser.cpp
	$(cpprule)

_build\main.o    : engine\main.cpp
	$(cpprule)
_build\vid.o     : engine\vid.cpp
	$(cpprule)
_build\polledas.o: engine\polledas.cpp
	$(cpprule)
_build\audio_mediaserver.o : engine\audio_mediaserver.cpp
	$(cpprule)
_build\debug.o   : engine\debug.cpp
	$(cpprule)
_build\blit.o    : engine\blit.c
	$(crule)

_build\Area.o    : ..\..\Pico\Area.c
	$(crule)
_build\Cart.o    : ..\..\Pico\Cart.c
	$(crule)
_build\Draw.o    : ..\..\Pico\Draw.c
	$(crule)
_build\Draw2.o   : ..\..\Pico\Draw2.c
	$(crule)
_build\Memory.o  : ..\..\Pico\Memory.c
	$(crule)
_build\Misc.o    : ..\..\Pico\Misc.c
	$(crule)
_build\Pico.o    : ..\..\Pico\Pico.c
	$(crule)
_build\Sek.o     : ..\..\Pico\Sek.c
	$(crule)
_build\Utils.o   : ..\..\Pico\Utils.c
	$(crule)
_build\VideoPort.o : ..\..\Pico\VideoPort.c
	$(crule)
_build\sound.o   : ..\..\Pico\sound\sound.c
	$(crule)
_build\sn76496.o : ..\..\Pico\sound\sn76496.c
	$(crule)
_build\ym2612.o  : ..\..\Pico\sound\ym2612.c
	$(crule)

_build\unzip.o     : ..\..\unzip\unzip.c
	$(crule)
_build\gzio_symb.o : ..\..\zlib\gzio_symb.c
	$(crule)

_build\m68kcpu.o  : ..\..\musashi\m68kcpu.c
	$(crule)
_build\m68kopac.o : ..\..\musashi\m68kopac.c
	$(crule)
_build\m68kopdm.o : ..\..\musashi\m68kopdm.c
	$(crule)
_build\m68kopnz.o : ..\..\musashi\m68kopnz.c
	$(crule)
_build\m68kops.o  : ..\..\musashi\m68kops.c
	$(crule)

_build\Cyclone.o : ..\..\cpu\Cyclone\proj\Cyclone.s
	$(asmrule)
_build\DrZ80.o : ..\..\cpu\DrZ80\drz80.s
	$(asmrule)
_build\draw_asm.o : ..\..\Pico\draw.s
	$(asmrule)
_build\draw2_asm.o : ..\..\Pico\draw2.s
	$(asmrule)
_build\memory_asm.o : ..\..\Pico\memory.s
	$(asmrule)
_build\ym2612_asm.o : ..\..\Pico\sound\ym2612.s
	$(asmrule)
_build\blit_asm.o : engine\blit.s
	$(asmrule)


PicoDrive.exe : $(OBJECTS)
	@echo * ld
	@$(LD) $(GCCLDFLAGS) -soname PicoDrive{000a0000}[a00010f3].exe --entry _E32Startup -u _E32Startup \
		$(EPOCROOT2)EPOC32\RELEASE\ARMV5\UREL\EEXE.LIB -o "_build\PicoDrive_elf.exe" -Map "_build\PicoDrive.exe.map" $(OBJECTS) $(LIBS) -lsupc++ -lgcc
#	@echo * strip
#	@$(STRIP) _build\PicoDrive_elf.exe
	@echo * elf2e32
	@elf2e32 --sid=0xa00010f3  --heap=0x00000100,0x00800000 --stack=0x00003000 \
		--uid1=0x1000007a --uid2=0x100039ce --uid3=0xa00010f3 \
		--capability=none --fpu=softvfp --targettype=EXE --output="$@" \
		--elfinput="_build\PicoDrive_elf.exe" --linkas=PicoDrive{000a0000}[a00010f3].exe --libpath="$(EPOCLIB)\LIB"
ifeq "$(sis)" "1"
	@make -C _out
ifeq "$(up)" "1"
	@qup.cmd
endif
endif


MAKEDIRS : _build

_build :
#	@echo * making build dir
	@perl -S emkdir.pl $@


# BitMap PicoDrive.mbm

RESOURCES : rsc\PicoDrive.mbm rsc\PicoDrive.RSC rsc\PicoDrive_reg.RSC rsc\PicoDrive_loc.RSC rsc\PicoDrive.mbg

rsc\PicoDrive.mbg : rsc\PicoDrive.mbm

rsc\PicoDrive.mbm : rsc\pico18x18.bmp rsc\pico18x18m.bmp rsc\pico40x40.bmp rsc\pico40x40m.bmp rsc\pico64x64.bmp rsc\pico64x64m.bmp
	@echo * $@
	@perl -S epocmbm.pl -h"rsc\PicoDrive.mbg" -o"rsc\PicoDrive.mbm" -l"\Z\Resource\Apps\:rsc" \
		-b"/c24rsc\pico18x18.bmp /8rsc\pico18x18m.bmp /c24rsc\pico40x40.bmp /8rsc\pico40x40m.bmp /c24rsc\pico64x64.bmp /8rsc\pico64x64m.bmp" -l"\Z\Resource\Apps\:rsc"
	@perl -S ecopyfile.pl "rsc\PicoDrive.mbg" "$(EPOCROOT2)EPOC32\INCLUDE\PicoDrive.mbg"

# Resource Z\Resource\Apps\PicoDrive.RSC

rsc\PicoDrive.RSC : rsc\PicoDrive.rss picodrive.hrh
	@echo * $@
	@perl -S epocrc.pl -m045,046,047 -I "." -I- -I "$(EPOCROOT2_NODRV)EPOC32\include" -DLANGUAGE_SC -u "rsc\PicoDrive.rss" -o$@ \
		-h"rsc\PicoDrive.rsg" -t"rsc" -l"Z\Resource\Apps:rsc"
	@perl -S ecopyfile.pl "rsc\PicoDrive.rsg" "$(EPOCROOT2)EPOC32\INCLUDE\PicoDrive.RSG"

# Resource Z\private\10003a3f\apps\PicoDrive_reg.RSC

rsc\PicoDrive_reg.RSC : rsc\PicoDrive_reg.rss
	@echo * $@
	@perl -S epocrc.pl -m045,046,047 -I "." -I- -I "$(EPOCROOT2)EPOC32\include" -DLANGUAGE_SC -u "rsc\PicoDrive_reg.rss" -o$@ \
	-t"rsc" -l"Z\private\10003a3f\apps:rsc"

# Resource Z\Resource\Apps\PicoDrive_loc.RSC

rsc\PicoDrive_loc.RSC : rsc\PicoDrive_loc.rss
	@echo * $@
	@perl -S epocrc.pl -m045,046,047 -I "." -I- -I "$(EPOCROOT2)EPOC32\include" -DLANGUAGE_SC -u "rsc\PicoDrive_loc.rss" -o$@ \
		-t"rsc"  -l"Z\Resource\Apps:rsc"
