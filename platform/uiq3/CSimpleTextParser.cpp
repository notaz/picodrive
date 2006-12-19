/*******************************************************************
 *
 *	File:		CSimpleTextParser.cpp
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *	(c) Copyright 2002, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#include "CSimpleTextParser.h"

enum
{
	EBadTag,
	EBadZeroLengthTag,
	EBadIntegerParam,
	EBadAlignmentParam,
	EBadRgbColorParam
};

void Panic(TInt aPanic)
{
	User::Panic(_L("STP"), aPanic);
}

CSimpleTextFormatParser* CSimpleTextFormatParser::NewLC()
{
	CSimpleTextFormatParser*	self = new(ELeave)CSimpleTextFormatParser;
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
}

CSimpleTextFormatParser::~CSimpleTextFormatParser()
{
	delete iParaFormat;
}

void CSimpleTextFormatParser::ConstructL()
{
	iParaFormat = CParaFormat::NewL();
}


void CSimpleTextFormatParser::SetBold(TBool aEnable)
{
	iCharFormat.iFontSpec.iFontStyle.SetStrokeWeight(aEnable ? EStrokeWeightBold : EStrokeWeightNormal);
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontStrokeWeight);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

void CSimpleTextFormatParser::SetItalic(TBool aEnable)
{
	iCharFormat.iFontSpec.iFontStyle.SetPosture(aEnable ? EPostureItalic : EPostureUpright);
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontPosture);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

void CSimpleTextFormatParser::SetUnderLine(TBool aEnable)
{
	iCharFormat.iFontPresentation.iUnderline = aEnable ? EUnderlineOn : EUnderlineOff;
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontUnderline);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

void CSimpleTextFormatParser::SetHiddenText(TBool aEnable)
{
	iCharFormat.iFontPresentation.iHiddenText = aEnable;
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontHiddenText);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

TRgb CSimpleTextFormatParser::ForegroundColor()
{
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttColor);
	iRichText->GetCharFormat(iCharFormat, iCharMask, TextPos(), 0);
	return iCharFormat.iFontPresentation.iTextColor;
}

void CSimpleTextFormatParser::SetForegroundColor(const TRgb& aColor)
{
	iCharFormat.iFontPresentation.iTextColor = aColor;
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttColor);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

void CSimpleTextFormatParser::SetBackgroundColor(const TRgb& aColor)
{
	iParaFormat->iFillColor = aColor;
	iParaMask.ClearAll();
	iParaMask.SetAttrib(EAttFillColor);
	iRichText->ApplyParaFormatL(iParaFormat, iParaMask, ParaPos(), 0);
}

void CSimpleTextFormatParser::NewParagraph()
{
	iCurrentPara++;
	iRichText->AppendParagraphL();
	AppendTextL(_L(""));
}


void CSimpleTextFormatParser::SetAlignment(CParaFormat::TAlignment aAlignment)
{
	iParaFormat->iHorizontalAlignment = aAlignment;
	iParaMask.ClearAll();
	iParaMask.SetAttrib(EAttAlignment);
	iRichText->ApplyParaFormatL(iParaFormat, iParaMask, ParaPos(), 0);
}


void CSimpleTextFormatParser::SetFontHeight(TInt aHeight)
{
	iCharFormat.iFontSpec.iHeight = (aHeight * KTwipsPerInch)/KPointsPerInch;
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontHeight);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}

void CSimpleTextFormatParser::SetFontName(const TDesC& aName)
{
	iCharFormat.iFontSpec.iTypeface.iName = aName;
	iCharFormat.iFontSpec.iTypeface.SetAttributes(0);
	iCharFormat.iFontSpec.iTypeface.SetIsProportional(ETrue);
	iCharMask.ClearAll();
	iCharMask.SetAttrib(EAttFontTypeface);
	iRichText->ApplyCharFormatL(iCharFormat, iCharMask, TextPos(), 0);
}


/*
 * Character formatting:
 * <b>				Bold on
 * </b>				Bold of
 * <i>				Italic on
 * </i>				Italic off
 * <u>				Underline on
 * </u>				Underline off
 * <h>				Hidden text on		**doesn't work**
 * </h>				Hidden text off		**doesn't work**
 * <f=name>			Fontname: name (type: string)
 * <s=size>			Fontsize: size (type: integer)
 * <fg=color>		Foreground color: color (type: color)
 * </fg>			Restore foreground color
 *
 * Paragraph formatting:
 * <p>				New paragraph - will reset both character & paragraph formatting to defaults
 * <a=align>		Alignment: aling (type: alignement)
 * <bg=color>		Background color: color (type: color) **doesn't work**
 *
 * Special characters:
 * </<>				The character: <
 *
 * Types:
 * - string:
 * - integer:		Either decimal or hexidecimal value
 * - color:			Either integer specifing rgb value, or (r,g,b) in which r, g and b are of type integer
 * - align:			element of enumeration {center, left, right}
 *
 * Comments:
 * The syntax/parser is fairly simplistic. The parser is not trying to match a tag like 
 * <tag> </tag> as XML/HTML do. Basically, when it encounters a tag (e.g., <b>) it will 
 * simply instruct the the editor to apply the formatting from the current position as 
 * specified by the tag (e.g., enable bold). For example, <b><b>Hello</b>World</b> results
 * in Hello displayed in a Bold font and World in a normal font.
 *
 * The only case where state is maintained is when using <fg=color> and </fg>. The current
 * fg color is stored when parsing <fg=color> and restored when doing </fg>. Again, <fg> and 
 * </fg> don't have the XML/HTML <tag> </tag> behavior. For example:
 *       <fg=red>Peter<fg=blue>was</fg></fg>here
 * results in "Peter" displayed in red, "was" displayed in blue and "here" displayed in red.
 * It literally goes like this:
 *   1) <fg=red>  --> apply editor text color red, previous color = whatever the editor's text color is now
 *   2) <fg=blue> --> apply editor text color blue, previous color = whatever the editor's text color 
 *                    is now --> red
 *   3) </fg>     --> apply editor text to previous color --> red
 *   4) </fg>     --> apply editor text to previous color --> red
 *
 * What you probably wanted was:
 *       <fg=red>Peter</fg><fg=blue>was</fg>here
 * Now "Peter" is displayed in red, "was" in blue and "here" in the default editor's color
 */

static TUint32 ParseInteger(const TDesC& aString)
{
	TUint32		val = 0;
	TBool		parsed = EFalse;
	if (aString.Length() > 2)
	{
		if ((aString[0] == '0') && ((aString[0] == 'x') || (aString[0] == 'X')))
		{
			TLex	lex(aString.Right(aString.Length()-2));
			if (lex.Val(val, EHex) != KErrNone)
			{
				__ASSERT_DEBUG(ETrue, Panic(EBadIntegerParam));
			}
			parsed = ETrue;
		}
	}
	if (!parsed)
	{
		TLex	lex(aString);
		if (lex.Val(val, EDecimal) != KErrNone)
		{
			__ASSERT_DEBUG(ETrue, Panic(EBadIntegerParam));
		}
	}
	return val;
}

static TRgb ParseColor(const TDesC& aString)
{
	if (aString.Length() > 0)
	{
		if (aString[0] == 'R')
		{
			if (aString.Compare(_L("RgbBlack")) == 0)
				return KRgbBlack;
			else if (aString.Compare(_L("RgbDarkGray")) == 0)
				return KRgbDarkGray;
			else if (aString.Compare(_L("RgbDarkRed")) == 0)
				return KRgbDarkRed;
			else if (aString.Compare(_L("RgbDarkGreen")) == 0)
				return KRgbDarkGreen;
			else if (aString.Compare(_L("RgbDarkYellow")) == 0)
				return KRgbDarkYellow;
			else if (aString.Compare(_L("RgbDarkBlue")) == 0)
				return KRgbDarkBlue;
			else if (aString.Compare(_L("RgbDarkMagenta")) == 0)
				return KRgbDarkMagenta;
			else if (aString.Compare(_L("RgbDarkCyan")) == 0)
				return KRgbDarkCyan;
			else if (aString.Compare(_L("RgbRed")) == 0)
				return KRgbRed;
			else if (aString.Compare(_L("RgbGreen")) == 0)
				return KRgbGreen;
			else if (aString.Compare(_L("RgbYellow")) == 0)
				return KRgbYellow;
			else if (aString.Compare(_L("RgbBlue")) == 0)
				return KRgbBlue;
			else if (aString.Compare(_L("RgbMagenta")) == 0)
				return KRgbMagenta;
			else if (aString.Compare(_L("RgbCyan")) == 0)
				return KRgbCyan;
			else if (aString.Compare(_L("RgbGray")) == 0)
				return KRgbGray;
			else if (aString.Compare(_L("RgbWhite")) == 0)
				return KRgbWhite;
			else
			{
				__ASSERT_DEBUG(ETrue, Panic(EBadRgbColorParam));
			}
		}
		return ParseInteger(aString);
	}
	__ASSERT_DEBUG(ETrue, Panic(EBadRgbColorParam));

	return KRgbBlack;
}



static CParaFormat::TAlignment ParseAlignment(const TDesC& aString)
{
	if (aString.Compare(_L("center")) == 0)
	{
		return CParaFormat::ECenterAlign;
	}
	else if (aString.Compare(_L("left")) == 0)
	{
		return CParaFormat::ELeftAlign;
	}
	else if (aString.Compare(_L("right")) == 0)
	{
		return CParaFormat::ERightAlign;
	}
	__ASSERT_DEBUG(ETrue, Panic(EBadAlignmentParam));

	return CParaFormat::ECenterAlign;
}

void CSimpleTextFormatParser::ParseTagL(const TDesC& aTag)
{
	TInt	tagLength = aTag.Length();
	if (tagLength == 0)
	{
		__ASSERT_DEBUG(ETrue, Panic(EBadZeroLengthTag));
		return;
	}

	TPtrC	param(_L(""));
	TInt pos = aTag.Find(_L("="));
	if (pos>0)
	{
		param.Set(aTag.Right(aTag.Length()-pos-1));
		tagLength = pos;
	}
	TPtrC	tag = aTag.Left(tagLength);

//	RDebug::Print(_L("tag=%S, param=%S"), &tag, &param);

	switch (tagLength)
	{
		case 1:
		{
			if (tag.Compare(_L("a")) == 0)
				SetAlignment(ParseAlignment(param));
			else if (tag.Compare(_L("b")) == 0)
				SetBold();
			else if (tag.Compare(_L("f")) == 0)
				SetFontName(param);
			else if (tag.Compare(_L("h")) == 0)
				SetHiddenText();
			else if (tag.Compare(_L("i")) == 0)
				SetItalic();
			else if (tag.Compare(_L("p")) == 0)
				NewParagraph();
			else if (tag.Compare(_L("s")) == 0)
				SetFontHeight(ParseInteger(param));
			else if (tag.Compare(_L("u")) == 0)
				SetUnderLine();
			else
			{
				__ASSERT_DEBUG(ETrue, Panic(EBadTag));
			}
			break;
		}
		
		case 2:
		{
			if (tag.Compare(_L("/b")) == 0)
				SetBold(EFalse);
			if (tag.Compare(_L("bg")) == 0)
				SetBackgroundColor(ParseColor(param));
			if (tag.Compare(_L("fg")) == 0)
			{
				iPrevFgColor = ForegroundColor();
				SetForegroundColor(ParseColor(param));
			}
			else if (tag.Compare(_L("/h")) == 0)
				SetHiddenText(EFalse);
			else if (tag.Compare(_L("/i")) == 0)
				SetItalic(EFalse);
			else if (tag.Compare(_L("/u")) == 0)
				SetUnderLine(EFalse);
			else if (tag.Compare(_L("/<")) == 0)
				AppendTextL(_L("<"));
			break;
		}
		case 3:
		{
			if (tag.Compare(_L("/fg")) == 0)
				SetForegroundColor(iPrevFgColor);
			break;
		}
		default:
			;
	}
}

void CSimpleTextFormatParser::ParseL(const TDesC& aSimpleText, CRichText& aRichText)
{
	iRichText = &aRichText;
	iCurrentPara = 0;

	TBool	done = EFalse;
	TPtrC simpleText(aSimpleText);
	do
	{
		TInt pos = simpleText.Locate('<');
		if (pos > 0)
		{
			AppendTextL(simpleText.Left(pos));
			simpleText.Set(simpleText.Right(simpleText.Length() - pos));
		}
		else if (pos == 0)
		{
			pos = simpleText.Locate('>');
			if (pos<=0)
				User::Leave(KErrArgument);
			ParseTagL(simpleText.Mid(1, pos-1));
			simpleText.Set(simpleText.Right(simpleText.Length() - pos - 1));
		}
		else
		{
			AppendTextL(simpleText);
			done = ETrue;
		}
	} while (!done);
}


TInt CSimpleTextFormatParser::TextPos()
{
	return iRichText->DocumentLength();
#if 0
	TInt pos, length;
	pos = iRichText->CharPosOfParagraph(length, iCurrentPara);
	return pos+length-1;
#endif
}

TInt CSimpleTextFormatParser::ParaPos()
{
	return TextPos();
#if 0
	TInt pos, length;
	pos = iRichText->CharPosOfParagraph(length, iCurrentPara);
	return pos+length-1;
#endif
}


void CSimpleTextFormatParser::AppendTextL(const TDesC& aText)
{
//	RDebug::Print(_L("text=%S"), &aText);
	iRichText->InsertL(TextPos(), aText);
}


#if 0
void CTestDialog::ShowTextL(CRichText& aRichText)
{
	aRichText.Reset();

	TCharFormat			charFormat;
	TCharFormatMask		charMask;
	aRichText.GetCharFormat(charFormat, charMask, 0, 0);

	TInt para = 0;
	AppendTextL(_L("http://www.yipton.net"), aRichText);

	para++;
	aRichText.AppendParagraphL();

	CParaFormat*	paraFormat = CParaFormat::NewLC();
	TParaFormatMask	paraMask;
	aRichText.GetParaFormatL(paraFormat, paraMask, ParaPos(aRichText, para), 0);
	paraFormat->iHorizontalAlignment = CParaFormat::ECenterAlign;
	paraMask.ClearAll();
	paraMask.SetAttrib(EAttAlignment);
	aRichText.ApplyParaFormatL(paraFormat, paraMask, ParaPos(aRichText, para), 0);

	charFormat.iFontPresentation.iUnderline = EUnderlineOn;
	charFormat.iFontSpec.iFontStyle.SetPosture(EPostureItalic);
	charMask.ClearAll();
	charMask.SetAttrib(EAttFontPosture);
	charMask.SetAttrib(EAttFontUnderline);
	aRichText.ApplyCharFormatL(charFormat, charMask, TextPos(aRichText, para));
	AppendTextL(_L("mailto:Peter is here"), aRichText, para);

	para++;
	aRichText.AppendParagraphL();

	TFontSpec	fontSpec(_L("edmunds"), 20 * KPointsPerInch);
//	CFont*	font = NULL;
//	iCoeEnv->ScreenDevice()->GetNearestFontInTwips(font, fontSpec);

	charFormat.iFontSpec = fontSpec;
	charMask.ClearAll();
	charMask.SetAttrib(EAttFontHeight);
	charMask.SetAttrib(EAttFontTypeface);
	aRichText.ApplyCharFormatL(charFormat, charMask, TextPos(aRichText, para));
	AppendTextL(_L("mailto:Peter is here"), aRichText, para);

	CleanupStack::PopAndDestroy();
}

#endif
