# Microsoft Developer Studio Project File - Name="GenaDrive" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=GenaDrive - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "GenaDrive.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "GenaDrive.mak" CFG="GenaDrive - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "GenaDrive - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "GenaDrive - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "GenaDrive - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W4 /GX /O2 /I "..\..\..\Pico" /I ".\\" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "EMU_M68K" /D "_USE_MZ80" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 gdi32.lib user32.lib advapi32.lib d3d8.lib d3dx8.lib dsound.lib comdlg32.lib /nologo /subsystem:windows /machine:I386

!ELSEIF  "$(CFG)" == "GenaDrive - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /I "..\..\..\Pico" /I ".\\" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "DEBUG" /D "EMU_M68K" /D "_USE_MZ80" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 user32.lib gdi32.lib advapi32.lib d3d8.lib d3dx8.lib dsound.lib comdlg32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "GenaDrive - Win32 Release"
# Name "GenaDrive - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\Emu.cpp
# End Source File
# Begin Source File

SOURCE=.\GenaDrive.txt
# End Source File
# Begin Source File

SOURCE=.\Input.cpp
# End Source File
# Begin Source File

SOURCE=.\LightCal.cpp
# End Source File
# Begin Source File

SOURCE=.\Loop.cpp
# End Source File
# Begin Source File

SOURCE=.\Main.cpp
# End Source File
# Begin Source File

SOURCE=.\Makefile
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\app.h
# End Source File
# End Group
# Begin Group "DirectX"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Direct.cpp
# End Source File
# Begin Source File

SOURCE=.\DSound.cpp
# End Source File
# Begin Source File

SOURCE=.\FileMenu.cpp
# End Source File
# Begin Source File

SOURCE=.\FileMenu.h
# End Source File
# Begin Source File

SOURCE=.\Font.cpp
# End Source File
# Begin Source File

SOURCE=.\TexScreen.cpp
# End Source File
# End Group
# Begin Group "Pico"

# PROP Default_Filter ""
# Begin Group "sound"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\Pico\sound\driver.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\sn76496.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\sn76496.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\sound.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\sound.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\ym2612.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\sound\ym2612.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\Pico\Area.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Cart.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Draw.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Draw2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Memory.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Misc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Pico.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Pico.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\PicoInt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Sek.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\Utils.c
# End Source File
# Begin Source File

SOURCE=..\..\..\Pico\VideoPort.c
# End Source File
# End Group
# Begin Group "cores"

# PROP Default_Filter ""
# Begin Group "musashi"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68k.h
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kconf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kcpu.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kcpu.h
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kdasm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kopac.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kopdm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kopnz.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kops.c
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\musashi\m68kops.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\..\cpu\mz80\mz80.obj
# End Source File
# Begin Source File

SOURCE=..\..\..\cpu\a68k\a68k.obj

!IF  "$(CFG)" == "GenaDrive - Win32 Release"

!ELSEIF  "$(CFG)" == "GenaDrive - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
