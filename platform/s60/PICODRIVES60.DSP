# Microsoft Developer Studio Project File - Name="PICODRIVE" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=PICODRIVE - Win32 Uni Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PICODRIVE.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PICODRIVE.mak" CFG="PICODRIVE - Win32 Uni Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PICODRIVE - Win32 Uni Release" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE "PICODRIVE - Win32 Uni Debug" (based on\
 "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "PICODRIVE - Win32 Uni Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Win32_U0"
# PROP BASE Intermediate_Dir ".\Win32_U0"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "\s60v1\EPOC32\RELEASE\WINS\UDEB"
# PROP Intermediate_Dir "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICODRIVES60\WINS\UDEB"
# ADD CPP /nologo /Zp4 /MDd /W4 /Zi /Od /X /I "\PICODRIVE\PICO" /I "\PICODRIVE\PICO\SOUND" /I "\PICODRIVE\S60" /I "\PICODRIVE" /I "\s60v1\EPOC32\INCLUDE" /I "\s60v1\EPOC32\INCLUDE\LIBC" /D "__SYMBIAN32__" /D "__VC32__" /D "__WINS__" /D "__AVKON_ELAF__" /D "__DLL__" /D "_DEBUG" /D "_UNICODE" /FR /Fd"\s60v1\EPOC32\RELEASE\WINS\UDEB\Z\SYSTEM\APPS\PICODRIVES60\PICODRIVE.PDB" /GF /c
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 "\s60v1\EPOC32\RELEASE\WINS\UDEB\EDLL.LIB" "\s60v1\EPOC32\RELEASE\WINS\UDEB\pico.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\a68k.obj" "\s60v1\EPOC32\RELEASE\WINS\UDEB\mz80_asm.obj" "\s60v1\EPOC32\RELEASE\WINS\UDEB\cone.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikcore.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\mediaclientaudiostream.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\euser.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\zlib.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\efsrv.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\estlib.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\fbscli.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\estor.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikcoctl.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\ws32.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\avkon.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\bafl.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\bitgdi.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikdlg.lib" "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICODRIVES60\WINS\UDEB\PICODRIVE.exp" /nologo /entry:"_E32Dll" /subsystem:windows /dll /pdb:"\s60v1\EPOC32\RELEASE\WINS\UDEB\Z\SYSTEM\APPS\PICODRIVES60\PICODRIVE.pdb" /debug /machine:IX86 /nodefaultlib /include:"?_E32Dll@@YGHPAXI0@Z" /out:"\s60v1\EPOC32\RELEASE\WINS\UDEB\Z\SYSTEM\APPS\PICODRIVES60\PICODRIVE.DLL" /WARN:3
# SUBTRACT LINK32 /pdb:none
# Begin Special Build Tool
SOURCE=$(InputPath)
PreLink_Cmds=echo Doing first-stage link by name\
	nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" PRELINKUDEB\
	if errorlevel 1 nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" STOPLINKUDEB 
PostBuild_Cmds=nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" POSTBUILDUDEB
# End Special Build Tool

!ELSEIF  "$(CFG)" == "PICODRIVE - Win32 Uni Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Win32_Un"
# PROP BASE Intermediate_Dir ".\Win32_Un"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "\s60v1\EPOC32\RELEASE\WINS\UREL"
# PROP Intermediate_Dir "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICODRIVES60\WINS\UREL"
# ADD CPP /nologo /Zp4 /MD /W4 /O1 /Op /X /I "\PICODRIVE\PICO" /I "\PICODRIVE\PICO\SOUND" /I "\PICODRIVE\S60" /I "\PICODRIVE" /I "\s60v1\EPOC32\INCLUDE" /I "\s60v1\EPOC32\INCLUDE\LIBC" /D "__SYMBIAN32__" /D "__VC32__" /D "__WINS__" /D "__AVKON_ELAF__" /D "__DLL__" /D "NDEBUG" /D "_UNICODE" /GF /c
# ADD MTL /nologo /mktyplib203 /D /win32
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /machine:IX86
# ADD LINK32 "\s60v1\EPOC32\RELEASE\WINS\UREL\EDLL.LIB" "\s60v1\EPOC32\RELEASE\WINS\UREL\pico.lib" "\s60v1\EPOC32\RELEASE\WINS\UREL\a68k.obj" "\s60v1\EPOC32\RELEASE\WINS\UREL\mz80_asm.obj" "\s60v1\EPOC32\RELEASE\WINS\UDEB\cone.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikcore.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\mediaclientaudiostream.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\euser.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\zlib.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\efsrv.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\estlib.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\fbscli.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\estor.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikcoctl.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\ws32.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\avkon.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\bafl.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\bitgdi.lib" "\s60v1\EPOC32\RELEASE\WINS\UDEB\eikdlg.lib" "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICODRIVES60\WINS\UREL\PICODRIVE.exp" /nologo /entry:"_E32Dll" /subsystem:windows /dll /machine:IX86 /nodefaultlib /include:"?_E32Dll@@YGHPAXI0@Z" /out:"\s60v1\EPOC32\RELEASE\WINS\UREL\Z\SYSTEM\APPS\PICODRIVES60\PICODRIVE.DLL" /WARN:3
# Begin Special Build Tool
SOURCE=$(InputPath)
PreLink_Cmds=echo Doing first-stage link by name\
	nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" PRELINKUREL\
	if errorlevel 1 nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" STOPLINKUREL 
PostBuild_Cmds=nmake -nologo -f "\PICODRIVE\S60\PICODRIVES60.SUP.MAKE" POSTBUILDUREL
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "PICODRIVE - Win32 Uni Debug"
# Name "PICODRIVE - Win32 Uni Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=\PICODRIVE\Unzip.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Picodrive.uid.cpp
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Picodriveexe.cpp
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Picodrives60.mmp
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=\PICODRIVE\PICO\Ggenie.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Pico.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\Unzip.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Normalvideo.inl
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Interpolatevideo.inl
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Picodriveexe.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Picoint.h
# End Source File
# End Group
# End Target
# End Project
