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

#include <QikSimpleDialog.h>


/************************************************
 *
 * config Dialog
 *
 ************************************************/

class TPicoConfig;

class CPicoConfigDialog : public CEikDialog
{
public:
	CPicoConfigDialog(TPicoConfig &cfg);

protected: // framework
    void PostLayoutDynInitL();
	void HandleControlStateChangeL(TInt aControlId);
	TBool OkToExitL(TInt aButtonId);

	TPicoConfig &config;
};


/************************************************
 *
 * About Dialog
 *
 ************************************************/

class CAboutDialog : public CQikSimpleDialog
{
protected:	// from CQikSimpleDialog
	void PostLayoutDynInitL();
};

/*************************************************************
*
* Credits dialog
*
**************************************************************/

class CCreditsDialog : public CQikSimpleDialog
{
protected:	// from CQikSimpleDialog
	void PreLayoutDynInitL();
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType);
};

/*************************************************************
*
* Debug dialog
*
**************************************************************/

class CDebugDialog : public CCreditsDialog
{
public:
	CDebugDialog(char *t);

protected:
	char iText[1024];
	void PreLayoutDynInitL();
};

#endif	// __DIALOGS_H
