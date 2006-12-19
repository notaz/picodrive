# Microsoft Developer Studio Project File - Name="PICO" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=PICO - Win32 Uni Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PICO.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PICO.mak" CFG="PICO - Win32 Uni Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PICO - Win32 Uni Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "PICO - Win32 Uni Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "PICO - Win32 Uni Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Win32_U0"
# PROP BASE Intermediate_Dir ".\Win32_U0"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "\s60v1\EPOC32\RELEASE\WINS\UDEB"
# PROP Intermediate_Dir "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICO\WINS\UDEB"
# ADD CPP /nologo /Zp4 /MDd /W4 /Zi /Od /X /I "\PICODRIVE\PICO" /I "\PICODRIVE\CYCLONE" /I "\s60v1\EPOC32\INCLUDE" /I "\s60v1\EPOC32\INCLUDE\LIBC" /D "__SYMBIAN32__" /D "__VC32__" /D "__WINS__" /D "__AVKON_ELAF__" /D "_USE_MZ80" /D "EMU_A68K" /D "_DEBUG" /D "_UNICODE" /FR /Fd"\s60v1\EPOC32\RELEASE\WINS\UDEB\PICO.PDB" /GF /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /machine:IX86
# ADD LIB32 /nologo /subsystem:windows /machine:IX86 /nodefaultlib /out:"\s60v1\EPOC32\RELEASE\WINS\UDEB\PICO.LIB"

!ELSEIF  "$(CFG)" == "PICO - Win32 Uni Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Win32_Un"
# PROP BASE Intermediate_Dir ".\Win32_Un"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "\s60v1\EPOC32\RELEASE\WINS\UREL"
# PROP Intermediate_Dir "\s60v1\EPOC32\BUILD\PICODRIVE\S60\PICO\WINS\UREL"
# ADD CPP /nologo /Zp4 /MD /W4 /O1 /Op /X /I "\PICODRIVE\PICO" /I "\PICODRIVE\CYCLONE" /I "\s60v1\EPOC32\INCLUDE" /I "\s60v1\EPOC32\INCLUDE\LIBC" /D "__SYMBIAN32__" /D "__VC32__" /D "__WINS__" /D "__AVKON_ELAF__" /D "_USE_MZ80" /D "EMU_A68K" /D "NDEBUG" /D "_UNICODE" /GF /c
# ADD BASE RSC /l 0x809
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /machine:IX86
# ADD LIB32 /nologo /subsystem:windows /machine:IX86 /nodefaultlib /out:"\s60v1\EPOC32\RELEASE\WINS\UREL\PICO.LIB"

!ENDIF 

# Begin Target

# Name "PICO - Win32 Uni Debug"
# Name "PICO - Win32 Uni Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=\PICODRIVE\PICO\Area.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Cart.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Draw.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Draw2.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Ggenie.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Memory.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Misc.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Pico.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Sek.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Utils.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Videoport.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Sn76496.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Sound.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Ym2612.c
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\S60\Pico.mmp
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

SOURCE=\PICODRIVE\PICO\SOUND\Sn76496.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Ym2612.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Pico.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Mz80.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Driver.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\SOUND\Sound.h
# End Source File
# Begin Source File

SOURCE=\PICODRIVE\PICO\Picoint.h
# End Source File
# End Group
# End Target
# End Project
