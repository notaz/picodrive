/*******************************************************************
 *
 *	File:		Audio_mediaserver.cpp
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

#include "audio_mediaserver.h"

//#define __DEBUG_PRINT_SND

#ifdef __DEBUG_PRINT_SND
	#include <e32svr.h> // RDebug
	#define DEBUGPRINT(x...) RDebug::Print(x)
#else
	#define DEBUGPRINT(x...)
#endif


GLDEF_C TInt E32Dll(TDllReason)
{
	return KErrNone;
}


/*******************************************
 *
 * CGameAudioMS
 *
 *******************************************/

CGameAudioMS::CGameAudioMS(TInt aRate, TBool aStereo, TInt aPcmFrames,  TInt aBufferedFrames)
: iRate(aRate), iStereo(aStereo), iBufferedFrames(aBufferedFrames), iPcmFrames(aPcmFrames)
{
}


CGameAudioMS* CGameAudioMS::NewL(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames)
{
	DEBUGPRINT(_L("CGameAudioMS::NewL(%i, %i, %i, %i)"),aRate, aStereo, aPcmFrames, aBufferedFrames);
	CGameAudioMS*		self = new(ELeave) CGameAudioMS(aRate, aStereo, aPcmFrames, aBufferedFrames);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();		// self
	return self;
}


CGameAudioMS::~CGameAudioMS()
{
	DEBUGPRINT(_L("CGameAudioMS::~CGameAudioMS()"));
	if(iMdaAudioOutputStream) {
		iScheduler->Schedule(); // let it finish it's stuff
		iMdaAudioOutputStream->Stop();
		delete iMdaAudioOutputStream;
	}
	if(iServer) delete iServer;

	for (TInt i=0 ; i<KSoundBuffers+1 ; i++)
		delete iSoundBuffers[i];

	// Polled AS
	if(iScheduler) delete iScheduler;
}


void CGameAudioMS::ConstructL()
{
	iServer = CMdaServer::NewL();

	iScheduler = CPolledActiveScheduler::NewL();

	switch(iRate) {
		case 11025: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate11025Hz; break;
		case 16000: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate16000Hz; break;
		case 22050: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate22050Hz; break;
		default:    iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;  break;
	}

	iMdaAudioDataSettings.iChannels   = (iStereo) ? TMdaAudioDataSettings::EChannelsStereo : TMdaAudioDataSettings::EChannelsMono;
	iMdaAudioDataSettings.iCaps       = TMdaAudioDataSettings::ESampleRateFixed | iMdaAudioDataSettings.iSampleRate;
	iMdaAudioDataSettings.iFlags      = TMdaAudioDataSettings::ENoNetworkRouting;

	TInt	bytesPerFrame = iStereo ? iPcmFrames << 2 : iPcmFrames << 1;
	for (TInt i=0 ; i<KSoundBuffers ; i++)
	{
		iSoundBuffers[i] = HBufC8::NewL(bytesPerFrame * iBufferedFrames);
		iSoundBuffers[i]->Des().FillZ  (bytesPerFrame * iBufferedFrames);
	}
	// because feeding 2 buffers after an underflow is a little too much, but feeding 1 may be not enough,
	// prepare this ~50ms empty buffer to additionaly feed after every underflow.
	// Another strange thing here: if we try to make and odd-length sound buffer here,
	// system then outputs horrible noise! (this happened on 22050 mono and when there
	// were no parenthesis around iBufferedFrames / 4.
	iSoundBuffers[KSoundBuffers] = HBufC8::NewL(bytesPerFrame * (iBufferedFrames / 4));
	iSoundBuffers[KSoundBuffers]->Des().FillZ  (bytesPerFrame * (iBufferedFrames / 4));

	iCurrentBuffer = 0;

	// here we actually test if we can create and open CMdaAudioOutputStream at all, but really create and use it later.
	iMdaAudioOutputStream = CMdaAudioOutputStream::NewL(iListener, iServer);
	if(iMdaAudioOutputStream) {
		delete iMdaAudioOutputStream;
		iMdaAudioOutputStream = 0;
	}
}

/* currently unused
TInt CGameAudioMS::Write(TInt16* aBuffer, TInt aSize)
{
	TInt	byteSize = iStereo ? aSize << 2 : aSize << 1;
	Mem::Copy(iCurrentPosition, aBuffer, byteSize);
	iCurrentPosition += aSize;

	if (++iFrameCount == iBufferedFrames)
	{
		WriteBlock();
	}

	CPolledActiveScheduler::Instance()->Schedule();
	if(iListener.iUnderflowed) Underflowed(); // oh no, CMdaAudioOutputStream underflowed!

	return aSize;
}
*/

// returns a pointer to buffer for next frame,
// to be used when iSoundBuffers are used directly
TInt16 *CGameAudioMS::NextFrameL()
{
	iCurrentPosition += iPcmFrames << (iStereo?1:0);

	if (++iFrameCount == iBufferedFrames)
	{
		WriteBlockL();
	}

	iScheduler->Schedule();

	if(iListener.iUnderflowed) {
		if(iListener.iUnderflowed > KMaxUnderflows) {
			delete iMdaAudioOutputStream;
			iMdaAudioOutputStream = 0;
			return 0;
		}
		UnderflowedL(); // not again!
	}

	return iCurrentPosition;
}

TInt16 *CGameAudioMS::DupeFrameL(TInt &aUnderflowed)
{
	TInt shorts = iStereo ? (iPcmFrames << 1) : iPcmFrames;
	if(iFrameCount)
		Mem::Copy(iCurrentPosition, iCurrentPosition-shorts, shorts<<1);
	else {
		TInt lastBuffer = iCurrentBuffer;
		if(--lastBuffer < 0) lastBuffer = KSoundBuffers - 1;
		Mem::Copy(iCurrentPosition, ((TInt16*) (iSoundBuffers[lastBuffer]->Ptr()))+shorts*(iBufferedFrames-1), shorts<<1);
	}				
	iCurrentPosition += shorts;

	if (++iFrameCount == iBufferedFrames)
	{
		WriteBlockL();
	}

	iScheduler->Schedule();

	if((aUnderflowed = iListener.iUnderflowed)) { // not again!
		if(iListener.iUnderflowed > KMaxUnderflows) {
			delete iMdaAudioOutputStream;
			iMdaAudioOutputStream = 0;
			return 0;
		}
		UnderflowedL(); // not again!
	}

	return iCurrentPosition;
}

void CGameAudioMS::WriteBlockL()
{
	iScheduler->Schedule();
	// do not write until stream is open
	if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	//if(!iListener.iHasCopied) WaitForCopyToCompleteL(); // almost never happens anyway and sometimes even deadlocks?
	//iListener.iHasCopied = EFalse;
	

	if(!iListener.iUnderflowed) {
		// don't write if sound is lagging too much
		if(iTime - iMdaAudioOutputStream->Position().Int64() <= TInt64(0, KMaxLag)) {
			//RDebug::Print(_L("delta: %i"), iTime.Low() - iMdaAudioOutputStream->Position().Int64().Low());
			iMdaAudioOutputStream->WriteL(*iSoundBuffers[iCurrentBuffer]);
			iTime += KBlockTime;
		}
	}

	iFrameCount = 0;
	if (++iCurrentBuffer == KSoundBuffers)
		iCurrentBuffer = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
}

void CGameAudioMS::Pause()
{
	if(!iMdaAudioOutputStream) return;

	iScheduler->Schedule(); // let it finish it's stuff
	iMdaAudioOutputStream->Stop();
	delete iMdaAudioOutputStream;
	iMdaAudioOutputStream = 0;
}

// call this before doing any playback!
TInt16 *CGameAudioMS::ResumeL()
{
	DEBUGPRINT(_L("CGameAudioMS::Resume()"));
	iScheduler->Schedule();

	// we act a bit strange here: simulate buffer underflow, which actually starts audio
	iListener.iIsOpen = ETrue;
	iListener.iUnderflowed = 1;
	iFrameCount = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
	return iCurrentPosition;
}

// handles underflow condition
void CGameAudioMS::UnderflowedL()
{
	// recreate the stream
	//iMdaAudioOutputStream->Stop();
	if(iMdaAudioOutputStream) delete iMdaAudioOutputStream;
	iMdaAudioOutputStream = CMdaAudioOutputStream::NewL(iListener, iServer);
	iMdaAudioOutputStream->Open(&iMdaAudioDataSettings);
	iListener.iIsOpen = EFalse;   // wait for it to open
	//iListener.iHasCopied = ETrue; // but don't wait for last copy to complete
	// let it open and feed some stuff to make it happy
	User::After(0);
	TInt lastBuffer = iCurrentBuffer;
	if(--lastBuffer < 0) lastBuffer = KSoundBuffers - 1;
	iScheduler->Schedule();
	if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	iMdaAudioOutputStream->WriteL(*iSoundBuffers[KSoundBuffers]); // special empty fill-up
	iMdaAudioOutputStream->WriteL(*iSoundBuffers[lastBuffer]);
	iTime = TInt64(0, KBlockTime/4 + KBlockTime);
}

/*
void CGameAudioMS::WaitForCopyToCompleteL()
{
	DEBUGPRINT(_L("CGameAudioMS::WaitForCopyToCompleteL"));
	while (!iListener.iHasCopied) {
		//User::After(0);
		iScheduler->Schedule();
	}
}
*/

void CGameAudioMS::WaitForOpenToCompleteL()
{
	DEBUGPRINT(_L("CGameAudioMS::WaitForOpenToCompleteL"));
	TInt	count = 20;		// 2 seconds
	TInt	waitPeriod = 100 * 1000;

	if(!iListener.iIsOpen) {
		// it is often enough to do this
		User::After(0);
		iScheduler->Schedule();
	}
	while (!iListener.iIsOpen && --count)
	{
		User::After(waitPeriod);
		iScheduler->Schedule();
	}
	if (!iListener.iIsOpen)
		User::LeaveIfError(KErrNotSupported);
}

void CGameAudioMS::ChangeVolume(TInt aUp)
{
	// do nothing
	DEBUGPRINT(_L("CGameAudioMS::ChangeVolume(%i)"), aUp);
}

void TGameAudioEventListener::MaoscOpenComplete(TInt aError)
{
	DEBUGPRINT(_L("CGameAudioMS::MaoscOpenComplete, error=%d"), aError);

	iIsOpen = ETrue;
	if(aError) iUnderflowed++;
	else       iUnderflowed = 0;
}

void TGameAudioEventListener::MaoscBufferCopied(TInt aError, const TDesC8& aBuffer)
{
	DEBUGPRINT(_L("CGameAudioMS::MaoscBufferCopied, error=%d"), aError);

//	iHasCopied = ETrue;

	if(aError) // shit!
		 iUnderflowed++;
}

void TGameAudioEventListener::MaoscPlayComplete(TInt aError)
{
	DEBUGPRINT(_L("CGameAudioMS::MaoscPlayComplete: %i"), aError);
	if(aError)
		iUnderflowed++; // never happened to me while testing, but just in case
}

