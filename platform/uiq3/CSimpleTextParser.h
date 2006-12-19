/*******************************************************************
 *
 *	File:		CSimpleTextParser.h
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *	(c) Copyright 2002, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#ifndef __CSIMPLE_TEXT_PARSER_H
#define __CSIMPLE_TEXT_PARSER_H

#include <e32def.h>
#include <txtrich.h>				// CRichText
#include <eikrted.h>				// CEikRichTextEditor

class CSimpleTextFormatParser : public CBase
{
public:
	static CSimpleTextFormatParser* NewLC();
	void ParseL(const TDesC& aPSTText, CRichText& aRichText);

protected:
	CSimpleTextFormatParser(){}
	~CSimpleTextFormatParser();
	void ConstructL();

	void ParseTagL(const TDesC& aTag);

	TRgb ForegroundColor();
	void SetBold(TBool aEnable=ETrue);
	void SetItalic(TBool aEnable=ETrue);
	void SetUnderLine(TBool aEnable=ETrue);
	void SetFontHeight(TInt aHeight);
	void SetFontName(const TDesC& aName);
	void SetHiddenText(TBool aEnable=ETrue);
	void SetForegroundColor(const TRgb& aColor);

	void NewParagraph();
	void SetAlignment(CParaFormat::TAlignment aAlignment);
	void SetBackgroundColor(const TRgb& aColor);

	void AppendTextL(const TDesC& aText);
	TInt TextPos();
	TInt ParaPos();


	CRichText*			iRichText;
	TCharFormat			iCharFormat;
	TCharFormatMask		iCharMask;
	CParaFormat*		iParaFormat;
	TParaFormatMask		iParaMask;
	TInt				iCurrentPara;
	TRgb				iPrevFgColor;
};

#endif			/* __CSIMPLE_TEXT_PARSER_H */
