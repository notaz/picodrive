/*******************************************************************
 *
 *	File:		Dialogs.cpp
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

#include "Dialogs.h"
#include "Engine.h"
#include "picodriven.hrh"
#include "picodriven.rsg"

#include "../version.h"
#include "CSimpleTextParser.h"
#include <txtrich.h>				// CRichText
#include <eikrted.h>				// CEikRichTextEditor
#include <qikvertoptionbuttonlist.h> // CEikHorOptionButtonList
#include <eikopbut.h>   // CEikOptionButton
#include <QuartzKeys.h> // EQuartzKeyTwoWayDown


/************************************************
 *
 * config Dialog
 *
 ************************************************/

CPicoConfigDialog::CPicoConfigDialog(TPicoConfig &cfg, TPLauncherConfig &cfgl) : config(cfg), config_l(cfgl)
{
}

void CPicoConfigDialog::PostLayoutDynInitL()
{
	CEikHorOptionButtonList *buttons_rot   = (CEikHorOptionButtonList*) Control( ECtlOptRotation );
	CEikHorOptionButtonList *buttons_disp  = (CEikHorOptionButtonList*) Control( ECtlOptScreenMode );
	CEikCheckBox            *chkbox_altrend= (CEikCheckBox*)            Control( ECtlOptUseAltRend );
	CEikCheckBox            *chkbox_acctmng= (CEikCheckBox*)            Control( ECtlOptUseAccTiming );
	CEikCheckBox            *chkbox_sram   = (CEikCheckBox*)            Control( ECtlOptUseSRAM );
	CEikCheckBox            *chkbox_fps    = (CEikCheckBox*)            Control( ECtlOptShowFPS );
	CEikCheckBox            *chkbox_sound  = (CEikCheckBox*)            Control( ECtlOptEnableSound );
	CEikCheckBox            *chkbox_z80    = (CEikCheckBox*)            Control( ECtlOptEmulateZ80 );
	CEikCheckBox            *chkbox_ym2612 = (CEikCheckBox*)            Control( ECtlOptEmulateYM2612 );
	CEikCheckBox            *chkbox_sn76496= (CEikCheckBox*)            Control( ECtlOptEmulateSN76496 );
	CEikChoiceListBase      *combo_sndq    = (CEikChoiceListBase*)      Control( ECtlOptSndQuality );
	CEikCheckBox            *chkbox_6bpad  = (CEikCheckBox*)            Control( ECtlOpt6ButtonPad );
	CEikCheckBox            *chkbox_gzipst = (CEikCheckBox*)            Control( ECtlOptGzipStates );
	CEikCheckBox            *chkbox_motvol = (CEikCheckBox*)            Control( ECtlOptMotDontUseVol );
	CEikCheckBox            *chkbox_accsprt= (CEikCheckBox*)            Control( ECtlOptUseAccSprites );
	CEikChoiceListBase      *combo_region  = (CEikChoiceListBase*)      Control( ECtlOptRegion );
	CEikOptionButton        *opt_fit2      = (CEikOptionButton*)        buttons_disp->ComponentControl( TPicoConfig::PMFit2 );

	buttons_rot ->SetButtonById(ECtlOptRotation0 + config.iScreenRotation);
	buttons_disp->SetButtonById(ECtlOptScreenModeCenter + config.iScreenMode);
	chkbox_sram   ->SetState(config.iFlags & 1     ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_fps    ->SetState(config.iFlags & 2     ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_sound  ->SetState(config.iFlags & 4     ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_motvol ->SetState(config.iFlags & 0x40  ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_gzipst ->SetState(config.iFlags & 0x80  ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_z80    ->SetState(config.iPicoOpt & 4   ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_ym2612 ->SetState(config.iPicoOpt & 1   ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_sn76496->SetState(config.iPicoOpt & 2   ? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_altrend->SetState(config.iPicoOpt & 0x10? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_6bpad  ->SetState(config.iPicoOpt & 0x20? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_acctmng->SetState(config.iPicoOpt & 0x40? CEikButtonBase::ESet : CEikButtonBase::EClear);
	chkbox_accsprt->SetState(config.iPicoOpt & 0x80? CEikButtonBase::ESet : CEikButtonBase::EClear);

	// hide "fit2" if we are not in 0 or 180 mode
	if(config.iScreenRotation != TPicoConfig::PRot0 && config.iScreenRotation != TPicoConfig::PRot180) opt_fit2->MakeVisible(EFalse);
	// dim some stuff for alternative renderer
	if(config.iPicoOpt & 0x10) {
		buttons_disp->SetDimmed(ETrue);
		((CEikOptionButton*)(buttons_rot->ComponentControl(TPicoConfig::PRot0)))->SetDimmed(ETrue);
		((CEikOptionButton*)(buttons_rot->ComponentControl(TPicoConfig::PRot180)))->SetDimmed(ETrue);
	}
	// dim accurate sprites
	if(config.iPicoOpt & 0x10) {
		chkbox_accsprt->SetState(CEikButtonBase::EClear);
		chkbox_accsprt->SetDimmed(ETrue);
	}

	TInt sel = (config.iPicoOpt&8) ? 4 : 0;
	sel+= (config.iFlags>>3)&3;
	combo_sndq->SetCurrentItem(sel);
	switch(config.PicoRegion) {
		case 1: sel = 4; break;
		case 2: sel = 3; break;
		case 4: sel = 2; break;
		case 8: sel = 1; break;
		default:sel = 0; // auto
	}
	combo_region->SetCurrentItem(sel);
}

TBool CPicoConfigDialog::OkToExitL(TInt aButtonId)
{
	if(aButtonId != EEikBidOk) return ETrue;

	CEikHorOptionButtonList *buttons_rot   = (CEikHorOptionButtonList*) Control( ECtlOptRotation );
	CEikHorOptionButtonList *buttons_disp  = (CEikHorOptionButtonList*) Control( ECtlOptScreenMode );
	CEikCheckBox            *chkbox_altrend= (CEikCheckBox*)            Control( ECtlOptUseAltRend );
	CEikCheckBox            *chkbox_acctmng= (CEikCheckBox*)            Control( ECtlOptUseAccTiming );
	CEikCheckBox            *chkbox_sram   = (CEikCheckBox*)            Control( ECtlOptUseSRAM );
	CEikCheckBox            *chkbox_fps    = (CEikCheckBox*)            Control( ECtlOptShowFPS );
	CEikCheckBox            *chkbox_sound  = (CEikCheckBox*)            Control( ECtlOptEnableSound );
	CEikCheckBox            *chkbox_z80    = (CEikCheckBox*)            Control( ECtlOptEmulateZ80 );
	CEikCheckBox            *chkbox_ym2612 = (CEikCheckBox*)            Control( ECtlOptEmulateYM2612 );
	CEikCheckBox            *chkbox_sn76496= (CEikCheckBox*)            Control( ECtlOptEmulateSN76496 );
	CEikChoiceListBase      *combo_sndq    = (CEikChoiceListBase*)      Control( ECtlOptSndQuality );
	CEikCheckBox            *chkbox_6bpad  = (CEikCheckBox*)            Control( ECtlOpt6ButtonPad );
	CEikCheckBox            *chkbox_gzipst = (CEikCheckBox*)            Control( ECtlOptGzipStates );
	CEikCheckBox            *chkbox_motvol = (CEikCheckBox*)            Control( ECtlOptMotDontUseVol );
	CEikCheckBox            *chkbox_accsprt= (CEikCheckBox*)            Control( ECtlOptUseAccSprites );
	CEikChoiceListBase      *combo_region  = (CEikChoiceListBase*)      Control( ECtlOptRegion );

	config.iScreenRotation = (TPicoConfig::TPicoScreenRotation) (buttons_rot->LabeledButtonId() - ECtlOptRotation0);
	config.iScreenMode = (TPicoConfig::TPicoScreenMode) (buttons_disp->LabeledButtonId() - ECtlOptScreenModeCenter);

	if(chkbox_sram   ->State() == CEikButtonBase::ESet) config.iFlags |= 1;     else config.iFlags   &= ~1;
	if(chkbox_fps    ->State() == CEikButtonBase::ESet) config.iFlags |= 2;     else config.iFlags   &= ~2;
	if(chkbox_sound  ->State() == CEikButtonBase::ESet) config.iFlags |= 4;     else config.iFlags   &= ~4;
	if(chkbox_motvol ->State() == CEikButtonBase::ESet) config.iFlags |= 0x40;  else config.iFlags   &= ~0x40;
	if(chkbox_gzipst ->State() == CEikButtonBase::ESet) config.iFlags |= 0x80;  else config.iFlags   &= ~0x80;
	if(chkbox_z80    ->State() == CEikButtonBase::ESet) config.iPicoOpt |= 4;   else config.iPicoOpt &= ~4;
	if(chkbox_ym2612 ->State() == CEikButtonBase::ESet) config.iPicoOpt |= 1;   else config.iPicoOpt &= ~1;
	if(chkbox_sn76496->State() == CEikButtonBase::ESet) config.iPicoOpt |= 2;   else config.iPicoOpt &= ~2;
	if(chkbox_altrend->State() == CEikButtonBase::ESet) config.iPicoOpt |= 0x10;else config.iPicoOpt &= ~0x10;
	if(chkbox_6bpad  ->State() == CEikButtonBase::ESet) config.iPicoOpt |= 0x20;else config.iPicoOpt &= ~0x20;
	if(chkbox_acctmng->State() == CEikButtonBase::ESet) config.iPicoOpt |= 0x40;else config.iPicoOpt &= ~0x40;
	if(chkbox_accsprt->State() == CEikButtonBase::ESet) config.iPicoOpt |= 0x80;else config.iPicoOpt &= ~0x80;

	TInt sel = combo_sndq->CurrentItem();
	if(sel > 3) { config.iPicoOpt |= 8; sel-=4; } else config.iPicoOpt &= ~8;
	config.iFlags &= ~0x18;
	config.iFlags |= (sel<<3)&0x18;

	switch(combo_region->CurrentItem()) {
		case 4: config.PicoRegion = 1; break;
		case 3: config.PicoRegion = 2; break;
		case 2: config.PicoRegion = 4; break;
		case 1: config.PicoRegion = 8; break;
		default:config.PicoRegion = 0; // auto
	}

	return ETrue;
}

// simple GUI stuff needs lots of code
void CPicoConfigDialog::HandleControlStateChangeL(TInt aControlId)
{
	if(aControlId == ECtlOptEnableSound) {
		CEikCheckBox *chkbox_sound  = (CEikCheckBox*) Control( ECtlOptEnableSound );
		CEikCheckBox *chkbox_z80    = (CEikCheckBox*) Control( ECtlOptEmulateZ80 );
		CEikCheckBox *chkbox_ym2612 = (CEikCheckBox*) Control( ECtlOptEmulateYM2612 );
		CEikCheckBox *chkbox_sn76496= (CEikCheckBox*) Control( ECtlOptEmulateSN76496 );

		if(chkbox_sound->State() == CEikButtonBase::ESet) {
			// check all sound chips too, but only if they all are not set
			if((chkbox_z80->State() | chkbox_ym2612->State() | chkbox_sn76496->State()) == CEikButtonBase::EClear) { // (==0)
				chkbox_z80    ->SetState(CEikButtonBase::ESet);
				chkbox_ym2612 ->SetState(CEikButtonBase::ESet);
				chkbox_sn76496->SetState(CEikButtonBase::ESet);
				chkbox_z80    ->DrawDeferred();
				chkbox_ym2612 ->DrawDeferred();
				chkbox_sn76496->DrawDeferred();
			}
		} else {
			// clear everything, but only if everything is set
			if((chkbox_z80->State() & chkbox_ym2612->State() & chkbox_sn76496->State()) == CEikButtonBase::ESet) { // (==1)
				chkbox_z80    ->SetState(CEikButtonBase::EClear);
				chkbox_ym2612 ->SetState(CEikButtonBase::EClear);
				chkbox_sn76496->SetState(CEikButtonBase::EClear);
				chkbox_z80    ->DrawDeferred();
				chkbox_ym2612 ->DrawDeferred();
				chkbox_sn76496->DrawDeferred();
			}
		}
	} else if(aControlId == ECtlOptUseAltRend) {
		CEikCheckBox            *chkbox_altrend= (CEikCheckBox*)            Control( ECtlOptUseAltRend );
		CEikCheckBox            *chkbox_accsprt= (CEikCheckBox*)            Control( ECtlOptUseAccSprites );
		CEikHorOptionButtonList *buttons_rot   = (CEikHorOptionButtonList*) Control( ECtlOptRotation );
		CEikHorOptionButtonList *buttons_disp  = (CEikHorOptionButtonList*) Control( ECtlOptScreenMode );

		TBool dimmed = chkbox_altrend->State() == CEikButtonBase::ESet;
		// show/hide more stuff for alternative renderer
		buttons_disp->SetDimmed(dimmed);
		chkbox_accsprt->SetDimmed(dimmed);
		((CEikOptionButton*)(buttons_rot->ComponentControl(TPicoConfig::PRot0)))->SetDimmed(dimmed);
		((CEikOptionButton*)(buttons_rot->ComponentControl(TPicoConfig::PRot180)))->SetDimmed(dimmed);
		if(dimmed && buttons_rot->LabeledButtonId() != ECtlOptRotation90 && buttons_rot->LabeledButtonId() != ECtlOptRotation270) {
			buttons_rot->SetButtonById(ECtlOptRotation90);
			aControlId = ECtlOptRotation; // cause rotation update
		}
		buttons_disp->SetButtonById(ECtlOptScreenModeCenter);
		chkbox_accsprt->DrawDeferred();
		buttons_disp->DrawDeferred();
		buttons_rot->DrawDeferred();
	}
	if(aControlId == ECtlOptRotation) {
		CEikHorOptionButtonList *buttons_rot   = (CEikHorOptionButtonList*) Control( ECtlOptRotation );
		CEikHorOptionButtonList *buttons_disp  = (CEikHorOptionButtonList*) Control( ECtlOptScreenMode );
		CEikOptionButton        *opt_fit2      = (CEikOptionButton*)        buttons_disp->ComponentControl( TPicoConfig::PMFit2 );

		if(buttons_rot->LabeledButtonId() == ECtlOptRotation0 || buttons_rot->LabeledButtonId() == ECtlOptRotation180) {
			opt_fit2->MakeVisible(ETrue);
			opt_fit2->DrawDeferred();
		} else {
			if(opt_fit2->State() == CEikButtonBase::ESet) {
				buttons_disp->SetButtonById(ECtlOptScreenModeFit);
				buttons_disp->DrawDeferred();
			}
			opt_fit2->MakeVisible(EFalse);
		}
	}
}


/*************************************************************
*
* Credits dialog
*
**************************************************************/

void CCreditsDialog::PreLayoutDynInitL()
{
	CDesCArrayFlat* desArray = CEikonEnv::Static()->ReadDesCArrayResourceL(iMessageResourceID);
	CleanupStack::PushL(desArray);

	TInt iLength;
	TInt count = desArray->Count();
	for (TInt i=0 ;i < count; i++)
	{
		iLength = static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->TextLength();
		static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->Text()->InsertL(iLength, desArray->operator[](i));
		iLength = static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->TextLength();
		static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->Text()->InsertL(iLength, CEditableText::ELineBreak);
	}
	CleanupStack::PopAndDestroy(desArray);
}

void CCreditsDialog::PostLayoutDynInitL()
{
	static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->CreateScrollBarFrameL();
	static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EAuto, CEikScrollBarFrame::EAuto);
}

TKeyResponse CCreditsDialog::OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType)
{
	if (aType == EEventKey)
	{
		if (aKeyEvent.iCode == EQuartzKeyTwoWayDown)
		{
			static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->MoveDisplayL(TCursorPosition::EFLineDown);
			static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->UpdateScrollBarsL();
			return EKeyWasConsumed;
		}
		else if (aKeyEvent.iCode == EQuartzKeyTwoWayUp)
		{
			static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->MoveDisplayL(TCursorPosition::EFLineUp);
			static_cast<CEikGlobalTextEditor*>(Control(ECtlCredits))->UpdateScrollBarsL();
			return EKeyWasConsumed;
		}
	}
	return CEikDialog::OfferKeyEventL(aKeyEvent, aType);
}


/*************************************************************
*
* Debug dialog
*
**************************************************************/

CDebugDialog::CDebugDialog(char *t)
{
	Mem::Copy(iText, t, 1024);
	iText[1023] = 0;
}

void CDebugDialog::PreLayoutDynInitL()
{
	char *p = iText, *line = iText;
	TBool end=0;
	TBuf<128> tbuf;
	CEikGlobalTextEditor *editor = static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit));

	while(!end) {
		while(*p && *p != '\r' && *p != '\n') p++;
		if(!*p) end=1;
		*p = 0;
		TPtrC8 ptr((TUint8*) line);
		tbuf.Copy(ptr);
		editor->Text()->InsertL(editor->TextLength(), tbuf);
		editor->Text()->InsertL(editor->TextLength(), CEditableText::ELineBreak);
		line = ++p;
	}
}

void CDebugDialog::PostLayoutDynInitL()
{
	static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->CreateScrollBarFrameL();
	static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EAuto, CEikScrollBarFrame::EAuto);
}

TKeyResponse CDebugDialog::OfferKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType)
{
	if (aType == EEventKey)
	{
		if (aKeyEvent.iCode == EQuartzKeyTwoWayDown)
		{
			static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->MoveDisplayL(TCursorPosition::EFLineDown);
			static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->UpdateScrollBarsL();
			return EKeyWasConsumed;
		}
		else if (aKeyEvent.iCode == EQuartzKeyTwoWayUp)
		{
			static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->MoveDisplayL(TCursorPosition::EFLineUp);
			static_cast<CEikGlobalTextEditor*>(Control(ECtlDebugEdit))->UpdateScrollBarsL();
			return EKeyWasConsumed;
		}
	}
	return CEikDialog::OfferKeyEventL(aKeyEvent, aType);
}


/************************************************
 *
 * SimpleTextInfoDialog
 *
 ************************************************/


CSimpleTextInfoDialog::CSimpleTextInfoDialog(TInt aTextIdOne, TInt aRichTextCtlIdOne, TInt aTextIdTwo, TInt aRichTextCtlIdTwo, TBool aSimpleTextResIdOneIsArray, TBool aSimpleTextResIdTwoIsArray)
	: iSimpleTextResIdOne(aTextIdOne),
	  iSimpleTextResIdTwo(aTextIdTwo),
	  iRichTextCtlIdOne(aRichTextCtlIdOne),
	  iRichTextCtlIdTwo(aRichTextCtlIdTwo),
	  iSimpleTextResIdOneIsArray(aSimpleTextResIdOneIsArray),
	  iSimpleTextResIdTwoIsArray(aSimpleTextResIdTwoIsArray),
	  iSetDialogBackground(ETrue)
{
}

void CSimpleTextInfoDialog::PreLayoutDynInitL()
{
	CEikRichTextEditor*		richTextEditor;
	
	if (iRichTextCtlIdOne != -1)
	{
		richTextEditor=STATIC_CAST(CEikRichTextEditor*, Control(iRichTextCtlIdOne));
		PreLayoutDynInitRichTextL(*richTextEditor, iRichTextCtlIdOne, iSimpleTextResIdOne);
	}

	if (iRichTextCtlIdTwo != -1)
	{
		richTextEditor=STATIC_CAST(CEikRichTextEditor*, Control(iRichTextCtlIdTwo));
		richTextEditor->Border().SetType(ENone);
		PreLayoutDynInitRichTextL(*richTextEditor, iRichTextCtlIdTwo, iSimpleTextResIdTwo);
	}
}


void CSimpleTextInfoDialog::PreLayoutDynInitRichTextL(CEikRichTextEditor& aRichTextEditor, TInt aRichTextCtlId, TInt aSimpleTextResId)
{
	iEikonEnv->BusyMsgL(_L("Opening"));
	aRichTextEditor.SetEdwinSizeObserver(this);
	if (iSetDialogBackground)
		aRichTextEditor.SetBackgroundColorL(iEikonEnv->Color(EColorDialogBackground));
	aRichTextEditor.SetSize(aRichTextEditor.Size()); // Set the size of the edwin

		// no scrollbars for us
	aRichTextEditor.CreateScrollBarFrameL(); // Create the scrollbars
	aRichTextEditor.ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, iWantVertScrollbar ? CEikScrollBarFrame::EAuto: CEikScrollBarFrame::EOff);

	ShowTextL(*aRichTextEditor.RichText(), aRichTextCtlId, aSimpleTextResId);

	aRichTextEditor.UpdateAllFieldsL(); // Updates all the fields in the document

	aRichTextEditor.UpdateScrollBarsL();
}


void CSimpleTextInfoDialog::PostLayoutDynInitL()
{
	Layout();
	iEikonEnv->BusyMsgCancel();
}

TBool CSimpleTextInfoDialog::HandleEdwinSizeEventL(CEikEdwin* aEdwin, TEdwinSizeEvent aEventType, TSize aDesirableEdwinSize)
{
	if ((aEventType == EEventSizeChanging))
		aEdwin->SetSize(aDesirableEdwinSize);
	return EFalse;
}

void CSimpleTextInfoDialog::ShowTextL(CRichText& aRichText, TInt /*aRichTextCtlId*/, TInt aResId)
{
	if (aResId != -1)
	{
		if ( ((aResId == iSimpleTextResIdOne) && (iSimpleTextResIdOneIsArray)) ||
		     ((aResId == iSimpleTextResIdTwo) && (iSimpleTextResIdTwoIsArray))
		   )
		{
			CDesCArrayFlat* desArray = CEikonEnv::Static()->ReadDesCArrayResourceL(aResId);
			CleanupStack::PushL(desArray);

			CSimpleTextFormatParser* parser = CSimpleTextFormatParser::NewLC();

			TInt	count = desArray->Count();
			for (TInt i=0 ;i<count ; i++)
				parser->ParseL(desArray->operator[](i), aRichText);

			CleanupStack::PopAndDestroy(parser);
			CleanupStack::PopAndDestroy(desArray);
		}
		else
		{
			HBufC*		text =	CEikonEnv::Static()->AllocReadResourceLC(aResId);
			ShowSimpleTextL(*text, aRichText);
			CleanupStack::PopAndDestroy(text);
		}
	}
}

void CSimpleTextInfoDialog::ShowSimpleTextL(const TDesC& aSimpleText, CRichText& aRichText)
{
	CSimpleTextFormatParser* parser = CSimpleTextFormatParser::NewLC();
	parser->ParseL(aSimpleText, aRichText);

	CleanupStack::PopAndDestroy(parser);
}



/************************************************
 *
 * About Dialog
 *
 ************************************************/

CAboutDialog::CAboutDialog() : CSimpleTextInfoDialog(-1, ECtlAboutVersion, R_SIMPLE_TEXT_ABOUT_LINKS, ECtlAboutLinks)
{
}

void CAboutDialog::ShowTextL(CRichText& aRichText, TInt aRichTextCtlId, TInt aResId)
{
	if (aRichTextCtlId == ECtlAboutLinks)
		CSimpleTextInfoDialog::ShowTextL(aRichText, aRichTextCtlId, aResId);
	else
	{
		TBuf<16>	versionText;
		TBuf<512>	text;

		#if (KPicoBuildNumber != 0)
			versionText.Format(_L("%d.%d%d"), KPicoMajorVersionNumber, KPicoMinorVersionNumber, KPicoBuildNumber);
		#else
			versionText.Format(_L("%d.%d"), KPicoMajorVersionNumber, KPicoMinorVersionNumber);
		#endif

		HBufC*		aboutFormat =	CEikonEnv::Static()->AllocReadResourceLC(R_SIMPLE_TEXT_ABOUT);
		text.Format(*aboutFormat, &versionText);

		ShowSimpleTextL(text, aRichText);

		CleanupStack::PopAndDestroy(aboutFormat);
	}
}
