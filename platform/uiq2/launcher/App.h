/*******************************************************************
 *
 *	File:		App.h
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *  Modified/adapted for picodriveN by notaz, 2006
 *
 *  (c) Copyright 2006, notaz
 *	(c) Copyright 2001, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#ifndef __APP_H
#define __APP_H

#include <coecntrl.h>
#include <coeccntx.h>
#include <coemain.h>

#include <eikappui.h>
#include <eikapp.h>
#include <eikdoc.h>

#include "Engine.h"
#include "../ClientServer.h"
#include "SimpleClient.h"
#include "picodriven.hrh"

const TUid KUidPicolApp    = { 0x1000C193 };
const TUid KUidPicolFOView = { 0x1000C194 };
const TUid KUidPicolFCView = { 0x1000C195 };

enum
{
  EScreenModeFlipOpen = 0,
  EScreenModeFlipClosed
};


//class CWsBitmap;


class CPicolDocument : public CEikDocument
{
public:
	~CPicolDocument();
	CPicolDocument(CEikApplication& aApp);
	void ConstructL();

private: // from CEikDocument
	CEikAppUi* CreateAppUiL();
};


class CEPicolAppView : public CCoeControl // , public MCoeControlBrushContext
{
public:
	~CEPicolAppView();
	void ConstructL(const TRect& aRect);

private:
	void Draw(const TRect& aRect) const;

	//CWsBitmap*	iBgImage;
	//TPoint		iImagePosition;
};


class CPicolViewBase : public CBase, public MCoeView
{
 public:

  CPicolViewBase(CEPicolAppView& aAppViewCtl) : iAppViewCtl(aAppViewCtl) {}
  //~CPicolViewBase();
  
 protected:		

  // implements MCoeView:
  virtual void ViewActivatedL(const TVwsViewId& /*aPrevViewId*/, TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/);
  virtual void ViewDeactivated();
  //virtual void ViewConstructL();
  
  CEPicolAppView& iAppViewCtl;
};


class CPicolFOView : public CPicolViewBase
{
 public:

  CPicolFOView(CEPicolAppView& aAppViewCtl) : CPicolViewBase(aAppViewCtl) {}
  //~CPicolFOView();
  virtual TVwsViewId ViewId() const;
  virtual TVwsViewIdAndMessage ViewScreenDeviceChangedL();
  virtual TBool ViewScreenModeCompatible(TInt aScreenMode);
  virtual void ViewActivatedL(const TVwsViewId& /*aPrevViewId*/, TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/);
};


class CPicolFCView : public CPicolViewBase
{
 public:

  CPicolFCView(CEPicolAppView& aAppViewCtl) : CPicolViewBase(aAppViewCtl) {}
  //~CPicolFCView();
  virtual TVwsViewId ViewId() const;
  virtual TVwsViewIdAndMessage ViewScreenDeviceChangedL();
  virtual TBool ViewScreenModeCompatible(TInt aScreenMode);
  virtual void ViewActivatedL(const TVwsViewId& /*aPrevViewId*/, TUid /*aCustomMessageId*/, const TDesC8& /*aCustomMessage*/);
};



class CPicolAppUi : public CEikAppUi, public MGameWatcher
{
public:
	CPicolAppUi();
	void ConstructL();
	~CPicolAppUi();

public:			// implements MGameWatcher
	void NotifyEmuDeath();
	void NotifyForcedExit();

	TBool EmuRunning() const;

protected: // from CEikAppUi
	void HandleCommandL(TInt aCommand);
	void DynInitMenuPaneL(TInt aResourceId,CEikMenuPane* aMenuPane);
	void HandleSystemEventL(const TWsEvent& aEvent);

protected:		// new stuf
	void DisplayAboutDialogL();
	void DisplayOpenROMDialogL();
	void DisplayConfigDialogL();
	void DisplayDebugDialogL();

	void StopGame();
	void RunGameL();
	void SendConfig();
	void RetrieveConfig();

	CGameRunner*			iGameRunner;
	CExitForcer*			iExitForcer;		// makes sure emu process exits
	CThreadWatcher*			iThreadWatcher1;	// emu process helper thread watcher
    RServSession			ss;

private:
	//void HandleScreenDeviceChangedL();
	//void HandleWsEventL(const TWsEvent& aEvent, CCoeControl* aDestination);
	virtual void HandleApplicationSpecificEventL(TInt aType, const TWsEvent& aEvent);

private:
	TBool				iQuitting;
	TBool				iROMLoaded;
	TBool				iEmuRunning;
	TBool				iAfterConfigDialog;
	TTime				iConfigDialogClosed;
	TPicoConfig			iCurrentConfig;
	TPLauncherConfig	iCurrentLConfig;

	CEPicolAppView*		iAppView;
	CPicolFOView*		iFOView;
	CPicolFCView*		iFCView;
};



class CPicolApplication : public CEikApplication
{
private: // from CApaApplication
	CApaDocument* CreateDocumentL();
	TUid AppDllUid() const;
};

#endif
