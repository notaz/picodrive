#######################################################################
### App specific part - this must be defined
#NAME="AnimatedStereogram"
#VENDOR="Pal Szasz"
#UID2=100039CE
#UID3=E0004201
#EPOCLIBS="euser.lib apparc.lib cone.lib eikcore.lib eikcoctl.lib qikcore.lib fbscli.lib estlib.lib"
#EPOCROOT=/opt/space/uiq3/
#######################################################################

APPNAME ?= "UIQ3 Program"
VENDOR ?= "Somebody"
UID2 ?= 100039CE
UID3 ?= E0001001
VER_MAJ ?= 1
VER_MIN ?= 0
STACK ?= 0x1000
HEAP ?= 0x1000,0x100000
EPOCROOT ?= /opt/space/uiq3/
GCCPREF ?= arm-none-symbianelf
GCCPATH ?= $(EPOCROOT)/gcc
GCCVER ?= 3.4.3

export EPOCROOT

NAME_ := $(shell echo $(APPNAME) | sed 's: ::g')
NAME := $(shell perl -e "print lc(\"$(NAME_)\")")
EPOCLIBS += euser.lib apparc.lib cone.lib eikcore.lib eikcoctl.lib qikcore.lib fbscli.lib estlib.lib qikallocdll.lib
EPOCREL = $(EPOCROOT)/epoc32/release/armv5
CC = $(GCCPREF)-gcc
CXX = $(GCCPREF)-g++
AS = $(GCCPREF)-as
LD = $(GCCPREF)-ld
ELF2E32 = elf2e32
BMCONV = bmconv
EPOCRC = EPOCROOT=$(EPOCROOT) epocrc

PATH := $(EPOCROOT)/bin:$(GCCPATH)/bin:$(PATH)

# -march=armv5t ?
CFLAGS += -Wall -pipe -nostdinc -msoft-float
CFLAGS += -DNDEBUG -D_UNICODE -D__GCCE__  -D__SYMBIAN32__ -D__EPOC32__ -D__MARM__
CFLAGS += -D__EABI__ -D__MARM_ARMV5__ -D__EXE__ -D__SUPPORT_CPP_EXCEPTIONS__ -D__MARM_ARMV5__
CFLAGS += -D__PRODUCT_INCLUDE__="$(EPOCROOT)/epoc32/include/variant/uiq_3.0.hrh"
CFLAGS += -include $(EPOCROOT)/epoc32/include/gcce/gcce.h
CFLAGS += -I$(EPOCROOT)/epoc32/include -I$(EPOCROOT)/epoc32/include/libc \
		-I$(EPOCROOT)/epoc32/include/variant -I$(GCCPATH)/lib/gcc/arm-none-symbianelf/$(GCCVER)/include/
# can't optimize .cpp without -fno-unit-at-a-time
CXXFLAGS += $(CFLAGS) -c -x c++ -mapcs -Wno-ctor-dtor-privacy -Wno-unknown-pragmas -fexceptions -fno-unit-at-a-time

LDFLAGS +=  -L$(GCCPATH)/lib -L$(GCCPATH)/lib/gcc/$(GCCPREF)/$(GCCVER) -L $(GCCPATH)/$(GCCPREF)/lib
LDFLAGS +=  --target1-abs --no-undefined -nostdlib  -shared
LDFLAGS +=  -Ttext 0x8000   -Tdata 0x400000 --default-symver
LDFLAGS +=  -soname $(NAME){000a0000}\[$(UID3)\].exe --entry _E32Startup -u _E32Startup
LDFLAGS +=  $(EPOCROOT)/epoc32/release/armv5/urel/eexe.lib
LDFLAGS +=  -o $(NAME).elf.exe -Map $(NAME).exe.map
LDFLAGS2 =  $(EPOCREL)/urel/qikalloc.lib $(EPOCREL)/lib/euser.dso
LDFLAGS2 += $(shell for i in $(EPOCLIBS); do echo -n " $(EPOCREL)/lib/$${i%%.lib}.dso "; done)
LDFLAGS2 += $(EPOCREL)/urel/usrt2_2.lib
LDFLAGS2 += $(shell for i in dfpaeabi dfprvct2_2 drtaeabi scppnwdl drtrvct2_2; do echo -n "  $(EPOCREL)/lib/$$i.dso "; done)
LDFLAGS2 += -lsupc++ -lgcc

E32FLAGS += --sid=0x$(UID3) --uid1=0x1000007a --uid2=0x$(UID2) --uid3=0x$(UID3)
E32FLAGS += --capability=none --fpu=softvfp --targettype=EXE
E32FLAGS += --output=$(NAME).exe --elfinput=$(NAME).elf.exe
E32FLAGS += --stack=$(STACK)
E32FLAGS += --heap=$(HEAP)
E32FLAGS += --linkas=$(NAME){000a0000}[$(UID3)].exe --libpath=$(EPOCREL)/lib

EPOCRCFLAGS += -I../inc -I- -I$(EPOCROOT)/epoc32/include -I$(EPOCROOT)/epoc32/include/variant -DLANGUAGE_SC

ICONS ?= $(shell echo ../data/appicon/*.bmp)
APPICON ?= $(NAME)appicon.mbm
RSCDIR ?= ../rsc
REGDIR ?= ../reg

SRCH += $(shell echo ../inc/*.h)
SRC += $(shell echo ../src/*.cpp)
SRCRES ?= $(shell echo $(RSCDIR)/*.rss $(RSCDIR)/*.rls $(REGDIR)/*.rss $(REGDIR)/*.rls)
OBJ ?= $(SRC:.cpp=.o)

.PHONY : all mbm icon_mbm rsc reg loc bin sis run

#all : sis

sis : $(NAME).sis

icon_mbm : $(APPICON)

mbm :

$(NAME)appicon.mbg $(NAME)appicon.mbm : $(ICONS)
	@echo "Creating multibitmap file..."
	$(BMCONV) /h$(NAME)appicon.mbg $(NAME)appicon.mbm \
		/c24../data/appicon/icon_small.bmp\
		/8../data/appicon/icon_small_mask.bmp\
		/c24../data/appicon/icon_large.bmp\
		/8../data/appicon/icon_large_mask.bmp\
		/c24../data/appicon/icon_xlarge.bmp\
		/8../data/appicon/icon_xlarge_mask.bmp

rsc : $(RSCDIR)/$(NAME).rsc

$(RSCDIR)/$(NAME).rsc : $(RSCDIR)/$(NAME).rss # $(RSCDIR)/$(NAME).rls
	@echo "Creating $@ ..."
	$(EPOCRC) $(EPOCRCFLAGS) -I$(RSCDIR) -u $(RSCDIR)/$(NAME).rss \
		-o$(RSCDIR)/$(NAME).rsc -h$(RSCDIR)/$(NAME).rsg -t/tmp -l$(RSCDIR)

reg : $(REGDIR)/$(NAME)_reg.rsc

$(REGDIR)/$(NAME)_reg.rsc : $(REGDIR)/$(NAME)_reg.rss
	@echo "Creating $@ ..."
	RC_UID2=0x101f8021 RC_UID3=0x$(UID3) $(EPOCRC) $(EPOCRCFLAGS) -I$(REGDIR) \
		-u $(REGDIR)/$(NAME)_reg.rss -o$(REGDIR)/$(NAME)_reg.rsc -h$(REGDIR)/$(NAME)_reg.rsg -t/tmp  -l$(REGDIR)

loc : $(REGDIR)/$(NAME)_loc.rsc

$(REGDIR)/$(NAME)_loc.rsc : $(REGDIR)/$(NAME)_loc.rss
	@echo "Creating $@ ..."
	$(EPOCRC) $(EPOCRCFLAGS) -I$(REGDIR) \
		-u $(REGDIR)/$(NAME)_loc.rss -o$(REGDIR)/$(NAME)_loc.rsc -h$(REGDIR)/$(NAME)_loc.rsg -t/tmp  -l$(REGDIR)

bin : bin_elf
	@echo "Elf -> E32"
	$(ELF2E32) $(E32FLAGS)

bin_elf : $(NAME).elf.exe

$(NAME).elf.exe : $(OBJ) $(EXTRALIB)
	@echo "Linking..."
	$(LD) $(LDFLAGS) $(OBJ) $(EXTRALIB) $(LDFLAGS2)

.cpp.o :
	@echo "Compiling $< ..."
	$(CXX) $(CXXFLAGS) -o $@ $<

$(NAME).sis : icon_mbm mbm rsc reg loc bin
	rm -f $(NAME).sis
	makesis $(NAME).pkg
	mv $(NAME).SIS $(NAME).sis

#cat $(EPOCROOT)/extra/in.pkg extra.pkg | \
#		sed "s:APPNAME:$(APPNAME):g" | \
#		sed "s:NAME:$(NAME):g" | \
#		sed "s:VER_MAJ:$(VER_MAJ):g" | \
#		sed "s:VER_MIN:$(VER_MIN):g" | \
#		sed "s:UID3:$(UID3):g" | \
#		sed "s:VENDOR:$(VENDOR):g" > $(NAME).pkg

run : sis
	xterm -e "to-phone m600 $(NAME).sis"

-include .deps

.deps : $(SRC) $(SRCH)
	echo > $@
	$(CXX) -M -DDEPS $(SRC) $(CXXFLAGS) >> $@

clean :
	rm -f $(NAME).exe $(NAME).elf.exe $(OBJ) tags .deps $(NAME).exe.map
	rm -f *.bkp ../src/*.bkp ../inc/*.bkp $(RSCDIR)/*.bkp $(REGDIR)/*.bkp
	rm -f $(RSCDIR)/*.rsc $(RSCDIR)/*.rsg
	rm -f $(REGDIR)/*.rsc $(REGDIR)/*.rsg
	rm -f $(APPICON) $(NAME)appicon.mbg $(NAME).mbg


