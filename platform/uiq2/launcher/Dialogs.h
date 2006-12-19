/*******************************************************************
 *
 *	File:		Dialogs.h
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

#ifndef __DIALOGS_H
#define __DIALOGS_H

#include <eikenv.h>
#include <eikdialg.h>
#include <eiktxlbx.h>
#include <eiktxlbm.h>
#include <eikdlgtb.h>
#include <eiklabel.h>
#include <eikchlst.h>
#include <eikchkbx.h>
#include <eikedwob.h>

#include "../ClientServer.h"

class CRichText;
class CEikRichTextEditor;
class TPLauncherConfig;

/************************************************
 *
 * CSimpleTextInfo Dialog
 *
 ************************************************/

class CSimpleTextInfoDialog : public CEikDialog, public MEikEdwinSizeObserver
{
public:
	CSimpleTextInfoDialog(TInt aTextIdOne = -1, TInt aRichTextCtlIdOne = -1, 
		TInt aTextIdTwo = -1, TInt aRichTextCtlIdTwo = -1,
		TBool aSimpleTextResIdOneIsArray = EFalse, TBool aSimpleTextResIdTwoIsArray = EFalse
		);
	void SetDialogBackground(TBool aEnable){iSetDialogBackground=aEnable;}
	void WantVertScrollbar(TBool aEnable){iWantVertScrollbar=aEnable;}
public:			// implements MEikEdwinSizeObserver
	virtual TBool HandleEdwinSizeEventL(CEikEdwin* aEdwin, TEdwinSizeEvent aEventType, TSize aDesirableEdwinSize);

protected: // framework
    void PreLayoutDynInitL();
	void PostLayoutDynInitL();

protected:	// new stuff
	virtual void ShowTextL(CRichText& aRichText, TInt aRichTextCtlId, TInt aResId);
	virtual void PreLayoutDynInitRichTextL(CEikRichTextEditor& aRichTextEditor, TInt aRichTextCtlId, TInt aResId);

	void ShowSimpleTextL(const TDesC& aSimpleText, CRichText& aRichText);

	TInt	iSimpleTextResIdOne;
	TInt	iSimpleTextResIdTwo;
	TInt	iRichTextCtlIdOne;
	TInt	iRichTextCtlIdTwo;
	TBool	iSimpleTextResIdOneIsArray;
	TBool	iSimpleTextResIdTwoIsArray;
	TBool	iSetDialogBackground;
	TBool	iWantVertScrollbar;
};


/************************************************
 *
 * config Dialog
 *
 ************************************************/

class CPicoConfigDialog : public CEikDialog
{
public:
	CPicoConfigDialog(TPicoConfig &cfg, TPLauncherConfig &cfgl);

protected: // framework
    void PostLayoutDynInitL();
	void HandleControlStateChangeL(TInt aControlId);
	TBool OkToExitL(TInt aButtonId);

	TPicoConfig &config;
	TPLauncherConfig &config_l;
};


/************************************************
 *
 * About Dialog
 *
 ************************************************/

class CAboutDialog : public CSimpleTextInfoDialog
{
public:
	CAboutDialog();
protected:	// from CSimpleTextInfoDialog
	virtual void ShowTextL(CRichText& aRichText, TInt aRichTextCtlId, TInt aResId);
};

/*************************************************************
*
* Credits dialog
*
**************************************************************/

class CCreditsDialog : public CEikDialog
{
public:
	TInt iMessageResourceID;
	
protected:
	void PreLayoutDynInitL();
	void PostLayoutDynInitL();
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType);
};

/*************************************************************
*
* Debug dialog
*
**************************************************************/

class CDebugDialog : public CEikDialog
{
public:
	CDebugDialog(char *t);

protected:
	char iText[1024];
	void PreLayoutDynInitL();
	void PostLayoutDynInitL();
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType);
};



#endif	// __DIALOGS_H
