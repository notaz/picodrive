/*******************************************************************
 *
 *	File:		App.cpp
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *  Modified/adapted for picodriveN by notaz, 2006
 *
 *  (c) Copyright 2006, notaz
 *	(c) Copyright 2002, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#include "App.h"
// #include "picodriven.mbg" // bitmap identifiers
#include "rsc/picodrive.rsg" // resource include
#include <eikenv.h>
#include <qbtselectdlg.h>
//#include <gulutil.h>
//#include <bautils.h>
//#include <eikmenub.h> // CEikMenuBar
#include <apgtask.h> // TApaSystemEvent
#include <eikstart.h>
#include <eikedwin.h>
#include <s32strm.h>

#include <qikappui.h>
#include <qikeditcategoryobserver.h>
#include <qikselectfiledlg.h>
#include <qikcommand.h>

#include "Dialogs.h"
#include "engine/debug.h"
#include "../common/emu.h"
#include "emu.h"

extern "C" char menuErrorMsg[];

////////////////////////////////////////////////////////////////
//
// class CPicolAppView
//
////////////////////////////////////////////////////////////////

// Creates and constructs the view.
CPicolAppView* CPicolAppView::NewLC(CQikAppUi& aAppUi, TPicoConfig& aCurrentConfig)
{
	CPicolAppView* self = new (ELeave) CPicolAppView(aAppUi, aCurrentConfig);
	CleanupStack::PushL(self);
	return self;
}

/**
Constructor for the view.
Passes the application UI reference to the construction of the super class.

KNullViewId should normally be passed as parent view for the applications 
default view. The parent view is the logical view that is normally activated 
when a go back command is issued. KNullViewId will activate the system 
default view. 

@param aAppUi Reference to the application UI
*/
CPicolAppView::CPicolAppView(CQikAppUi& aAppUi, TPicoConfig& aCurrentConfig) 
: CQikViewBase(aAppUi, KNullViewId), iCurrentConfig(aCurrentConfig)
{
}

void CPicolAppView::ConstructL()
{
	BaseConstructL();
}

CPicolAppView::~CPicolAppView()
{
}


/**
Inherited from CQikViewBase and called upon by the UI Framework. 
It creates the view from resource.
*/
void CPicolAppView::ViewConstructL()
{
	// Loads information about the UI configurations this view supports
	// together with definition of each view.	
	ViewConstructFromResourceL(R_APP_UI_CONFIGURATIONS);
	UpdateCommandList();
}

/**
Returns the view Id

@return Returns the Uid of the view
*/
TVwsViewId CPicolAppView::ViewId()const
{
	return TVwsViewId(KUidPicolApp, KUidPicolMainView);
}

/**
Handles all commands in the view.
Called by the UI framework when a command has been issued.
The command Ids are defined in the .hrh file.

@param aCommand The command to be executed
@see CQikViewBase::HandleCommandL
*/
void CPicolAppView::HandleCommandL(CQikCommand& aCommand)
{
	TInt res;

	switch(aCommand.Id())
	{
		case EEikCmdPicoLoadState:
			if(iROMLoaded) {
				CEikonEnv::Static()->BusyMsgL(_L("Loading State"));
				res = CPicoGameSession::Do(PicoMsgLoadState);
				CEikonEnv::Static()->BusyMsgCancel();
				// emu doesn't start to run if load fails, so we can display this
				if(res) CEikonEnv::Static()->InfoMsg(_L("Load Failed"));
			}
			break;

		case EEikCmdPicoSaveState:
			if(iROMLoaded) {
				CEikonEnv::Static()->BusyMsgL(_L("Saving State"));
				res = CPicoGameSession::Do(PicoMsgSaveState);
				CEikonEnv::Static()->BusyMsgCancel();
				if(res) CEikonEnv::Static()->InfoMsg(_L("Save Failed"));
			}
			break;

		case EEikCmdPicoLoadROM:
			DisplayOpenROMDialogL();
			DEBUGPRINT(_L("after DisplayOpenROMDialogL()"));
			break;

		case EEikCmdPicoResume:
			CPicoGameSession::Do(PicoMsgResume);
			break;

		case EEikCmdPicoReset:
			CPicoGameSession::Do(PicoMsgReset);
			break;

		case EEikCmdPicoSettings:
			DisplayConfigDialogL();
			break;

		case EEikCmdHelpAbout:
			DisplayAboutDialogL();
			break;

		case EEikCmdPicoDebugInfo:
			DisplayDebugDialogL();
			break;

		case EEikCmdPicoKeys:
			CPicoGameSession::Do(PicoMsgConfigChange, &iCurrentConfig);
			CPicoGameSession::Do(PicoMsgKeys);
			break;

		case EEikCmdPicoFrameskipAuto:
			currentConfig.Frameskip = -1;
			emu_WriteConfig(0);
			break;

		case EEikCmdPicoFrameskip0:
			currentConfig.Frameskip = 0;
			emu_WriteConfig(0);
			break;

		case EEikCmdPicoFrameskip1:
			currentConfig.Frameskip = 1;
			emu_WriteConfig(0);
			break;

		case EEikCmdPicoFrameskip2:
			currentConfig.Frameskip = 2;
			emu_WriteConfig(0);
			break;

		case EEikCmdPicoFrameskip4:
			currentConfig.Frameskip = 4;
			emu_WriteConfig(0);
			break;

		case EEikCmdPicoFrameskip8:
			currentConfig.Frameskip = 8;
			emu_WriteConfig(0);
			break;

		case EEikCmdExit:
			emu_Deinit();
			CPicoGameSession::freeResources();
			//break; // this is intentional

		default:
			// Go back and exit command will be passed to the CQikViewBase to handle.
			CQikViewBase::HandleCommandL(aCommand);
			break;
	}
}

void CPicolAppView::DisplayOpenROMDialogL()
{
	// Array of mimetypes that the dialog shall filter on, if empty all
	// mimetypes will be visible.
	CDesCArray* mimeArray = new (ELeave) CDesCArrayFlat(1);
	CleanupStack::PushL(mimeArray);
	// Array that will be filled with the file paths that are choosen
	// from the dialog. 
	CDesCArray* fileArray = new (ELeave) CDesCArraySeg(3);
	CleanupStack::PushL(fileArray);
	_LIT16(KDlgTitle, "Select a ROM file");

	TPtrC8 text8((TUint8*) loadedRomFName);
	iCurrentConfig.iLastROMFile.Copy(text8);

	if( CQikSelectFileDlg::RunDlgLD( *mimeArray, *fileArray, &KDlgTitle, &iCurrentConfig.iLastROMFile) )
	{
		CEikonEnv::Static()->BusyMsgL(_L("Loading ROM"));
		TPtrC16 file = (*fileArray)[0];
		//iCurrentConfig.iLastROMFile.Copy(file);

		// push the config first
		CPicoGameSession::Do(PicoMsgSetAppView, this);
		CPicoGameSession::Do(PicoMsgConfigChange, &iCurrentConfig);

		TInt res = CPicoGameSession::Do(PicoMsgLoadROM, &file);

		CEikonEnv::Static()->BusyMsgCancel();

		iROMLoaded = EFalse;
		switch (res)
		{
			case PicoErrRomOpenFailed: {
				TBuf<64> mErrorBuff;
				TPtrC8 buff8((TUint8*) menuErrorMsg);
				mErrorBuff.Copy(buff8);
				CEikonEnv::Static()->InfoWinL(_L("Error"), mErrorBuff);
				break;
			}

			case PicoErrOutOfMem:
				CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to allocate memory."));
				break;

			case PicoErrEmuThread:
				CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to create emulation thread. Try to restart this application."));
				break;

			default:
				iROMLoaded = ETrue;
				break;
		}

		// errors which leave ROM loaded
		switch (res)
		{
			case PicoErrOutOfMemSnd:
				CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to allocate sound buffer, disabled sound."));
				break;

			case PicoErrGenSnd:
				CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to start soundsystem, disabled sound."));
				break;
		}
		if(res == 6 || res == 7) currentConfig.EmuOpt &= ~EOPT_EN_SOUND;

		if(iROMLoaded) {
			if(iTitleAdded)
			     ViewContext()->ChangeTextL(EEikCidTitleBarLabel, CPicoGameSession::iRomInternalName);
			else ViewContext()->AddTextL   (EEikCidTitleBarLabel, CPicoGameSession::iRomInternalName);
			iTitleAdded = ETrue;
			UpdateCommandList();
		}
	}
	CleanupStack::PopAndDestroy(2, mimeArray);
}


void CPicolAppView::DisplayConfigDialogL()
{
	CPicoConfigDialog* configDialog = new(ELeave)CPicoConfigDialog(currentConfig);
	emu_packConfig();
	configDialog->ExecuteLD(R_PICO_CONFIG);
	emu_unpackConfig();
	emu_WriteConfig(0);

	CPicoGameSession::Do(PicoMsgConfigChange, &currentConfig);
}


void CPicolAppView::DisplayAboutDialogL()
{
	TInt iButtonRes;
	CAboutDialog* dialog = new (ELeave) CAboutDialog;

	dialog->PrepareLC(R_PICO_ABOUT);
	iButtonRes = dialog->RunLD();

	if(iButtonRes == EEikCmdPicoAboutCreditsCmd) {
		CCreditsDialog *creditsDialog = new (ELeave) CCreditsDialog();
		creditsDialog->PrepareLC(R_PICO_CREDITS);
		creditsDialog->RunLD();
	}
}

#ifdef __DEBUG_PRINT
extern "C" char *PDebugMain();
#endif

void CPicolAppView::DisplayDebugDialogL()
{
#ifdef __DEBUG_PRINT
	CDebugDialog* dialog = new (ELeave) CDebugDialog(PDebugMain());

	dialog->PrepareLC(R_PICO_DEBUG);
	dialog->RunLD();
#endif
}

void CPicolAppView::UpdateCommandList()
{
	CQikCommandManager& commandManager = CQikCommandManager::Static(*iCoeEnv);
	CQikCommand *cmd_fs[10];
	Mem::FillZ(cmd_fs, sizeof(CQikCommand*)*10);

	CQikCommand* cmd_reset  = commandManager.Command(*this, EEikCmdPicoReset);
	CQikCommand* cmd_savest = commandManager.Command(*this, EEikCmdPicoSaveState);
	CQikCommand* cmd_loadst = commandManager.Command(*this, EEikCmdPicoLoadState);
	CQikCommand* cmd_resume = commandManager.Command(*this, EEikCmdPicoResume);
	cmd_fs[0]  = commandManager.Command(*this, EEikCmdPicoFrameskipAuto);
	cmd_fs[1]  = commandManager.Command(*this, EEikCmdPicoFrameskip0);
	cmd_fs[2]  = commandManager.Command(*this, EEikCmdPicoFrameskip1);
	cmd_fs[3]  = commandManager.Command(*this, EEikCmdPicoFrameskip2);
	cmd_fs[5]  = commandManager.Command(*this, EEikCmdPicoFrameskip4);
	cmd_fs[9]  = commandManager.Command(*this, EEikCmdPicoFrameskip8);

	TBool dimmed = !CPicoGameSession::iEmuRunning || !iROMLoaded;
	cmd_reset ->SetDimmed(dimmed);
	cmd_savest->SetDimmed(dimmed);
	cmd_loadst->SetDimmed(dimmed);
	cmd_resume->SetDimmed(dimmed);

	// frameskip
	TInt fs_index = currentConfig.Frameskip + 1;
	if (fs_index >= 0 && fs_index < 10 && cmd_fs[fs_index])
	{
		cmd_fs[fs_index]->SetChecked(ETrue);
	}
}


////////////////////////////////////////////////////////////////
//
// class CPicolAppUi
//
////////////////////////////////////////////////////////////////


void CPicolAppUi::ConstructL()
{
	BaseConstructL();

	// Create the view and add it to the framework
	iAppView = CPicolAppView::NewLC(*this, ((CPicolDocument *)Document())->iCurrentConfig);
	AddViewL(*iAppView);
	CleanupStack::Pop(iAppView);
}


////////////////////////////////////////////////////////////////
//
// CPicolDocument
//
////////////////////////////////////////////////////////////////


CPicolDocument::CPicolDocument(CQikApplication& aApp)
: CQikDocument(aApp)
{
}

CQikAppUi* CPicolDocument::CreateAppUiL()
{
	return new(ELeave) CPicolAppUi;
}

/**
Called by the framework when ::SaveL has been called.
*/
void CPicolDocument::StoreL(CStreamStore& aStore, CStreamDictionary& aStreamDic) const
{
#if 0
	RStoreWriteStream stream;

	TStreamId preferenceId = stream.CreateLC(aStore);
	aStreamDic.AssignL(KUidPicolStore, preferenceId);

	// Externalize preference
	stream << iCurrentConfig;

	// Ensures that any buffered data is written to aStore
	stream.CommitL();
	CleanupStack::PopAndDestroy(); // stream
#endif
/*
	// tmp
	TInt res;
	RFile logFile;
	res = logFile.Replace(CEikonEnv::Static()->FsSession(), _L("C:\\Shared\\pico.cfg"), EFileWrite|EFileShareAny);
	if(!res) {
		logFile.Write(TPtr8((TUint8 *)&iCurrentConfig, sizeof(iCurrentConfig), sizeof(iCurrentConfig)));
		logFile.Close();
	}
*/
}

/**
Called by the framework on application start.
Loads the application data from disk, i.e. domain data and preferences.
*/
void CPicolDocument::RestoreL(const CStreamStore& aStore, const CStreamDictionary& aStreamDic)
{ 
#if 0
	// Find the stream ID of the model data from the stream dictionary:
	TStreamId preferenceId(aStreamDic.At(KUidPicolStore));
	RStoreReadStream stream;
	stream.OpenLC(aStore, preferenceId);
	if(preferenceId != KNullStreamId)
	{
		// Interalize preference and model
		stream >> iCurrentConfig;
	}

	CleanupStack::PopAndDestroy(); // stream
#endif

	// tmp
/*	TInt res;
	RFile logFile;
	res = logFile.Open(CEikonEnv::Static()->FsSession(), _L("C:\\Shared\\pico.cfg"), EFileRead|EFileShareAny);
	if(!res) {
		TPtr8 ptr((TUint8 *)&iCurrentConfig, sizeof(iCurrentConfig), sizeof(iCurrentConfig));
		logFile.Read(ptr);
		logFile.Close();
	}*/
}

////////////////////////////////////////////////////////////////
//
// framework
//
////////////////////////////////////////////////////////////////


CApaDocument* CPicolApplication::CreateDocumentL()
{
	return new (ELeave) CPicolDocument(*this);
}

EXPORT_C CApaApplication* NewApplication()
{
	return new CPicolApplication;
}


TUid CPicolApplication::AppDllUid() const
{
	return KUidPicolApp;
}


extern "C" TInt my_SetExceptionHandler(TInt, TExceptionHandler, TUint32);

GLDEF_C TInt E32Main()
{
	// doesn't work :(
	User::SetExceptionHandler(ExceptionHandler, (TUint32) -1);
//	my_SetExceptionHandler(KCurrentThreadHandle, ExceptionHandler, 0xffffffff);

	emu_Init();

	return EikStart::RunApplication(NewApplication);
}


