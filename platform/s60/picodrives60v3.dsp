# Microsoft Developer Studio Project File - Name="PicoDrive" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=PicoDrive - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PicoDrive.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PicoDrive.mak" CFG="PicoDrive - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PicoDrive - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "PicoDrive - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "PicoDrive - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Cmd_Line "NMAKE /f PicoDrive.mak "
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "PicoDrive.exe"
# PROP BASE Bsc_Name "PicoDrive.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "\S60V3\EPOC32\RELEASE\WINSCW\UDEB"
# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UDEB"
# PROP Cmd_Line ""\S60V3\epoc32\tools\MAKE.exe" -r -f picodrives60v3_UDEB.mak "
# PROP Rebuild_Opt "REBUILD"
# PROP Target_File "PicoDrive.exe"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "PicoDrive - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Cmd_Line "NMAKE /f PicoDrive.mak "
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "PicoDrive.exe"
# PROP BASE Bsc_Name "PicoDrive.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "\S60V3\EPOC32\RELEASE\WINSCW\UREL"
# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW\UREL"
# PROP Cmd_Line ""\S60V3\epoc32\tools\MAKE.exe" -r -f picodrives60v3_UREL.mak "
# PROP Rebuild_Opt "REBUILD"
# PROP Target_File "PicoDrive.exe"
# PROP Bsc_Name "PicoDrive.bsc"
# PROP Target_Dir ""


!ENDIF

# Begin Target

# Name "PicoDrive - Win32 Debug"
# Name "PicoDrive - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;hpj;bat;for;f90"
# Begin Source File

SOURCE=\picodrive\S60\Picodrive_reg.rss
USERDEP__PicoDrive_reg="\S60V3\EPOC32\include\AppInfo.rh"	"\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh"	
!IF  "$(CFG)" == "PicoDrive - Win32 Debug"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive_reg.rss
InputPath=\picodrive\S60\Picodrive_reg.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\private\10003a3f\apps\PicoDrive_reg.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\private\10003a3f\apps\PicoDrive_reg.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "PicoDrive - Win32 Release"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive_reg.rss
InputPath=\picodrive\S60\Picodrive_reg.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\private\10003a3f\apps\PicoDrive_reg.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\private\10003a3f\apps\PicoDrive_reg.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=\picodrive\S60\Picodrive_loc.rss
USERDEP__PicoDrive_loc="\S60V3\EPOC32\include\AppInfo.rh"	"\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh"	
!IF  "$(CFG)" == "PicoDrive - Win32 Debug"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive_loc.rss
InputPath=\picodrive\S60\Picodrive_loc.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps\PicoDrive_loc.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps\PicoDrive_loc.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "PicoDrive - Win32 Release"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive_loc.rss
InputPath=\picodrive\S60\Picodrive_loc.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps\PicoDrive_loc.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps\PicoDrive_loc.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=\picodrive\s60\Picodrive.rss
USERDEP__PicoDrive="\S60V3\EPOC32\include\BADEF.RH"	"\S60V3\EPOC32\include\BAERRRSVR.RH"	"\S60V3\EPOC32\include\aknfontcategory.hrh"	"\S60V3\EPOC32\include\aknfontidoffsets.hrh"	"\S60V3\EPOC32\include\avkon.hrh"	"\S60V3\EPOC32\include\avkon.rh"	"\S60V3\EPOC32\include\avkon.rsg"	"\S60V3\EPOC32\include\eikcdlg.rsg"	"\S60V3\EPOC32\include\eikcoctl.rsg"	"\S60V3\EPOC32\include\eikcolor.hrh"	"\S60V3\EPOC32\include\eikcore.rsg"	"\S60V3\EPOC32\include\eikctl.rsg"	"\S60V3\EPOC32\include\eikon.hrh"	"\S60V3\EPOC32\include\eikon.rh"	"\S60V3\EPOC32\include\eikon.rsg"	"\S60V3\EPOC32\include\gulftflg.hrh"	"\S60V3\EPOC32\include\lafpublc.hrh"	"\S60V3\EPOC32\include\uikon.hrh"	"\S60V3\EPOC32\include\uikon.rh"	"\S60V3\epoc32\include\variant\Symbian_OS_v9.1.hrh"	
!IF  "$(CFG)" == "PicoDrive - Win32 Debug"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive.rss
InputPath=\picodrive\s60\Picodrive.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps\PicoDrive.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UDEB\Z\Resource\Apps\PicoDrive.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "PicoDrive - Win32 Release"

# PROP Intermediate_Dir "\S60V3\EPOC32\BUILD\picodrive\s60\picodrives60v3\WINSCW"
# Begin Custom Build - Building resources from Picodrive.rss
InputPath=\picodrive\s60\Picodrive.rss

BuildCmds= \
	nmake -nologo -f "\picodrive\s60\picodrives60v3.SUP.MAKE"\
  "\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps\PicoDrive.r"

"\S60V3\EPOC32\RELEASE\WINSCW\UREL\Z\Resource\Apps\PicoDrive.rSC.dummy" : $(SOURCE) "$(INTDIR)"\
 "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=\picodrive\S60\Picodriveexe.cpp
# End Source File
# Begin Source File

SOURCE=\picodrive\Unzip.c
# End Source File
# Begin Source File

SOURCE=\picodrive\s60\Picodrive.uid.cpp
# End Source File
# Begin Source File

SOURCE=\picodrive\S60\S60v3video.inl
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=\picodrive\S60\Interpolatevideo.inl
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=\picodrive\S60\Normalvideo.inl
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=\picodrive\s60\Picodrives60v3.mmp
# PROP Exclude_From_Build 1
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl;fi;fd"
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmffourcc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Base.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikaufty.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknmultilinequerycontrol.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32buf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknlayout2id.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Ecomresolverparams.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikfpne.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknintermediate.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mmf\Common\Mmfbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknfontidoffsets.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\String.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apmstd.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknappui.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Ecom.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikspane.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfbuffer.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Stdlib.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikalign.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklbv.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknappui.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apgtask.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksrvs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bitdev.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikdpobs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfcontrollerframework.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Babitflags.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfvideo.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Vwsdef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Ctype.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikimage.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikdef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikbutb.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bitbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Importfile.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpopupfader.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akndef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Metacontainer.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32cmn.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknscrlb.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fldbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikscbut.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfstandardcustomcommands.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32base.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikmobs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32std.inl
# End Source File
# Begin Source File

SOURCE=\picodrive\pico\Pico.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coehelp.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32const.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikdialg.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apparc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coemop.h
# End Source File
# Begin Source File

SOURCE=\picodrive\s60\Picodriveexe.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtetext.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknquerycontrol.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmlaydt.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikappui.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32err.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfdatabuffer.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Stringattributeset.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\sys\Stdio_t.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulcolor.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklbx.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coeview.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Resource.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknlistquerycontrol.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikdoc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Caferr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpopuplayout.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfipc.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Graphicsaccelerator.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32file.inl
# End Source File
# Begin Source File

SOURCE=\picodrive\s60\S60v3video.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfipc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fldbltin.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Caftypes.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Tagma.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\In_sock.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apaid.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Resource.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Caf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtfrmat.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Stddef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\sys\Reent.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulutil.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Agent.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Manager.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknlistquerydialog.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Partitions.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulbordr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Virtualpathptr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Picodrive.rsg
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Barsc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtfmlyr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32strm.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Linebreak.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikstart.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32strm.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Zconf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gdi.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32ldr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\W32std.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiktxlbm.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtfmstm.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulalign.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Lafmain.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gdi.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikamnt.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32svr.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mmf\Common\Mmfutilities.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Ecomerrorcodes.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Audio.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32cmn.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Implementationinformation.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulftflg.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\epoc32\include\variant\Symbian_os_v9.1.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikedwin.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32page.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksrv.pan
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcmbut.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Gulicon.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknnumseced.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtfrmat.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Ecomresolverparams.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcolor.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Stdio.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcba.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Badesca.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcmobs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknapp.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknmfnecommandobserver.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Baerrhan.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fldinfo.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiktxlbx.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikccpu.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfcontrollerframeworkbase.h
# End Source File
# Begin Source File

SOURCE=\picodrive\pico\Ggenie.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Avkon.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fldset.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpictographdrawerinterface.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Vwsappst.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfutilities.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Content.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksbobs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bamdesca.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\D32locd.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfaudio.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikmenub.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32stor.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\plugin\Mmfplugininterfaceuids.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32std.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikbtgpc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Ecom\Implementationinformation.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bitdev.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Streamableptrarray.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Metadata.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Supplier.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Port.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Basched.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coecontrolarray.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksbfrm.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknenv.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\machine\Types.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mmfclntutility.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32keys.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Biditext.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32ktran.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Openfont.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfdatasourcesink.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfdatasource.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknquerydata.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklbo.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikvcurs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknlayout.lag
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bidi.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Supplieroutputfile.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikunder.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coemain.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32hal.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Guldef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknnumedwin.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bitmap.h
# End Source File
# Begin Source File

SOURCE=\picodrive\pico\Picoint.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Base.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mda\common\Base.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknquerydialog.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\F32file.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpanic.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknfontcategory.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Avkon.rsg
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Metadata.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtstyle.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklay.h
# End Source File
# Begin Source File

SOURCE=\picodrive\pico\Pico.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apaflrec.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikmfne.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coeccntx.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32event.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknipfed.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Uikon.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknsconstants.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32std.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Embeddedobject.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknform.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Es_sock.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mm\Mmcaf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Nifvar.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32mem.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32base.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Savenotf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32capability.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Base.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coecntrl.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikfctry.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklbm.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Assert.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Medobsrv.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknsitemid.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coecobs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknutils.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmparam.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Metadataarray.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Lafpublc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfcontrollerpluginresolver.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Audio.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Cafmimeheader.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akndialog.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Attribute.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32stor.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Attributeset.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikspmod.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmvis.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\ecom\Ecom.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklibry.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmtlay.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknsconstants.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikctgrp.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtetext.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akndoc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coeinput.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32share.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\F32file.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikmenup.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akncontrol.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32base.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coedef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcal.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fntstore.h
# End Source File
# Begin Source File

SOURCE=\picodrive\Unzip.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Rightsmanager.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apacmdln.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Es_sock.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Metacontainer.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikseced.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\F32file.h
# End Source File
# Begin Source File

SOURCE=\picodrive\zlib\Zlib.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfdatasink.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmcaf.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mdaaudiooutputstream.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikdgfty.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Cafpanic.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfutilities.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32share.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmtview.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fepbase.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Rightsinfo.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknnumed.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknsitemid.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Metadata.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Dirstreamable.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32des8.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Frmframe.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32buf.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coeaui.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikapp.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikenv.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Fbs.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akndef.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfcontroller.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknscbut.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtstyle.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikbtgrp.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpopup.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akncontrol.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Port.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\_ansi.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Bitstd.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Controller.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32def.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksrvc.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikscrlb.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\server\Mmfbuffer.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Akntouchpaneobserver.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Apadef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikbctrl.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Aknpopupheadingpane.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mmf\common\Mmfutilities.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\D32locd.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikon.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Mda\Common\Audiostream.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32page.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Lafpublc.hrh
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiklbed.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32file.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Stdarg_e.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\ecom\Ecom.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikedwob.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\Time.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32notif.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eikcycledef.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32debug.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Coetextdrawer.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Eiksrv.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32std.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Virtualpath.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Streamableptrarray.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\mda\client\Utility.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\S32mem.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\Txtfmlyr.inl
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32lmsg.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\caf\Data.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\E32des16.h
# End Source File
# Begin Source File

SOURCE=\S60V3\EPOC32\include\libc\sys\Time.h
# End Source File
# End Group
# Begin Group "Make Files"

# PROP Default_Filter "mak;mk"
# Begin Source File

SOURCE=\picodrive\s60\picodrives60v3.mak
# End Source File
# End Group
# End Target
# End Project
