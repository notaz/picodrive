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

#include <qikappui.h>
#include <QikApplication.h>
#include <QikViewBase.h>
//#include <eikapp.h>
#include <QikDocument.h>

#include "Engine.h"
#include "picodrive.hrh"

const TUid KUidPicolApp      = { 0xA00010F3 };
const TUid KUidPicolMainView = { 0x00000001 };
//const TUid KUidPicolFOView = { 0x1000C194 };
//const TUid KUidPicolFCView = { 0x1000C195 };
const TUid KUidPicolStore    = { 0x00000011 }; // store stream UID

//enum
//{
//  EScreenModeFlipOpen = 0,
//  EScreenModeFlipClosed
//};



class CPicolAppView : public CQikViewBase
{
public:
	static CPicolAppView* NewLC(CQikAppUi& aAppUi, TPicoConfig& aCurrentConfig);
	~CPicolAppView();

	// from CQikViewBase
	TVwsViewId ViewId()const;
	void HandleCommandL(CQikCommand& aCommand);
	void UpdateCommandList();

protected: 
	// from CQikViewBase
	void ViewConstructL();
	
private:
	CPicolAppView(CQikAppUi& aAppUi, TPicoConfig& aCurrentConfig);
	void ConstructL();

protected:		// new stuf
	void DisplayAboutDialogL();
	void DisplayOpenROMDialogL();
	void DisplayConfigDialogL();
	void DisplayDebugDialogL();

/*	void StopGame();
	void RunGameL();*/

private:
	TPicoConfig&		iCurrentConfig;
	TBool				iROMLoaded;
	TBool				iTitleAdded;
};



class CPicolAppUi : public CQikAppUi
{
public:
//	CPicolAppUi();
	void ConstructL();

	CPicolAppView*		iAppView;
};


class CPicolDocument : public CQikDocument
{
public:
	CPicolDocument(CQikApplication& aApp);
	void StoreL(CStreamStore& aStore, CStreamDictionary& aStreamDic) const;
	void RestoreL(const CStreamStore& aStore, const CStreamDictionary& aStreamDic);

	TPicoConfig			iCurrentConfig;

private: // from CQikDocument
	CQikAppUi* CreateAppUiL();
};


class CPicolApplication : public CQikApplication
{
private: // from CApaApplication
	CApaDocument* CreateDocumentL();
	TUid AppDllUid() const;
};

#endif
