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

#include "app.h"
// #include "picodriven.mbg" // bitmap identifiers
#include "picodriven.rsg"
#include <eikenv.h>
#include <qbtselectdlg.h>
//#include <gulutil.h>
//#include <bautils.h>
#include <eikmenub.h> // CEikMenuBar
#include <apgtask.h> // TApaSystemEvent

#include "Dialogs.h"


CApaDocument* CPicolApplication::CreateDocumentL()
{
	return new (ELeave) CPicolDocument(*this);
}


CPicolDocument::CPicolDocument(CEikApplication& aApp)
		: CEikDocument(aApp)
{
}

CPicolDocument::~CPicolDocument()
{
}

CEikAppUi* CPicolDocument::CreateAppUiL()
{
	return new(ELeave) CPicolAppUi;
}


////////////////////////////////////////////////////////////////
//
// class CPicolAppUi
//
////////////////////////////////////////////////////////////////

CPicolAppUi::CPicolAppUi()
: iCurrentLConfig(iCurrentConfig)
{
	// set default config
	Mem::FillZ(&iCurrentConfig, sizeof(iCurrentConfig));
	iCurrentConfig.iFlags = 1; // use_sram
	iCurrentConfig.iFrameskip		= TPicoConfig::PFSkipAuto;
	iCurrentConfig.iScreenRotation	= TPicoConfig::PRot90;
}

CPicolAppUi::~CPicolAppUi()
{
	delete iAppView;
	DeregisterView(*iFOView);
	delete iFOView;
	DeregisterView(*iFCView);
	delete iFCView;
}

void CPicolAppUi::ConstructL()
{
	BaseConstructL();

	// load config
	iCurrentLConfig.Load();

	iAppView=new(ELeave) CEPicolAppView;
	iAppView->ConstructL(ClientRect());

	iFOView=new(ELeave) CPicolFOView(*iAppView);
	RegisterViewL(*iFOView);  
	iFCView=new(ELeave) CPicolFCView(*iAppView);
	RegisterViewL(*iFCView);
}


void CPicolAppUi::HandleCommandL(TInt aCommand)
{
	TInt oldFrameskip = iCurrentConfig.iFrameskip;
	TInt res;

	// give time for config dialog destruction
	if(iAfterConfigDialog) {
		iAfterConfigDialog = EFalse;
		TTime now; now.UniversalTime();
		if(now.MicroSecondsFrom(iConfigDialogClosed).Int64().Low() < 2500*1000)
			User::After(2500*1000-now.MicroSecondsFrom(iConfigDialogClosed).Int64().Low());
	}

	switch (aCommand)
	{
		case EEikCmdPicoLoadState:
			if(iGameRunner) {
				CEikonEnv::Static()->BusyMsgL(_L("Loading State"));
				res = ss.SendReceive(PicoMsgLoadState, 0);
				CEikonEnv::Static()->BusyMsgCancel();
				// emu doesn't start to run if load fails, so we can display this
				if(res) CEikonEnv::Static()->InfoMsg(_L("Load Failed"));
			}
			break;

		case EEikCmdPicoSaveState:
			if(iGameRunner) {
				CEikonEnv::Static()->BusyMsgL(_L("Saving State"));
				res = ss.SendReceive(PicoMsgSaveState, 0);
				CEikonEnv::Static()->BusyMsgCancel();
				if(res) CEikonEnv::Static()->InfoMsg(_L("Save Failed"));
			}
			break;

		case EEikCmdPicoLoadROM:
			DisplayOpenROMDialogL();
			break;

		case EEikCmdPicoResume:
			ss.Send(PicoMsgResume, 0);
			iEmuRunning = ETrue;
			break;

		case EEikCmdPicoReset: 
			ss.Send(PicoMsgReset, 0);
			iEmuRunning = ETrue;
			break;

		case EEikCmdPicoKeys:
			if(!iGameRunner) RunGameL();
			ss.Send(PicoMsgKeys, 0);
			iEmuRunning = ETrue;
			break;
		
		case EEikCmdPicoSettings:
			DisplayConfigDialogL();
			break;

		case EEikCmdHelpAbout: // EEikCmdPicoAbout:
			DisplayAboutDialogL();
			break;

		// standard identifier must be used here, TApaTask::EndTask() and probably others send it
		case EEikCmdExit: // EEikCmdPicoExit:
			if(iGameRunner) {
				iQuitting = ETrue;
				iExitForcer = CExitForcer::NewL(*this, 2000);
				ss.Send(PicoMsgQuit, 0);
			} else {
				iCurrentLConfig.Save();
				DEBUGPRINT(_L("[app] Exit (menu)"));
				Exit();
			}
			break;

		// frameskips
		case EEikCmdPicoFrameskipAuto:
			iCurrentConfig.iFrameskip = TPicoConfig::PFSkipAuto;
			break;

		case EEikCmdPicoFrameskip0:
			iCurrentConfig.iFrameskip = TPicoConfig::PFSkip0;
			break;

		case EEikCmdPicoFrameskip1:
			iCurrentConfig.iFrameskip = 1;
			break;

		case EEikCmdPicoFrameskip2:
			iCurrentConfig.iFrameskip = 2;
			break;

		case EEikCmdPicoFrameskip4:
			iCurrentConfig.iFrameskip = 4;
			break;

		case EEikCmdPicoFrameskip8:
			iCurrentConfig.iFrameskip = 8;
			break;

		case EEikCmdPicoDebugKillEmu:
			if(iGameRunner) {
				iExitForcer = CExitForcer::NewL(*this, 4000);
				ss.Send(PicoMsgQuit, 0);
			}
			break;

		case EEikCmdPicoDebugInfo:
			if(iGameRunner)
				DisplayDebugDialogL();
			break;
	}

	// send config update if needed
	if(iCurrentConfig.iFrameskip != oldFrameskip)
		SendConfig();
}


void CPicolAppUi::HandleSystemEventL(const TWsEvent& aEvent)
{
    TApaSystemEvent event;
    event = *(TApaSystemEvent*)aEvent.EventData();
	 
    if(event == EApaSystemEventBroughtToForeground) // application brought to foreground
    {
		DEBUGPRINT(_L("[app] EApaSystemEventBroughtToForeground, iEmuRunning=%i"), iEmuRunning);
		// we might have missed flip open event (when moved to background),
		// so make sure we have correct view active
		if(iCoeEnv->ScreenDevice()->CurrentScreenMode() == EScreenModeFlipOpen) {
			ActivateViewL(TVwsViewId(KUidPicolApp, KUidPicolFOView));
			return;
		}
	}
    if(event == EApaSystemEventShutdown)
    {
		DEBUGPRINT(_L("[app] EApaSystemEventShutdown"));
	}

	CEikAppUi::HandleSystemEventL(aEvent);
}


// called just before the menu is shown
void CPicolAppUi::DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
{
	if(aResourceId == R_APP_EMU_MENU) {
		TBool dimmed = !iGameRunner || !iROMLoaded;
		aMenuPane->SetItemDimmed(EEikCmdPicoLoadState, dimmed);
		aMenuPane->SetItemDimmed(EEikCmdPicoSaveState, dimmed);
		aMenuPane->SetItemDimmed(EEikCmdPicoResume, dimmed);
		aMenuPane->SetItemDimmed(EEikCmdPicoReset,  dimmed);
	} else if(aResourceId == R_APP_FRAMESKIP_MENU) {
		TInt itemToCheck = EEikCmdPicoFrameskipAuto;
		switch(iCurrentConfig.iFrameskip) {
			case 0: itemToCheck = EEikCmdPicoFrameskip0; break;
			case 1: itemToCheck = EEikCmdPicoFrameskip1; break;
			case 2: itemToCheck = EEikCmdPicoFrameskip2; break;
			case 4: itemToCheck = EEikCmdPicoFrameskip4; break;
			case 8: itemToCheck = EEikCmdPicoFrameskip8; break;
		}
		aMenuPane->SetItemButtonState(itemToCheck, EEikMenuItemSymbolOn);
	}
}


void CPicolAppUi::DisplayAboutDialogL()
{
	CEikDialog*	dialog = new(ELeave) CAboutDialog;
	TInt iButtonRes = dialog->ExecuteLD(R_DIALOG_ABOUT);
	if(iButtonRes == EEikBidYes) {
		CCreditsDialog *creditsDialog = new (ELeave) CCreditsDialog();
		creditsDialog->iMessageResourceID = R_TBUF_CREDITS;
		creditsDialog->ExecuteLD(R_DIALOG_CREDITS);
	}
}


void CPicolAppUi::DisplayOpenROMDialogL()
{

	TFileName file(iCurrentLConfig.iLastROMFile);
	CEikDialog* dialog = new(ELeave) CEikFileOpenDialog(&file);
	//((CEikFileOpenDialog *)dialog)->SetRequiredExtension(&ext);

	if(dialog->ExecuteLD(R_EIK_DIALOG_FILE_OPEN) == EEikBidOk) {
		CEikonEnv::Static()->BusyMsgL(_L("Loading ROM"));

		// start emu process if it is not running
		if(!iGameRunner) RunGameL();
		iROMLoaded = EFalse;

		TBuf8<KMaxFileName> file8;
		file8.Copy(file);
		TAny *p[KMaxMessageArguments];
		p[0]= (TAny*)(&file8);
		TInt res = ss.SendReceive(PicoMsgLoadROM, &p[0]);

		CEikonEnv::Static()->BusyMsgCancel();

		if(res == 1)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to open file."));
		else if(res == 2)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to allocate memory."));
		else if(res == 3)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("The file you selected is not a game ROM."));
		else if(res == 4)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("No game ROMs found in zipfile."));
		else if(res == 5)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed while unzipping ROM."));
		else if(res < 0)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to send request to emu process."));
		else {
			iROMLoaded = ETrue;
			iEmuRunning = ETrue;
		}

		// sound errors which leave ROM loaded
		if(res == 6)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to allocate sound buffer, disabled sound."));
		else if(res == 7)
			CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to start soundsystem, disabled sound."));
		if(res == 6 || res == 7) iCurrentConfig.iFlags &= ~4;

		iCurrentLConfig.iLastROMFile.Copy(file);
	}
}


void CPicolAppUi::DisplayConfigDialogL()
{
	CPicoConfigDialog* configDialog = new(ELeave)CPicoConfigDialog(iCurrentConfig, iCurrentLConfig);
	configDialog->ExecuteLD(R_PICO_CONFIG);

	if(iGameRunner)
		SendConfig();

	iCurrentLConfig.Save();

	// configDialog seems to be actually destroyed later after returning,
	// and this usually happens just after resuming game and causes emu slowdowns :/
	iAfterConfigDialog = ETrue;
	iConfigDialogClosed.UniversalTime();
}


void CPicolAppUi::DisplayDebugDialogL()
{
	// first get our debug info
	char dtxt[1024];

	TAny *p[KMaxMessageArguments];
	TPtr8 descr((TUint8*) dtxt, sizeof(dtxt));
	p[0]= (TAny*)(&descr);
	ss.SendReceive(PicoMsgRetrieveDebugStr, &p[0]);

	CEikDialog*	dialog = new(ELeave) CDebugDialog(dtxt);
	dialog->ExecuteLD(R_DIALOG_DEBUG);
}


void CPicolAppUi::SendConfig()
{
	// send config
	if(iGameRunner) {
		TAny *p[KMaxMessageArguments];
		TPtrC8 descr((TUint8*) &iCurrentConfig, sizeof(iCurrentConfig));
		p[0]= (TAny*)(&descr);
		ss.Send(PicoMsgConfigChange, &p[0]);
	}
}


// get config from emu proc
void CPicolAppUi::RetrieveConfig()
{
	// ask to configure keys and receive new config
	TAny *p[KMaxMessageArguments];
	TPtr8 descr((TUint8*) &iCurrentConfig, sizeof(iCurrentConfig));
	p[0]= (TAny*)(&descr);
	ss.SendReceive(PicoMsgRetrieveConfig, &p[0]);

	iCurrentLConfig.Save();
}


void CPicolAppUi::NotifyEmuDeath()
{
	StopGame();
	if(iQuitting) {
		DEBUGPRINT(_L("[app] Exit (NotifyEmuDeath)"));
		iCurrentLConfig.Save();
		RProcess me;
		me.Terminate(0);
	}
}


void CPicolAppUi::NotifyForcedExit()
{
	DEBUGPRINT(_L("[app] Exit (NotifyForcedExit)"));
	StopGame();
	RProcess me;
	me.Terminate(0);
}


TBool CPicolAppUi::EmuRunning() const
{
	return iEmuRunning;
}


void CPicolAppUi::StopGame()
{
	// in case we have busyMsg and process crashes
	CEikonEnv::Static()->BusyMsgCancel();

	ss.Close();
	if(iGameRunner) delete iGameRunner;
	iGameRunner = NULL;
	if(iExitForcer) delete iExitForcer;
	iExitForcer = NULL;
	if(iThreadWatcher1) delete iThreadWatcher1;
	iThreadWatcher1 = NULL;
	iROMLoaded  = EFalse;
	iEmuRunning = EFalse;
}


void CPicolAppUi::RunGameL()
{
	TInt res = KErrNone;

	// do one connection attemt to emu and ask it to quit if we succeed
	res = ss.Connect();
	if(res == KErrNone) {
		ss.Send(PicoMsgQuit, 0);
		ss.Close();
	}

	iGameRunner = CGameRunner::NewL(*this);

	// Connect to the server
	// we don't want to do complex asynchronous stuff here, so we just
	// wait and do several connection attempts
	User::After(200000);
	for(TInt attempt=0; attempt < 10; attempt++) {
		res = ss.Connect();
		if(res == KErrNone) break;
		User::After(200000);
	}

	if(res != KErrNone) {
		CEikonEnv::Static()->BusyMsgCancel();
		CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to communicate with the emulation process."));
		StopGame();
		RProcess me;
		me.Terminate(2);
	}

	// now we are successfully connected, that means emu process' helper-communication thread is running.
	// we have to keep an eye on it too, because if it crashes, symbian OS leaves it's process
	// alive, but it becomes useless without it's communication thread so we have to detect it's death.
	iThreadWatcher1 = CThreadWatcher::NewL(*this, KServerName);

	// send initial config
	SendConfig();
}


/*
void CPicolAppUi::HandleScreenDeviceChangedL()
{
	// does not receive when emu is in foreground
	if(iCoeEnv->ScreenDevice()->CurrentScreenMode() == 0) { // flip open
		// regain focus
		//iCoeEnv->BringOwnerToFront();

	}
//	ss.Send(PicoMsgFlipChange, 0);
}
*/

void CPicolAppUi::HandleApplicationSpecificEventL(TInt aType, const TWsEvent& aEvent)
{
	DEBUGPRINT(_L("[app] event from server: %i"), aEvent.Type());

	switch (aEvent.Type())
	{
		case EEventKeyCfgDone:
			RetrieveConfig();
			break;
		
		case EEventGamePaused:
			iEmuRunning = EFalse;
			break;
	}
}

////////////////////////////////////////////////////////////////
//
// class CEPicolAppView
//
////////////////////////////////////////////////////////////////

void CEPicolAppView::ConstructL(const TRect& aRect)
{
	CreateWindowL();
	SetRect(aRect);
	ActivateL();

		/*
		 * Load background image
		 */
/*
	TBuf<1> name = _L("*");
	TRAPD(err, iBgImage = CEikonEnv::Static()->CreateBitmapL(name, EMbmEdoomDoom));
	if (iBgImage)
	{
		iImagePosition.iX = (aRect.Size().iWidth - iBgImage->SizeInPixels().iWidth) / 2;
		iImagePosition.iY = (aRect.Size().iHeight - iBgImage->SizeInPixels().iHeight) / 2;
	}
*/
}

CEPicolAppView::~CEPicolAppView()
{
	//if (iBgImage) delete iBgImage;
}


void CEPicolAppView::Draw(const TRect& aRect) const
{
	CWindowGc& gc = SystemGc();

	//if (iBgImage)
	//{
	//	gc.DrawBitmap(iImagePosition, iBgImage);
	//	DrawUtils::ClearBetweenRects(gc, Rect(), TRect(iImagePosition, iBgImage->SizeInPixels()));
	//}
	//else
		gc.Clear();//aRect);
}


////////////////////////////////////////////////////////////////
//
// class CPicolViewBase
//
////////////////////////////////////////////////////////////////

void CPicolViewBase::ViewActivatedL(const TVwsViewId& /*aPrevViewId*/, TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/)
{
	TPixelsAndRotation sizeAndRotation;
	CEikonEnv::Static()->ScreenDevice()->GetDefaultScreenSizeAndRotation(sizeAndRotation);
	CEikonEnv::Static()->ScreenDevice()->SetScreenSizeAndRotation(sizeAndRotation);
	//iAppViewCtl.MakeVisible(ETrue);
}

void CPicolViewBase::ViewDeactivated()
{
	//iAppViewCtl.MakeVisible(EFalse);
}


////////////////////////////////////////////////////////////////
//
// class CPicolFOView
//
////////////////////////////////////////////////////////////////

TVwsViewId CPicolFOView::ViewId() const
{
	return TVwsViewId(KUidPicolApp, KUidPicolFOView);
}

TVwsViewIdAndMessage CPicolFOView::ViewScreenDeviceChangedL()
{
	// only handle change to FC mode when emu process is running
	if(static_cast<CPicolAppUi*>(CEikonEnv::Static()->AppUi())->EmuRunning())
		 return TVwsViewIdAndMessage(TVwsViewId(KUidPicolApp, KUidPicolFCView));
	else return MCoeView::ViewScreenDeviceChangedL();
}

TBool CPicolFOView::ViewScreenModeCompatible(TInt aScreenMode)
{
	return (aScreenMode == EScreenModeFlipOpen);
}

void CPicolFOView::ViewActivatedL(const TVwsViewId& aPrevViewId, TUid aCustomMessageId, const TDesC8& aCustomMessage)
{
	DEBUGPRINT(_L("[app] FO"));
	CPicolViewBase::ViewActivatedL(aPrevViewId, aCustomMessageId, aCustomMessage);
	CEikonEnv::Static()->AppUiFactory()->MenuBar()->MakeVisible(ETrue);

	iAppViewCtl.SetRect(static_cast<CEikAppUi*>(CEikonEnv::Static()->AppUi())->ClientRect());
}


////////////////////////////////////////////////////////////////
//
// class CPicolFCView
//
////////////////////////////////////////////////////////////////

TVwsViewId CPicolFCView::ViewId() const
{
	return TVwsViewId(KUidPicolApp, KUidPicolFCView);
}

TVwsViewIdAndMessage CPicolFCView::ViewScreenDeviceChangedL()
{
	return TVwsViewIdAndMessage(TVwsViewId(KUidPicolApp, KUidPicolFOView));
}

TBool CPicolFCView::ViewScreenModeCompatible(TInt aScreenMode)
{
	return (aScreenMode == EScreenModeFlipClosed);
}

void CPicolFCView::ViewActivatedL(const TVwsViewId& aPrevViewId, TUid aCustomMessageId, const TDesC8& aCustomMessage)
{
	DEBUGPRINT(_L("[app] FC"));
	CPicolViewBase::ViewActivatedL(aPrevViewId, aCustomMessageId, aCustomMessage);
	CEikonEnv::Static()->AppUiFactory()->MenuBar()->MakeVisible(EFalse);
	//iAppViewCtl.ChangeLayout(ETrue);
	iAppViewCtl.SetRect(CEikonEnv::Static()->ScreenDevice()->SizeInPixels());
}


////////////////////////////////////////////////////////////////
//
// framework
//
////////////////////////////////////////////////////////////////

GLDEF_C TInt E32Dll(TDllReason)
{
	return KErrNone;
}


EXPORT_C CApaApplication* NewApplication()
{
	return new CPicolApplication;
}


TUid CPicolApplication::AppDllUid() const
{
	return KUidPicolApp;
}

