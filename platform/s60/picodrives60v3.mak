PATH=\S60V3\epoc32\tools\;C:\Program\CSL Arm Toolchain\arm-none-symbianelf\bin;C:\Program\CSL Arm Toolchain\bin;\uiq3\epoc32\tools\;\S60V3\epoc32\tools\;\S60V3\epoc32\gcc\bin\;C:\winnt\system32;C:\winnt;C:\winnt\System32\Wbem;C:\Program Files\ATI Technologies\ATI Control Panel;C:\Program Files\Common Files\Adaptec Shared\System;C:\Perl\bin;\uiq3\epoc32\tools;c:\MSVC6\VC98\Bin;e:\UIQ3\epoc32\tools\nokia_compiler\Symbian_Tools\Command_Line_Tools;C:\Program Files\CSL Arm Toolchain\arm-none-symbianelf\bin;C:\Program Files\CSL Arm Toolchain\bin
Path=$(PATH)
COMPILER_PATH="\S60V3\epoc32\tools\nokia_compiler\Symbian_Tools\Command_Line_Tools\"

# CWD \picodrive\s60\
# MMPFile \picodrive\s60\picodrives60v3.MMP
# Target PicoDrive.exe
# TargetType EXE
# BasicTargetType EXE
# MakefileType GNU

ERASE = @erase 2>>nul

# EPOC DEFINITIONS

EPOCBLD = \S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW
EPOCTRG = \S60V3\EPOC32\RELEASE\WINSCW
EPOCLIB = \S60V3\EPOC32\RELEASE\WINSCW
EPOCLINK = \S60V3\EPOC32\RELEASE\WINSCW
EPOCSTATLINK = \S60V3\EPOC32\RELEASE\WINSCW
EPOCASSPLINK = \S60V3\EPOC32\RELEASE\WINSCW
EPOCDATA = \S60V3\EPOC32\DATA
EPOCINC = \S60V3\EPOC32\INCLUDE
TRGDIR = Z\sys\bin
DATADIR = Z\System\Data

EPOCBLDUDEB = $(EPOCBLD)\UDEB
EPOCTRGUDEB = $(EPOCTRG)\UDEB
EPOCLIBUDEB = $(EPOCLIB)\UDEB
EPOCLINKUDEB = $(EPOCLINK)\UDEB
EPOCSTATLINKUDEB = $(EPOCSTATLINK)\UDEB
EPOCASSPLINKUDEB = $(EPOCASSPLINK)\UDEB

EPOCBLDUREL = $(EPOCBLD)\UREL
EPOCTRGUREL = $(EPOCTRG)\UREL
EPOCLIBUREL = $(EPOCLIB)\UDEB
EPOCLINKUREL = $(EPOCLINK)\UDEB
EPOCSTATLINKUREL = $(EPOCSTATLINK)\UREL
EPOCASSPLINKUREL = $(EPOCASSPLINK)\UDEB

# EPOC PSEUDOTARGETS

UDEB : MAKEWORKUDEB RESOURCEUDEB

UREL : MAKEWORKUREL RESOURCEUREL

ALL : UDEB UREL

CLEAN CLEANALL : CLEANBUILD CLEANRELEASE CLEANLIBRARY



WHAT WHATALL : WHATUDEB WHATUREL

RESOURCE RESOURCEALL : RESOURCEUDEB RESOURCEUREL

CLEANBUILD CLEANBUILDALL : CLEANBUILDUDEB CLEANBUILDUREL

CLEANRELEASE CLEANRELEASEALL : CLEANRELEASEUDEB CLEANRELEASEUREL

MAKEWORK MAKEWORKALL : MAKEWORKUDEB MAKEWORKUREL

LISTING LISTINGALL : LISTINGUDEB LISTINGUREL

MAKEWORK : MAKEWORKLIBRARY

RESOURCEUDEB RESOURCEUREL : GENERIC_RESOURCE


MWCIncludes:=$(MWCSym2Includes)
export MWCIncludes


MWLibraries:=+\S60V3\epoc32\tools\nokia_compiler\Symbian_Support\Runtime\Runtime_x86\Runtime_Win32\Libs;\S60V3\epoc32\tools\nokia_compiler\Symbian_Support\Win32-x86 Support\Libraries\Win32 SDK
export MWLibraries


MWLibraryFiles:=gdi32.lib;user32.lib;kernel32.lib;
export MWLibraryFiles

# EPOC DEFINITIONS

INCDIR  = -cwd source -i- \
 -i "\picodrive\pico" \
 -i "\picodrive\pico\sound" \
 -i "\picodrive\s60" \
 -i "\picodrive" \
 -i "\S60V3\EPOC32\include" \
 -i "\S60V3\EPOC32\include\libc" \
 -i "\S60V3\EPOC32\include\mmf\plugin" \
 -i "\S60V3\epoc32\include\variant"\
 -i "\S60V3\epoc32\include\variant\ " -include "Symbian_OS_v9.1.hrh"

CWFLAGS = -wchar_t off -align 4 -warnings on -w nohidevirtual,nounusedexpr -enum int -str pool -exc ms  -nostdinc

CWDEFS  =  -d "__SYMBIAN32__" -d "__CW32__" -d "__WINS__" -d "__WINSCW__" -d "__EXE__" -d "S60V3" -d "__SUPPORT_CPP_EXCEPTIONS__" $(USERDEFS)

CWUDEB = perl -S err_formatter.pl $(COMPILER_PATH)mwccsym2.exe -msgstyle parseable  -sym codeview -inline off $(CWFLAGS) -d _DEBUG -d _UNICODE $(CWDEFS) $(INCDIR)
CWUREL = perl -S err_formatter.pl $(COMPILER_PATH)mwccsym2.exe -msgstyle parseable  -O4,s $(CWFLAGS) -d NDEBUG -d _UNICODE $(CWDEFS) $(INCDIR)


UDEB : \
	$(EPOCTRGUDEB)\PicoDrive.exe

UREL : \
	$(EPOCTRGUREL)\PicoDrive.exe


RESOURCEUDEB : MAKEWORKUDEB \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.mbm \
	$(EPOCTRGUDEB)\Z\private\10003a3f\apps\PicoDrive_reg.RSC \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive_loc.RSC \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.RSC

RESOURCEUREL : MAKEWORKUREL \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.mbm \
	$(EPOCTRGUREL)\Z\private\10003a3f\apps\PicoDrive_reg.RSC \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive_loc.RSC \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.RSC



# REAL TARGET - LIBRARY

LIBRARY : MAKEWORKLIBRARY

FREEZE :

CLEANLIBRARY :

GENERIC_RESOURCE : GENERIC_MAKEWORK

# REAL TARGET - BUILD VARIANT UDEB

WHATUDEB : WHATGENERIC

CLEANUDEB : CLEANBUILDUDEB CLEANRELEASEUDEB

CLEANBUILDUDEB : 
	@perl -S ermdir.pl "$(EPOCBLDUDEB)"

CLEANRELEASEUDEB : CLEANGENERIC


UDEB_RELEASEABLES1= \
	$(EPOCTRGUDEB)\PicoDrive.exe \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.RSC \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.mbm \
	$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive_loc.RSC \
	$(EPOCTRGUDEB)\Z\private\10003a3f\apps\PicoDrive_reg.RSC

WHATUDEB:
	@echo $(UDEB_RELEASEABLES1)

CLEANRELEASEUDEB:
	-$(ERASE) $(UDEB_RELEASEABLES1)



LISTINGUDEB : MAKEWORKUDEB \
	LISTINGUDEBpicodriveexe \
	LISTINGUDEBunzip \
	LISTINGUDEBPicoDrive_UID_

LIBSUDEB= \
	$(EPOCSTATLINKUDEB)\pico.lib \
	$(EPOCSTATLINKUDEB)\a68k.obj \
	$(EPOCSTATLINKUDEB)\mz80_asm.obj \
	$(EPOCSTATLINKUDEB)\zlib.lib \
	$(EPOCLINKUDEB)\cone.lib \
	$(EPOCLINKUDEB)\EIKCORE.lib \
	$(EPOCLINKUDEB)\MEDIACLIENTAUDIOSTREAM.LIB \
	$(EPOCLINKUDEB)\euser.lib \
	$(EPOCLINKUDEB)\apparc.lib \
	$(EPOCLINKUDEB)\efsrv.lib \
	$(EPOCLINKUDEB)\estlib.lib \
	$(EPOCLINKUDEB)\fbscli.lib \
	$(EPOCLINKUDEB)\estor.lib \
	$(EPOCLINKUDEB)\eikcoctl.lib \
	$(EPOCLINKUDEB)\ws32.lib \
	$(EPOCLINKUDEB)\AVKON.LIB \
	$(EPOCLINKUDEB)\bafl.lib \
	$(EPOCLINKUDEB)\bitgdi.lib \
	$(EPOCLINKUDEB)\gdi.lib \
	$(EPOCLINKUDEB)\eikdlg.lib

LINK_OBJSUDEB= \
	$(EPOCBLDUDEB)\picodriveexe.o \
	$(EPOCBLDUDEB)\unzip.o \
	$(EPOCBLDUDEB)\PicoDrive_UID_.o

COMMON_LINK_FLAGSUDEB= -stdlib "$(EPOCSTATLINKUDEB)\EEXE.LIB" -m\
 "?_E32Bootstrap@@YGXXZ" -subsystem windows -heapreserve=8000 -heapcommit=256\
 -sym codeview -lMSL_All_MSE_Symbian_D.lib


LINK_FLAGSUDEB= $(COMMON_LINK_FLAGSUDEB) $(LIBSUDEB) \
	 -o "$(EPOCTRGUDEB)\PicoDrive.exe" -noimplib

$(EPOCTRGUDEB)\PicoDrive.exe : $(LINK_OBJSUDEB)  $(EPOCSTATLINKUDEB)\EEXE.LIB $(LIBSUDEB)
	$(COMPILER_PATH)mwldsym2.exe -msgstyle gcc $(LINK_FLAGSUDEB) -l $(EPOCBLDUDEB) -search $(notdir $(LINK_OBJSUDEB))


# REAL TARGET - BUILD VARIANT UREL

WHATUREL : WHATGENERIC

CLEANUREL : CLEANBUILDUREL CLEANRELEASEUREL

CLEANBUILDUREL : 
	@perl -S ermdir.pl "$(EPOCBLDUREL)"

CLEANRELEASEUREL : CLEANGENERIC


UREL_RELEASEABLES1= \
	$(EPOCTRGUREL)\PicoDrive.exe \
	$(EPOCTRGUREL)\PicoDrive.exe.map \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.RSC \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.mbm \
	$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive_loc.RSC \
	$(EPOCTRGUREL)\Z\private\10003a3f\apps\PicoDrive_reg.RSC

WHATUREL:
	@echo $(UREL_RELEASEABLES1)

CLEANRELEASEUREL:
	-$(ERASE) $(UREL_RELEASEABLES1)



LISTINGUREL : MAKEWORKUREL \
	LISTINGURELpicodriveexe \
	LISTINGURELunzip \
	LISTINGURELPicoDrive_UID_

LIBSUREL= \
	$(EPOCSTATLINKUREL)\pico.lib \
	$(EPOCSTATLINKUREL)\a68k.obj \
	$(EPOCSTATLINKUREL)\mz80_asm.obj \
	$(EPOCSTATLINKUREL)\zlib.lib \
	$(EPOCLINKUREL)\cone.lib \
	$(EPOCLINKUREL)\EIKCORE.lib \
	$(EPOCLINKUREL)\MEDIACLIENTAUDIOSTREAM.LIB \
	$(EPOCLINKUREL)\euser.lib \
	$(EPOCLINKUREL)\apparc.lib \
	$(EPOCLINKUREL)\efsrv.lib \
	$(EPOCLINKUREL)\estlib.lib \
	$(EPOCLINKUREL)\fbscli.lib \
	$(EPOCLINKUREL)\estor.lib \
	$(EPOCLINKUREL)\eikcoctl.lib \
	$(EPOCLINKUREL)\ws32.lib \
	$(EPOCLINKUREL)\AVKON.LIB \
	$(EPOCLINKUREL)\bafl.lib \
	$(EPOCLINKUREL)\bitgdi.lib \
	$(EPOCLINKUREL)\gdi.lib \
	$(EPOCLINKUREL)\eikdlg.lib

LINK_OBJSUREL= \
	$(EPOCBLDUREL)\picodriveexe.o \
	$(EPOCBLDUREL)\unzip.o \
	$(EPOCBLDUREL)\PicoDrive_UID_.o

COMMON_LINK_FLAGSUREL= -stdlib "$(EPOCSTATLINKUREL)\EEXE.LIB" -m\
 "?_E32Bootstrap@@YGXXZ" -subsystem windows -heapreserve=8000 -heapcommit=256\
 -lMSL_All_MSE_Symbian.lib


LINK_FLAGSUREL= $(COMMON_LINK_FLAGSUREL) $(LIBSUREL) \
	 -o "$(EPOCTRGUREL)\PicoDrive.exe" -map "$(EPOCTRGUREL)\PicoDrive.exe.map" -noimplib

$(EPOCTRGUREL)\PicoDrive.exe : $(LINK_OBJSUREL)  $(EPOCSTATLINKUREL)\EEXE.LIB $(LIBSUREL)
	$(COMPILER_PATH)mwldsym2.exe -msgstyle gcc $(LINK_FLAGSUREL) -l $(EPOCBLDUREL) -search $(notdir $(LINK_OBJSUREL))


# SOURCES

# BitMap PicoDrive.mbm

GENERIC_RESOURCE : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm

$(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm : \
  \picodrive\s60\picos.bmp \
  \picodrive\s60\picosmi.bmp \
  \picodrive\s60\picol.bmp \
  \picodrive\s60\picolmi.bmp
	perl -S epocmbm.pl -h"\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\PicoDrive.mbg"	-o"$(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm"	-l"\Z\Resource\Apps\:\picodrive\s60"\
		 -b"\
		/c24\picodrive\s60\picos.bmp\
		/8\picodrive\s60\picosmi.bmp\
		/c24\picodrive\s60\picol.bmp\
		/8\picodrive\s60\picolmi.bmp" \
		 -l"\Z\Resource\Apps\:\picodrive\s60"

$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.mbm : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm
	perl -S ecopyfile.pl $? $@

$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.mbm : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm
	perl -S ecopyfile.pl $? $@

# Resource Z\private\10003a3f\apps\PicoDrive_reg.RSC

DEPEND= \
	\S60V3\EPOC32\include\AppInfo.rh \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh

GENERIC_RESOURCE : $(EPOCDATA)\Z\private\10003a3f\apps\PicoDrive_reg.RSC

$(EPOCDATA)\Z\private\10003a3f\apps\PicoDrive_reg.RSC : \picodrive\S60\PicoDrive_reg.rss $(DEPEND)
	perl -S epocrc.pl -m045,046,047 -I "\picodrive\S60" -I "\picodrive\pico" -I "\picodrive\pico\sound" -I "\picodrive\s60" -I "\picodrive" -I- -I "\S60V3\EPOC32\include" -I "\S60V3\EPOC32\include\libc" -I "\S60V3\EPOC32\include\mmf\plugin" -I "\S60V3\epoc32\include\variant" -DLANGUAGE_SC -u "\picodrive\S60\PicoDrive_reg.rss"   -o$@  -t"\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"  -l"Z\private\10003a3f\apps:\picodrive\s60"

$(EPOCTRGUDEB)\Z\private\10003a3f\apps\PicoDrive_reg.RSC : $(EPOCDATA)\Z\private\10003a3f\apps\PicoDrive_reg.RSC
	perl -S ecopyfile.pl $? $@

$(EPOCTRGUREL)\Z\private\10003a3f\apps\PicoDrive_reg.RSC : $(EPOCDATA)\Z\private\10003a3f\apps\PicoDrive_reg.RSC
	perl -S ecopyfile.pl $? $@

# Resource Z\Resource\Apps\PicoDrive_loc.RSC

DEPEND= \
	\S60V3\EPOC32\include\AppInfo.rh \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh

GENERIC_RESOURCE : $(EPOCDATA)\Z\Resource\Apps\PicoDrive_loc.RSC

$(EPOCDATA)\Z\Resource\Apps\PicoDrive_loc.RSC : \picodrive\S60\PicoDrive_loc.rss $(DEPEND)
	perl -S epocrc.pl -m045,046,047 -I "\picodrive\S60" -I "\picodrive\pico" -I "\picodrive\pico\sound" -I "\picodrive\s60" -I "\picodrive" -I- -I "\S60V3\EPOC32\include" -I "\S60V3\EPOC32\include\libc" -I "\S60V3\EPOC32\include\mmf\plugin" -I "\S60V3\epoc32\include\variant" -DLANGUAGE_SC -u "\picodrive\S60\PicoDrive_loc.rss"   -o$@  -t"\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"  -l"Z\Resource\Apps:\picodrive\s60"

$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive_loc.RSC : $(EPOCDATA)\Z\Resource\Apps\PicoDrive_loc.RSC
	perl -S ecopyfile.pl $? $@

$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive_loc.RSC : $(EPOCDATA)\Z\Resource\Apps\PicoDrive_loc.RSC
	perl -S ecopyfile.pl $? $@

# Resource Z\Resource\Apps\PicoDrive.RSC

DEPEND= \
	\S60V3\EPOC32\include\BADEF.RH \
	\S60V3\EPOC32\include\BAERRRSVR.RH \
	\S60V3\EPOC32\include\aknfontcategory.hrh \
	\S60V3\EPOC32\include\aknfontidoffsets.hrh \
	\S60V3\EPOC32\include\avkon.hrh \
	\S60V3\EPOC32\include\avkon.rh \
	\S60V3\EPOC32\include\avkon.rsg \
	\S60V3\EPOC32\include\eikcdlg.rsg \
	\S60V3\EPOC32\include\eikcoctl.rsg \
	\S60V3\EPOC32\include\eikcolor.hrh \
	\S60V3\EPOC32\include\eikcore.rsg \
	\S60V3\EPOC32\include\eikctl.rsg \
	\S60V3\EPOC32\include\eikon.hrh \
	\S60V3\EPOC32\include\eikon.rh \
	\S60V3\EPOC32\include\eikon.rsg \
	\S60V3\EPOC32\include\gulftflg.hrh \
	\S60V3\EPOC32\include\lafpublc.hrh \
	\S60V3\EPOC32\include\uikon.hrh \
	\S60V3\EPOC32\include\uikon.rh \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh

GENERIC_RESOURCE : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.RSC

$(EPOCDATA)\Z\Resource\Apps\PicoDrive.RSC : \picodrive\s60\PicoDrive.rss $(DEPEND)
	perl -S epocrc.pl -m045,046,047 -I "\picodrive\s60" -I "\picodrive\pico" -I "\picodrive\pico\sound" -I "\picodrive\s60" -I "\picodrive" -I- -I "\S60V3\EPOC32\include" -I "\S60V3\EPOC32\include\libc" -I "\S60V3\EPOC32\include\mmf\plugin" -I "\S60V3\epoc32\include\variant" -DLANGUAGE_SC -u "\picodrive\s60\PicoDrive.rss"   -o$@  -h"\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\PicoDrive.rsg" -t"\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"  -l"Z\Resource\Apps:\picodrive\s60"
	perl -S ecopyfile.pl "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\PicoDrive.rsg" "\S60V3\EPOC32\INCLUDE\PicoDrive.RSG"

$(EPOCTRGUDEB)\Z\Resource\Apps\PicoDrive.RSC : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.RSC
	perl -S ecopyfile.pl $? $@

$(EPOCTRGUREL)\Z\Resource\Apps\PicoDrive.RSC : $(EPOCDATA)\Z\Resource\Apps\PicoDrive.RSC
	perl -S ecopyfile.pl $? $@

# Source picodriveexe.cpp

$(EPOCBLDUDEB)\picodriveexe.o \
$(EPOCBLDUREL)\picodriveexe.o \
: \
	\S60V3\EPOC32\include\AknControl.h \
	\S60V3\EPOC32\include\AknMfneCommandObserver.h \
	\S60V3\EPOC32\include\AknPictographDrawerInterface.h \
	\S60V3\EPOC32\include\AknTouchPaneObserver.h \
	\S60V3\EPOC32\include\AknsConstants.h \
	\S60V3\EPOC32\include\AknsConstants.hrh \
	\S60V3\EPOC32\include\AknsItemID.h \
	\S60V3\EPOC32\include\AknsItemID.inl \
	\S60V3\EPOC32\include\E32Base.h \
	\S60V3\EPOC32\include\Ecom\EComErrorCodes.h \
	\S60V3\EPOC32\include\Ecom\EComResolverParams.h \
	\S60V3\EPOC32\include\Ecom\EComResolverParams.inl \
	\S60V3\EPOC32\include\Ecom\Ecom.h \
	\S60V3\EPOC32\include\Ecom\ImplementationInformation.h \
	\S60V3\EPOC32\include\Ecom\ImplementationInformation.inl \
	\S60V3\EPOC32\include\Eikspane.h \
	\S60V3\EPOC32\include\F32File.h \
	\S60V3\EPOC32\include\LineBreak.h \
	\S60V3\EPOC32\include\MdaAudioOutputStream.h \
	\S60V3\EPOC32\include\Mda\Common\Audio.h \
	\S60V3\EPOC32\include\Mda\Common\Audio.hrh \
	\S60V3\EPOC32\include\Mda\Common\AudioStream.hrh \
	\S60V3\EPOC32\include\Mda\Common\Base.h \
	\S60V3\EPOC32\include\Mda\Common\Base.h \
	\S60V3\EPOC32\include\Mda\Common\Base.hrh \
	\S60V3\EPOC32\include\Mda\Common\Base.inl \
	\S60V3\EPOC32\include\Mda\Common\Controller.h \
	\S60V3\EPOC32\include\Mda\Common\Port.h \
	\S60V3\EPOC32\include\Mda\Common\Port.hrh \
	\S60V3\EPOC32\include\Mda\Common\Resource.h \
	\S60V3\EPOC32\include\Mda\Common\Resource.hrh \
	\S60V3\EPOC32\include\MetaContainer.h \
	\S60V3\EPOC32\include\MetaContainer.inl \
	\S60V3\EPOC32\include\MetaData.h \
	\S60V3\EPOC32\include\Metadata.inl \
	\S60V3\EPOC32\include\Mmf\Common\MmfBase.h \
	\S60V3\EPOC32\include\Mmf\Common\MmfUtilities.h \
	\S60V3\EPOC32\include\Mmfclntutility.h \
	\S60V3\EPOC32\include\aknDialog.h \
	\S60V3\EPOC32\include\aknPanic.h \
	\S60V3\EPOC32\include\aknapp.h \
	\S60V3\EPOC32\include\aknappUI.h \
	\S60V3\EPOC32\include\aknappui.h \
	\S60V3\EPOC32\include\akncontrol.h \
	\S60V3\EPOC32\include\akndef.h \
	\S60V3\EPOC32\include\akndef.hrh \
	\S60V3\EPOC32\include\akndoc.h \
	\S60V3\EPOC32\include\aknenv.h \
	\S60V3\EPOC32\include\aknfontcategory.hrh \
	\S60V3\EPOC32\include\aknfontidoffsets.hrh \
	\S60V3\EPOC32\include\aknform.h \
	\S60V3\EPOC32\include\aknintermediate.h \
	\S60V3\EPOC32\include\aknipfed.h \
	\S60V3\EPOC32\include\aknlayout.lag \
	\S60V3\EPOC32\include\aknlayout2id.h \
	\S60V3\EPOC32\include\aknlistquerycontrol.h \
	\S60V3\EPOC32\include\aknlistquerydialog.h \
	\S60V3\EPOC32\include\aknmultilinequerycontrol.h \
	\S60V3\EPOC32\include\aknnumed.h \
	\S60V3\EPOC32\include\aknnumedwin.h \
	\S60V3\EPOC32\include\aknnumseced.h \
	\S60V3\EPOC32\include\aknpopup.h \
	\S60V3\EPOC32\include\aknpopupfader.h \
	\S60V3\EPOC32\include\aknpopupheadingpane.h \
	\S60V3\EPOC32\include\aknpopuplayout.h \
	\S60V3\EPOC32\include\aknquerycontrol.h \
	\S60V3\EPOC32\include\aknquerydata.h \
	\S60V3\EPOC32\include\aknquerydialog.h \
	\S60V3\EPOC32\include\aknscbut.h \
	\S60V3\EPOC32\include\aknscrlb.h \
	\S60V3\EPOC32\include\aknutils.h \
	\S60V3\EPOC32\include\apacmdln.h \
	\S60V3\EPOC32\include\apadef.h \
	\S60V3\EPOC32\include\apaflrec.h \
	\S60V3\EPOC32\include\apaid.h \
	\S60V3\EPOC32\include\apgtask.h \
	\S60V3\EPOC32\include\apmstd.h \
	\S60V3\EPOC32\include\apparc.h \
	\S60V3\EPOC32\include\avkon.hrh \
	\S60V3\EPOC32\include\avkon.rsg \
	\S60V3\EPOC32\include\babitflags.h \
	\S60V3\EPOC32\include\badesca.h \
	\S60V3\EPOC32\include\baerrhan.h \
	\S60V3\EPOC32\include\bamdesca.h \
	\S60V3\EPOC32\include\barsc.h \
	\S60V3\EPOC32\include\basched.h \
	\S60V3\EPOC32\include\bidi.h \
	\S60V3\EPOC32\include\biditext.h \
	\S60V3\EPOC32\include\bitbase.h \
	\S60V3\EPOC32\include\bitdev.h \
	\S60V3\EPOC32\include\bitdev.inl \
	\S60V3\EPOC32\include\bitmap.h \
	\S60V3\EPOC32\include\bitstd.h \
	\S60V3\EPOC32\include\caf\agent.h \
	\S60V3\EPOC32\include\caf\attribute.h \
	\S60V3\EPOC32\include\caf\attributeset.h \
	\S60V3\EPOC32\include\caf\caf.h \
	\S60V3\EPOC32\include\caf\caferr.h \
	\S60V3\EPOC32\include\caf\cafmimeheader.h \
	\S60V3\EPOC32\include\caf\cafpanic.h \
	\S60V3\EPOC32\include\caf\caftypes.h \
	\S60V3\EPOC32\include\caf\caftypes.h \
	\S60V3\EPOC32\include\caf\content.h \
	\S60V3\EPOC32\include\caf\data.h \
	\S60V3\EPOC32\include\caf\dirstreamable.h \
	\S60V3\EPOC32\include\caf\embeddedobject.h \
	\S60V3\EPOC32\include\caf\importfile.h \
	\S60V3\EPOC32\include\caf\manager.h \
	\S60V3\EPOC32\include\caf\metadata.h \
	\S60V3\EPOC32\include\caf\metadataarray.h \
	\S60V3\EPOC32\include\caf\rightsinfo.h \
	\S60V3\EPOC32\include\caf\rightsmanager.h \
	\S60V3\EPOC32\include\caf\streamableptrarray.h \
	\S60V3\EPOC32\include\caf\streamableptrarray.inl \
	\S60V3\EPOC32\include\caf\stringattributeset.h \
	\S60V3\EPOC32\include\caf\supplier.h \
	\S60V3\EPOC32\include\caf\supplieroutputfile.h \
	\S60V3\EPOC32\include\caf\virtualpath.h \
	\S60V3\EPOC32\include\caf\virtualpathptr.h \
	\S60V3\EPOC32\include\coeaui.h \
	\S60V3\EPOC32\include\coeccntx.h \
	\S60V3\EPOC32\include\coecntrl.h \
	\S60V3\EPOC32\include\coecobs.h \
	\S60V3\EPOC32\include\coecontrolarray.h \
	\S60V3\EPOC32\include\coedef.h \
	\S60V3\EPOC32\include\coehelp.h \
	\S60V3\EPOC32\include\coeinput.h \
	\S60V3\EPOC32\include\coemain.h \
	\S60V3\EPOC32\include\coemop.h \
	\S60V3\EPOC32\include\coetextdrawer.h \
	\S60V3\EPOC32\include\coeview.h \
	\S60V3\EPOC32\include\d32locd.h \
	\S60V3\EPOC32\include\d32locd.inl \
	\S60V3\EPOC32\include\e32base.h \
	\S60V3\EPOC32\include\e32base.inl \
	\S60V3\EPOC32\include\e32capability.h \
	\S60V3\EPOC32\include\e32cmn.h \
	\S60V3\EPOC32\include\e32cmn.inl \
	\S60V3\EPOC32\include\e32const.h \
	\S60V3\EPOC32\include\e32debug.h \
	\S60V3\EPOC32\include\e32def.h \
	\S60V3\EPOC32\include\e32des16.h \
	\S60V3\EPOC32\include\e32des8.h \
	\S60V3\EPOC32\include\e32err.h \
	\S60V3\EPOC32\include\e32event.h \
	\S60V3\EPOC32\include\e32hal.h \
	\S60V3\EPOC32\include\e32keys.h \
	\S60V3\EPOC32\include\e32ktran.h \
	\S60V3\EPOC32\include\e32ldr.h \
	\S60V3\EPOC32\include\e32lmsg.h \
	\S60V3\EPOC32\include\e32notif.h \
	\S60V3\EPOC32\include\e32std.h \
	\S60V3\EPOC32\include\e32std.inl \
	\S60V3\EPOC32\include\e32svr.h \
	\S60V3\EPOC32\include\ecom\ECom.h \
	\S60V3\EPOC32\include\ecom\ecom.h \
	\S60V3\EPOC32\include\eikalign.h \
	\S60V3\EPOC32\include\eikamnt.h \
	\S60V3\EPOC32\include\eikapp.h \
	\S60V3\EPOC32\include\eikappui.h \
	\S60V3\EPOC32\include\eikaufty.h \
	\S60V3\EPOC32\include\eikbctrl.h \
	\S60V3\EPOC32\include\eikbtgpc.h \
	\S60V3\EPOC32\include\eikbtgrp.h \
	\S60V3\EPOC32\include\eikbutb.h \
	\S60V3\EPOC32\include\eikcal.h \
	\S60V3\EPOC32\include\eikcba.h \
	\S60V3\EPOC32\include\eikccpu.h \
	\S60V3\EPOC32\include\eikcmbut.h \
	\S60V3\EPOC32\include\eikcmobs.h \
	\S60V3\EPOC32\include\eikcolor.hrh \
	\S60V3\EPOC32\include\eikctgrp.h \
	\S60V3\EPOC32\include\eikcycledef.h \
	\S60V3\EPOC32\include\eikdef.h \
	\S60V3\EPOC32\include\eikdgfty.h \
	\S60V3\EPOC32\include\eikdialg.h \
	\S60V3\EPOC32\include\eikdoc.h \
	\S60V3\EPOC32\include\eikdpobs.h \
	\S60V3\EPOC32\include\eikedwin.h \
	\S60V3\EPOC32\include\eikedwob.h \
	\S60V3\EPOC32\include\eikenv.h \
	\S60V3\EPOC32\include\eikfctry.h \
	\S60V3\EPOC32\include\eikfpne.h \
	\S60V3\EPOC32\include\eikimage.h \
	\S60V3\EPOC32\include\eiklay.h \
	\S60V3\EPOC32\include\eiklbed.h \
	\S60V3\EPOC32\include\eiklbm.h \
	\S60V3\EPOC32\include\eiklbo.h \
	\S60V3\EPOC32\include\eiklbv.h \
	\S60V3\EPOC32\include\eiklbx.h \
	\S60V3\EPOC32\include\eiklibry.h \
	\S60V3\EPOC32\include\eikmenub.h \
	\S60V3\EPOC32\include\eikmenup.h \
	\S60V3\EPOC32\include\eikmfne.h \
	\S60V3\EPOC32\include\eikmobs.h \
	\S60V3\EPOC32\include\eikon.hrh \
	\S60V3\EPOC32\include\eiksbfrm.h \
	\S60V3\EPOC32\include\eiksbobs.h \
	\S60V3\EPOC32\include\eikscbut.h \
	\S60V3\EPOC32\include\eikscrlb.h \
	\S60V3\EPOC32\include\eikseced.h \
	\S60V3\EPOC32\include\eikspmod.h \
	\S60V3\EPOC32\include\eiksrv.h \
	\S60V3\EPOC32\include\eiksrv.pan \
	\S60V3\EPOC32\include\eiksrvc.h \
	\S60V3\EPOC32\include\eiksrvs.h \
	\S60V3\EPOC32\include\eikstart.h \
	\S60V3\EPOC32\include\eiktxlbm.h \
	\S60V3\EPOC32\include\eiktxlbx.h \
	\S60V3\EPOC32\include\eikunder.h \
	\S60V3\EPOC32\include\eikvcurs.h \
	\S60V3\EPOC32\include\es_sock.h \
	\S60V3\EPOC32\include\es_sock.inl \
	\S60V3\EPOC32\include\f32file.h \
	\S60V3\EPOC32\include\f32file.inl \
	\S60V3\EPOC32\include\fbs.h \
	\S60V3\EPOC32\include\fepbase.h \
	\S60V3\EPOC32\include\fldbase.h \
	\S60V3\EPOC32\include\fldbltin.h \
	\S60V3\EPOC32\include\fldinfo.h \
	\S60V3\EPOC32\include\fldset.h \
	\S60V3\EPOC32\include\fntstore.h \
	\S60V3\EPOC32\include\frmframe.h \
	\S60V3\EPOC32\include\frmlaydt.h \
	\S60V3\EPOC32\include\frmparam.h \
	\S60V3\EPOC32\include\frmtlay.h \
	\S60V3\EPOC32\include\frmtview.h \
	\S60V3\EPOC32\include\frmvis.h \
	\S60V3\EPOC32\include\gdi.h \
	\S60V3\EPOC32\include\gdi.inl \
	\S60V3\EPOC32\include\graphicsaccelerator.h \
	\S60V3\EPOC32\include\gulalign.h \
	\S60V3\EPOC32\include\gulbordr.h \
	\S60V3\EPOC32\include\gulcolor.h \
	\S60V3\EPOC32\include\guldef.h \
	\S60V3\EPOC32\include\gulftflg.hrh \
	\S60V3\EPOC32\include\gulicon.h \
	\S60V3\EPOC32\include\gulutil.h \
	\S60V3\EPOC32\include\in_sock.h \
	\S60V3\EPOC32\include\lafmain.h \
	\S60V3\EPOC32\include\lafpublc.h \
	\S60V3\EPOC32\include\lafpublc.hrh \
	\S60V3\EPOC32\include\libc\_ansi.h \
	\S60V3\EPOC32\include\libc\ctype.h \
	\S60V3\EPOC32\include\libc\machine\types.h \
	\S60V3\EPOC32\include\libc\stdarg_e.h \
	\S60V3\EPOC32\include\libc\stddef.h \
	\S60V3\EPOC32\include\libc\stdio.h \
	\S60V3\EPOC32\include\libc\stdlib.h \
	\S60V3\EPOC32\include\libc\string.h \
	\S60V3\EPOC32\include\libc\sys\reent.h \
	\S60V3\EPOC32\include\libc\sys\stdio_t.h \
	\S60V3\EPOC32\include\libc\sys\time.h \
	\S60V3\EPOC32\include\libc\time.h \
	\S60V3\EPOC32\include\mda\client\utility.h \
	\S60V3\EPOC32\include\mda\common\base.h \
	\S60V3\EPOC32\include\medobsrv.h \
	\S60V3\EPOC32\include\mm\mmcaf.h \
	\S60V3\EPOC32\include\mmf\common\MmfFourCC.h \
	\S60V3\EPOC32\include\mmf\common\MmfIpc.inl \
	\S60V3\EPOC32\include\mmf\common\MmfUtilities.h \
	\S60V3\EPOC32\include\mmf\common\MmfUtilities.inl \
	\S60V3\EPOC32\include\mmf\common\Mmfbase.h \
	\S60V3\EPOC32\include\mmf\common\mmcaf.h \
	\S60V3\EPOC32\include\mmf\common\mmfaudio.h \
	\S60V3\EPOC32\include\mmf\common\mmfbase.h \
	\S60V3\EPOC32\include\mmf\common\mmfcontroller.h \
	\S60V3\EPOC32\include\mmf\common\mmfcontrollerframework.h \
	\S60V3\EPOC32\include\mmf\common\mmfcontrollerframeworkbase.h \
	\S60V3\EPOC32\include\mmf\common\mmfcontrollerpluginresolver.h \
	\S60V3\EPOC32\include\mmf\common\mmfipc.h \
	\S60V3\EPOC32\include\mmf\common\mmfstandardcustomcommands.h \
	\S60V3\EPOC32\include\mmf\common\mmfutilities.h \
	\S60V3\EPOC32\include\mmf\common\mmfvideo.h \
	\S60V3\EPOC32\include\mmf\plugin\mmfPluginInterfaceUIDs.hrh \
	\S60V3\EPOC32\include\mmf\server\mmfbuffer.h \
	\S60V3\EPOC32\include\mmf\server\mmfbuffer.hrh \
	\S60V3\EPOC32\include\mmf\server\mmfdatabuffer.h \
	\S60V3\EPOC32\include\mmf\server\mmfdatasink.h \
	\S60V3\EPOC32\include\mmf\server\mmfdatasource.h \
	\S60V3\EPOC32\include\mmf\server\mmfdatasourcesink.hrh \
	\S60V3\EPOC32\include\nifvar.h \
	\S60V3\EPOC32\include\openfont.h \
	\S60V3\EPOC32\include\partitions.h \
	\S60V3\EPOC32\include\picodrive.rsg \
	\S60V3\EPOC32\include\s32buf.h \
	\S60V3\EPOC32\include\s32buf.inl \
	\S60V3\EPOC32\include\s32file.h \
	\S60V3\EPOC32\include\s32file.inl \
	\S60V3\EPOC32\include\s32mem.h \
	\S60V3\EPOC32\include\s32mem.inl \
	\S60V3\EPOC32\include\s32page.h \
	\S60V3\EPOC32\include\s32page.inl \
	\S60V3\EPOC32\include\s32share.h \
	\S60V3\EPOC32\include\s32share.inl \
	\S60V3\EPOC32\include\s32std.h \
	\S60V3\EPOC32\include\s32std.inl \
	\S60V3\EPOC32\include\s32stor.h \
	\S60V3\EPOC32\include\s32stor.inl \
	\S60V3\EPOC32\include\s32strm.h \
	\S60V3\EPOC32\include\s32strm.inl \
	\S60V3\EPOC32\include\savenotf.h \
	\S60V3\EPOC32\include\tagma.h \
	\S60V3\EPOC32\include\txtetext.h \
	\S60V3\EPOC32\include\txtetext.inl \
	\S60V3\EPOC32\include\txtfmlyr.h \
	\S60V3\EPOC32\include\txtfmlyr.inl \
	\S60V3\EPOC32\include\txtfmstm.h \
	\S60V3\EPOC32\include\txtfrmat.h \
	\S60V3\EPOC32\include\txtfrmat.inl \
	\S60V3\EPOC32\include\txtstyle.h \
	\S60V3\EPOC32\include\txtstyle.inl \
	\S60V3\EPOC32\include\uikon.hrh \
	\S60V3\EPOC32\include\vwsappst.h \
	\S60V3\EPOC32\include\vwsdef.h \
	\S60V3\EPOC32\include\w32std.h \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh \
	\picodrive\pico\GGenie.h \
	\picodrive\pico\Pico.h \
	\picodrive\pico\PicoInt.h \
	\picodrive\pico\pico.h \
	\picodrive\s60\PicoDriveexe.h \
	\picodrive\s60\S60V3Video.inl \
	\picodrive\unzip.h

$(EPOCBLDUDEB)\picodriveexe.o : \picodrive\S60\picodriveexe.cpp
	echo picodriveexe.cpp
	$(CWUDEB) -o "$@" -c "\picodrive\S60\picodriveexe.cpp"

LISTINGUDEBpicodriveexe : $(EPOCBLDUDEB)\picodriveexe.lis
	perl -S ecopyfile.pl $? \picodrive\S60\picodriveexe.WINSCW.lst

$(EPOCBLDUREL)\picodriveexe.o : \picodrive\S60\picodriveexe.cpp
	echo picodriveexe.cpp
	$(CWUREL) -o "$@" -c "\picodrive\S60\picodriveexe.cpp"

LISTINGURELpicodriveexe : $(EPOCBLDUREL)\picodriveexe.lis
	perl -S ecopyfile.pl $? \picodrive\S60\picodriveexe.WINSCW.lst



# Source unzip.c

$(EPOCBLDUDEB)\unzip.o \
$(EPOCBLDUREL)\unzip.o \
: \
	\S60V3\EPOC32\include\libc\_ansi.h \
	\S60V3\EPOC32\include\libc\assert.h \
	\S60V3\EPOC32\include\libc\ctype.h \
	\S60V3\EPOC32\include\libc\machine\types.h \
	\S60V3\EPOC32\include\libc\stdarg_e.h \
	\S60V3\EPOC32\include\libc\stddef.h \
	\S60V3\EPOC32\include\libc\stdio.h \
	\S60V3\EPOC32\include\libc\stdlib.h \
	\S60V3\EPOC32\include\libc\string.h \
	\S60V3\EPOC32\include\libc\sys\reent.h \
	\S60V3\EPOC32\include\libc\sys\stdio_t.h \
	\S60V3\EPOC32\include\libc\time.h \
	\S60V3\EPOC32\include\zconf.h \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh \
	\picodrive\unzip.h \
	\picodrive\zlib\zlib.h

$(EPOCBLDUDEB)\unzip.o : \picodrive\unzip.c
	echo unzip.c
	$(CWUDEB) -o "$@" -c "\picodrive\unzip.c"

LISTINGUDEBunzip : $(EPOCBLDUDEB)\unzip.lis
	perl -S ecopyfile.pl $? \picodrive\unzip.WINSCW.lst

$(EPOCBLDUREL)\unzip.o : \picodrive\unzip.c
	echo unzip.c
	$(CWUREL) -o "$@" -c "\picodrive\unzip.c"

LISTINGURELunzip : $(EPOCBLDUREL)\unzip.lis
	perl -S ecopyfile.pl $? \picodrive\unzip.WINSCW.lst



# Source PicoDrive.UID.CPP

$(EPOCBLDUDEB)\PicoDrive_UID_.o \
$(EPOCBLDUREL)\PicoDrive_UID_.o \
: \
	\S60V3\EPOC32\include\e32capability.h \
	\S60V3\EPOC32\include\e32cmn.h \
	\S60V3\EPOC32\include\e32cmn.inl \
	\S60V3\EPOC32\include\e32const.h \
	\S60V3\EPOC32\include\e32def.h \
	\S60V3\EPOC32\include\e32des16.h \
	\S60V3\EPOC32\include\e32des8.h \
	\S60V3\EPOC32\include\e32err.h \
	\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh

$(EPOCBLDUDEB)\PicoDrive_UID_.o : \picodrive\s60\PicoDrive.UID.CPP
	echo PicoDrive.UID.CPP
	$(CWUDEB) -o "$@" -c "\picodrive\s60\PicoDrive.UID.CPP"

LISTINGUDEBPicoDrive_UID_ : $(EPOCBLDUDEB)\PicoDrive_UID_.lis
	perl -S ecopyfile.pl $? \picodrive\s60\PicoDrive_UID_.WINSCW.lst

$(EPOCBLDUREL)\PicoDrive_UID_.o : \picodrive\s60\PicoDrive.UID.CPP
	echo PicoDrive.UID.CPP
	$(CWUREL) -o "$@" -c "\picodrive\s60\PicoDrive.UID.CPP"

LISTINGURELPicoDrive_UID_ : $(EPOCBLDUREL)\PicoDrive_UID_.lis
	perl -S ecopyfile.pl $? \picodrive\s60\PicoDrive_UID_.WINSCW.lst



ROMFILE:

# Implicit rule for generating .lis files

.SUFFIXES : .lis .o

.o.lis:
	$(COMPILER_PATH)mwldsym2.exe -msgstyle gcc -S -show source,unmangled,comments $< -o $@



GENERIC_RELEASEABLES1= \
	$(EPOCDATA)\Z\Resource\Apps\PicoDrive.RSC \
	$(EPOCDATA)\Z\Resource\Apps\PicoDrive.mbm \
	$(EPOCDATA)\Z\Resource\Apps\PicoDrive_loc.RSC \
	$(EPOCDATA)\Z\private\10003a3f\apps\PicoDrive_reg.RSC \
	$(EPOCINC)\PicoDrive.RSG \
	\S60V3\EPOC32\LOCALISATION\GROUP\PICODRIVE.INFO \
	\S60V3\EPOC32\LOCALISATION\GROUP\PICODRIVE_LOC.INFO \
	\S60V3\EPOC32\LOCALISATION\GROUP\PICODRIVE_REG.INFO \
	\S60V3\EPOC32\LOCALISATION\PICODRIVE_LOC\RSC\PICODRIVE_LOC.RPP \
	\S60V3\EPOC32\LOCALISATION\PICODRIVE_REG\RSC\PICODRIVE_REG.RPP \
	\S60V3\EPOC32\LOCALISATION\\MBM\PICOL.BMP \
	\S60V3\EPOC32\LOCALISATION\\MBM\PICOLMI.BMP \
	\S60V3\EPOC32\LOCALISATION\\MBM\PICOS.BMP \
	\S60V3\EPOC32\LOCALISATION\\MBM\PICOSMI.BMP \
	\S60V3\EPOC32\LOCALISATION\\RSC\PICODRIVE.RPP

WHATGENERIC:
	@echo $(GENERIC_RELEASEABLES1)

CLEANGENERIC:
	-$(ERASE) $(GENERIC_RELEASEABLES1)

# Rules to create all necessary directories

GENERIC_MAKEWORK : \
	\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW \
	\S60V3\EPOC32\DATA\Z\Resource\Apps \
	\S60V3\EPOC32\DATA\Z\private\10003a3f\apps \
	\S60V3\EPOC32\INCLUDE
MAKEWORKLIBRARY : \
	\S60V3\EPOC32\RELEASE\WINSCW\UDEB
MAKEWORKUDEB : \
	\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UDEB \
	\S60V3\EPOC32\RELEASE\WINSCW\UDEB \
	\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps \
	\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\private\10003a3f\apps
MAKEWORKUREL : \
	\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UREL \
	\S60V3\EPOC32\RELEASE\WINSCW\UREL \
	\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps \
	\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\private\10003a3f\apps

\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW \
\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UDEB \
\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UREL \
\S60V3\EPOC32\DATA\Z\Resource\Apps \
\S60V3\EPOC32\DATA\Z\private\10003a3f\apps \
\S60V3\EPOC32\INCLUDE \
\S60V3\EPOC32\RELEASE\WINSCW\UDEB \
\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps \
\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\private\10003a3f\apps \
\S60V3\EPOC32\RELEASE\WINSCW\UREL \
\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps \
\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\private\10003a3f\apps \
:
	perl -S emkdir.pl $@

