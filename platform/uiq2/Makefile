# environmental vars required:
# EPOCROOT2 - root of your SDK with slash at the end

# settings
#dprint = 1
#mz80 = 1
#debug_cyclone = 1
asm_memory = 1
#asm_render = 1
#use_musashi = 1

EPOCBLDUREL = _BUILD
EPOCTRGUREL = $(EPOCROOT2)EPOC32\RELEASE\ARMI\UREL
EPOCLINKUREL = $(EPOCTRGUREL)
EPOCSTATLINKUREL = $(EPOCTRGUREL)


# must set both PATH and Path to make it work correctly (?)
Path:=$(EPOCROOT2)EPOC32\gcc\bin;$(Path)
PATH:=$(Path)
ERASE = @erase 2>>nul


ALL : UREL
UREL : CHECKENV MAKEDIRS
UREL : $(EPOCTRGUREL)\PICOSMALL.EXE

CLEAN :
	@perl -S ermdir.pl "$(EPOCBLDUREL)"
	@del *.o 2> NUL

CLEANZ :
	del ..\..\zlib\*.o 2> NUL

CLEANM :
	del ..\..\musashi\*.o 2> NUL

CLEANALL : CLEAN CLEANZ CLEANM


# check for EPOCROOT2
CHECKENV : $(EPOCROOT2)epoc32

$(EPOCROOT2)epoc32 :
	@echo Please set EPOCROOT2 environmental variable to full path to your SDK
	@echo with ending slash (something like C:\Uiq_21\)
	@cd : 2> NUL # do something stupid to make it silently fail


# GCC
INCDIR  = -I. -I "..\..\" -I "$(EPOCROOT2)EPOC32\INCLUDE" -I "$(EPOCROOT2)EPOC32\INCLUDE\LIBC"

GCCFLAGS = -nostdinc -Wall -Wno-ctor-dtor-privacy -Wno-unknown-pragmas -march=armv4t -mthumb-interwork -pipe -O3 -fomit-frame-pointer

GCCDEFS = -D__SYMBIAN32__ -D__GCC32__ -D__EPOC32__ -D__MARM__ -D__MARM_ARMI__ -D__EXE__ -DNDEBUG -D_UNICODE -DARM

# dprint
ifeq "$(dprint)" "1"
GCCDEFS += -D__DEBUG_PRINT
endif
# drz80/mz80
ifeq "$(mz80)" "1"
GCCDEFS += -D_USE_MZ80
else
GCCDEFS += -D_USE_DRZ80
endif
# debug_cyclone
ifeq "$(debug_cyclone)" "1"
use_musashi := 1
GCCDEFS += -DEMU_C68K
asm_memory := 0
endif
# musashi
ifeq "$(use_musashi)" "1"
GCCDEFS += -DEMU_M68K
else
GCCDEFS += -DEMU_C68K
endif


GCC = gcc -c $(GCCFLAGS) $(GCCDEFS) $(INCDIR)


LIBSUREL = \
	$(EPOCSTATLINKUREL)\EGCC.LIB \
	$(EPOCLINKUREL)\ESTLIB.LIB \
	$(EPOCLINKUREL)\WS32.LIB \
	$(EPOCLINKUREL)\HAL.LIB \
	$(EPOCLINKUREL)\EUSER.LIB \
	$(EPOCLINKUREL)\EFSRV.LIB \
	$(EPOCLINKUREL)\EZLIB.LIB

$(EPOCTRGUREL)\PICOSMALL.EXE : $(EPOCBLDUREL)\PICOSMALL.in $(EPOCSTATLINKUREL)\EEXE.LIB $(LIBSUREL)
	@echo * linking and finishing
	@ld  -s -e _E32Startup -u _E32Startup \
		--base-file "$(EPOCBLDUREL)\PICOSMALL.bas" -o "$(EPOCBLDUREL)\PICOSMALL.EXE" \
		"$(EPOCSTATLINKUREL)\EEXE.LIB" --whole-archive "$(EPOCBLDUREL)\PICOSMALL.in" \
		--no-whole-archive $(LIBSUREL) $(USERLDFLAGS)
	-$(ERASE) "$(EPOCBLDUREL)\PICOSMALL.EXE"
	@dlltool -m arm_interwork \
		--base-file "$(EPOCBLDUREL)\PICOSMALL.bas" \
		--output-exp "$(EPOCBLDUREL)\PICOSMALL.exp" 
	-$(ERASE) "$(EPOCBLDUREL)\PICOSMALL.bas"
	@ld  -s -e _E32Startup -u _E32Startup \
		"$(EPOCBLDUREL)\PICOSMALL.exp" \
		-Map "$(EPOCBLDUREL)\PICOSMALL.EXE.map" -o "$(EPOCBLDUREL)\PICOSMALL.EXE" \
		"$(EPOCSTATLINKUREL)\EEXE.LIB" --whole-archive "$(EPOCBLDUREL)\PICOSMALL.in" \
		--no-whole-archive $(LIBSUREL) $(USERLDFLAGS)
	-$(ERASE) "$(EPOCBLDUREL)\PICOSMALL.exp"
ifeq "$(noecompxl)" "1"
	@petran  "$(EPOCBLDUREL)\PICOSMALL.EXE" "$@" \
		 -nocall -uid1 0x1000007a -uid2 0x00000000 -uid3 0x00000000 \
		 -heap 0x00000001 0x00800000
else
	@petran_ "$(EPOCBLDUREL)\PICOSMALL.EXE" "$@" \
		 -nocall -uid1 0x1000007a -uid2 0x00000000 -uid3 0x00000000 \
		 -heap 0x00000001 0x00800000 -stack 0x80000000
endif
#	-$(ERASE) "$(EPOCBLDUREL)\PICOSMALL.EXE"
	@perl -S ecopyfile.pl "$@" "PICOSMALL.EXE"
ifeq "$(up)" "1"
	@quploadpico.cmd
endif


OBJECTSUREL= \
	$(EPOCBLDUREL)\debug.o \
	$(EPOCBLDUREL)\CART.o \
	$(EPOCBLDUREL)\DRAW.o \
	$(EPOCBLDUREL)\DRAW2.o \
	$(EPOCBLDUREL)\MAIN.o \
	$(EPOCBLDUREL)\MEMORY.o \
	$(EPOCBLDUREL)\PICO.o \
	$(EPOCBLDUREL)\SEK.o \
	$(EPOCBLDUREL)\VIDEOPORT.o \
	$(EPOCBLDUREL)\SIMPLESERVER.o \
	$(EPOCBLDUREL)\VID.o \
	$(EPOCBLDUREL)\Utils.o \
	$(EPOCBLDUREL)\Area.o \
	$(EPOCBLDUREL)\Misc.o \
	$(EPOCBLDUREL)\unzip.o \
	..\..\zlib\gzio_symb.o \
	$(EPOCBLDUREL)\sound.o \
	$(EPOCBLDUREL)\sn76496.o \
	$(EPOCBLDUREL)\ym2612.o \
	$(EPOCBLDUREL)\blit.o

ifeq "$(debug_cyclone)" "1"
OBJECTSUREL += $(EPOCBLDUREL)\Cyclone.o $(EPOCBLDUREL)\_cyclone_debug.o
endif
# the MUSASHI core
ifeq "$(use_musashi)" "1"
OBJECTSUREL += \
	..\..\musashi\m68kcpu.o \
	..\..\musashi\m68kops.o \
	..\..\musashi\m68kopac.o \
	..\..\musashi\m68kopdm.o \
	..\..\musashi\m68kopnz.o \
	..\..\musashi\m68kdasm.o
else
OBJECTSUREL += $(EPOCBLDUREL)\Cyclone.o
endif

ifeq "$(mz80)" "1"
OBJECTSUREL := $(OBJECTSUREL) $(EPOCBLDUREL)\mz80.o
else
OBJECTSUREL := $(OBJECTSUREL) $(EPOCBLDUREL)\DrZ80.o
endif

ifeq "$(asm_memory)" "1"
ASMDEFINES += -D_ASM_MEMORY_C
OBJECTSUREL := $(OBJECTSUREL) $(EPOCBLDUREL)\memory_asm.o
endif

ifeq "$(asm_render)" "1"
ASMDEFINES += -D_ASM_DRAW_C
OBJECTSUREL := $(OBJECTSUREL) $(EPOCBLDUREL)\draw_asm.o $(EPOCBLDUREL)\draw2_asm.o
endif

$(EPOCBLDUREL)\PICOSMALL.in : $(OBJECTSUREL)
	@echo * ar
	@if exist "$@" del "$@"
	@ar cr $@ $^

..\..\Cyclone\proj\Cyclone.s :
	@echo You need to build Cyclone first. See Cyclone.txt in cyclone directory.
	@cd : 2> NUL # do something stupid to make it silently fail

$(EPOCBLDUREL)\Cyclone.o : ..\..\Cyclone\proj\Cyclone.s
	@echo * assembling Cyclone..
	@as -marmv4t -mthumb-interwork -o $@ $^

$(EPOCBLDUREL)\DrZ80.o : ..\..\pico\sound\DrZ80.s ..\..\pico\sound\DrZ80.h
	@echo * assembling DrZ80..
	@as -marmv4t -mthumb-interwork -o $@ ..\..\pico\sound\DrZ80.s


# Source

$(EPOCBLDUREL)\debug.o : debug.cpp debug.h
	@echo * debug.cpp
	@$(GCC) -o $@ debug.cpp

$(EPOCBLDUREL)\MAIN.o : Main.cpp ..\..\pico\picoInt.h vid.h SimpleServer.h ClientServer.h audio.h debug.h
	@echo * Main.cpp
	@$(GCC) -o $@ "Main.cpp"

$(EPOCBLDUREL)\VID.o : VID.cpp vid.h ClientServer.h ..\..\pico\picoInt.h
	@echo * Vid.cpp
	@$(GCC) -o $@ "VID.cpp"

$(EPOCBLDUREL)\SIMPLESERVER.o : SIMPLESERVER.cpp version.h ClientServer.h SimpleServer.h ..\..\pico\picoInt.h debug.h
	@echo * SimpleServer.cpp
	@$(GCC) -o $@ "SIMPLESERVER.cpp"

$(EPOCBLDUREL)\CART.o : ..\..\pico\Cart.c ..\..\pico\picoInt.h ..\..\pico\Pico.h ..\..\unzip\unzip.h
	@echo * Cart.c
	@$(GCC) -D_UNZIP_SUPPORT -o $@ "..\..\pico\Cart.c"

$(EPOCBLDUREL)\DRAW.o : ..\..\pico\Draw.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Draw.c
	@$(GCC) $(ASMDEFINES) -o $@ "..\..\pico\Draw.c"
#

$(EPOCBLDUREL)\DRAW2.o : ..\..\pico\Draw2.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Draw2.c
	@$(GCC) $(ASMDEFINES) -o $@ "..\..\pico\Draw2.c"

$(EPOCBLDUREL)\MEMORY.o : ..\..\pico\Memory.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Memory.c
	@$(GCC) $(ASMDEFINES) -o $@ "..\..\pico\Memory.c"

$(EPOCBLDUREL)\PICO.o : ..\..\pico\Pico.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Pico.c
	@$(GCC) -o $@ "..\..\pico\Pico.c"

$(EPOCBLDUREL)\SEK.o : ..\..\pico\Sek.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Sek.c
	@$(GCC) -o $@ "..\..\pico\Sek.c"

$(EPOCBLDUREL)\VIDEOPORT.o : ..\..\pico\Videoport.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Videoport.c
	@$(GCC) -o $@ "..\..\pico\Videoport.c"

$(EPOCBLDUREL)\Utils.o : ..\..\pico\Utils.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Utils.c
	@$(GCC) -o $@ "..\..\pico\Utils.c"

$(EPOCBLDUREL)\Area.o : ..\..\pico\Area.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Area.c
	@$(GCC) -o $@ "..\..\pico\Area.c"

$(EPOCBLDUREL)\Misc.o : ..\..\pico\Misc.c ..\..\pico\picoInt.h ..\..\pico\Pico.h
	@echo * Misc.c
	@$(GCC) -o $@ "..\..\pico\Misc.c"

$(EPOCBLDUREL)\unzip.o : ..\..\unzip\unzip.c ..\..\unzip\unzip.h
	@echo * unzip.c
	@$(GCC) -o $@ "..\..\unzip\unzip.c"

# assembly "optimized" stuff
$(EPOCBLDUREL)\blit.o : blit.s
	@echo * blit.s
	@as -marmv4t -mthumb-interwork -o $@ blit.s

$(EPOCBLDUREL)\draw_asm.o : ..\..\pico\draw.s
	@echo * draw.s
	@as -marmv4t -mthumb-interwork -o $@ ..\..\pico\draw.s

$(EPOCBLDUREL)\draw2_asm.o : ..\..\pico\draw2.s
	@echo * draw2.s
	@as -marmv4t -mthumb-interwork -o $@ ..\..\pico\draw2.s

$(EPOCBLDUREL)\memory_asm.o : ..\..\pico\memory.s
	@echo * memory.s
	@as -marmv4t -mthumb-interwork -o $@ ..\..\pico\memory.s

# sound stuff
$(EPOCBLDUREL)\sound.o : ..\..\pico\sound\sound.c ..\..\pico\sound\sound.h
	@echo * sound.c
	@$(GCC) -o $@ "..\..\pico\sound\sound.c"

$(EPOCBLDUREL)\ym2612.o : ..\..\pico\sound\ym2612.c ..\..\pico\sound\ym2612.h ..\..\pico\sound\driver.h
	@echo * ym2612.c
	@$(GCC) -o $@ "..\..\pico\sound\ym2612.c"

$(EPOCBLDUREL)\sn76496.o : ..\..\pico\sound\sn76496.c ..\..\pico\sound\sn76496.h ..\..\pico\sound\driver.h
	@echo * sn76496.c
	@$(GCC) -o $@ "..\..\pico\sound\sn76496.c"

$(EPOCBLDUREL)\mz80.o : ..\..\pico\sound\mz80.c ..\..\pico\sound\mz80.h
	@echo * mz80.c
	@$(GCC) -o $@ "..\..\pico\sound\mz80.c"

# -D__DEBUG_PRINT_SND

# misc

$(EPOCBLDUREL)\_cyclone_debug.o : ..\..\pico\_cyclone_debug.c
	@echo * _cyclone_debug.c
	@$(GCC) -o $@ "..\..\pico\_cyclone_debug.c"

# generic rule for generic C stuff

.c.o:
	@echo * $<
	@$(GCC) $< -o $@



# --- SRCFILES END ---


# Rules to create all necessary directories

MAKEDIRS : \
	_build

_build :
	@echo * making build dir
	@perl -S emkdir.pl $@
