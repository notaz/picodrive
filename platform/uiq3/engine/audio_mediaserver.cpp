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
#include "debug.h"

//#undef DEBUGPRINT
//#define DEBUGPRINT(x...)


const TInt KUpdatesPerSec = 10;
const TInt KBlockTime = 1000000 / KUpdatesPerSec;
const TInt KMaxLag = 200000; // max sound lag, lower values increase chance of underflow
const TInt KMaxUnderflows = 50; // max underflows/API errors we are going allow in a row (to prevent lockups)


/*******************************************
 *
 * CGameAudioMS
 *
 *******************************************/

CGameAudioMS::CGameAudioMS(TInt aRate, TBool aStereo, TInt aWritesPerSec)
: iRate(aRate), iStereo(aStereo), iWritesPerSec(aWritesPerSec)
{
}


CGameAudioMS* CGameAudioMS::NewL(TInt aRate, TBool aStereo, TInt aWritesPerSec)
{
	DEBUGPRINT(_L("CGameAudioMS::NewL(%i, %i, %i)"), aRate, aStereo, aWritesPerSec);
	CGameAudioMS*		self = new(ELeave) CGameAudioMS(aRate, aStereo, aWritesPerSec);
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

	for (TInt i=0; i<KSoundBuffers; i++)
		delete iSoundBuffers[i];

	// Polled AS
	//if(iScheduler) delete iScheduler;
}


void CGameAudioMS::ConstructL()
{
	iServer = CMdaServer::NewL();

	// iScheduler = CPolledActiveScheduler::NewL();
	iScheduler = CPolledActiveScheduler::Instance();

	switch(iRate) {
		case 11025: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate11025Hz; break;
		case 16000: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate16000Hz; break;
		case 22050: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate22050Hz; break;
		case 44100: iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate44100Hz; break;
		default:    iMdaAudioDataSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;  break;
	}

	iMdaAudioDataSettings.iChannels   = (iStereo) ? TMdaAudioDataSettings::EChannelsStereo : TMdaAudioDataSettings::EChannelsMono;
	iMdaAudioDataSettings.iCaps       = TMdaAudioDataSettings::ESampleRateFixed | iMdaAudioDataSettings.iSampleRate;
	iMdaAudioDataSettings.iFlags      = TMdaAudioDataSettings::ENoNetworkRouting;

	int pcmFrames = iRate / iWritesPerSec;
	pcmFrames += iRate - (iRate / iWritesPerSec) * iWritesPerSec; // add division remainder too for our buffer size
	iBufferedFrames = iWritesPerSec / KUpdatesPerSec;

	TInt bytesPerFrame = pcmFrames << (iStereo?2:1);
	for (TInt i=0 ; i<KSoundBuffers ; i++)
	{
		iSoundBuffers[i] = HBufC8::NewL(bytesPerFrame * iBufferedFrames);
		iSoundBuffers[i]->Des().FillZ  (bytesPerFrame * iBufferedFrames);
	}

	iCurrentBuffer = 0;
	iCurrentBufferSize = 0;

	// here we actually test if we can create and open CMdaAudioOutputStream at all, but really create and use it later.
	iMdaAudioOutputStream = CMdaAudioOutputStream::NewL(iListener, iServer);
	if(iMdaAudioOutputStream) {
		iVolume = iMdaAudioOutputStream->MaxVolume();
		DEBUGPRINT(_L("MaxVolume: %i"), iVolume);
		delete iMdaAudioOutputStream;
		iMdaAudioOutputStream = 0;
	}
}

// returns a pointer to buffer for next frame,
// to be used when iSoundBuffers are used directly
TInt16 *CGameAudioMS::NextFrameL(TInt aPcmFrames)
{
	iCurrentPosition   += aPcmFrames << (iStereo?1:0);
	iCurrentBufferSize += aPcmFrames << (iStereo?2:1);

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

void CGameAudioMS::WriteBlockL()
{
	iScheduler->Schedule();
	// do not write until stream is open
	if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	//if(!iListener.iHasCopied) WaitForCopyToCompleteL(); // almost never happens anyway and sometimes even deadlocks?
	//iListener.iHasCopied = EFalse;
	

	if(!iListener.iUnderflowed) {
		TInt64 delta;
		// don't write if sound is lagging too much
		delta = iTime - iMdaAudioOutputStream->Position().Int64();
		if (delta > MAKE_TINT64(0, KMaxLag))
			// another query sometimes returns very different result
			delta = iTime - iMdaAudioOutputStream->Position().Int64();

		if(delta <= MAKE_TINT64(0, KMaxLag)) {
			//RDebug::Print(_L("delta: %i"), iTime.Low() - iMdaAudioOutputStream->Position().Int64().Low());
			iSoundBuffers[iCurrentBuffer]->Des().SetLength(iCurrentBufferSize);
			iMdaAudioOutputStream->WriteL(*iSoundBuffers[iCurrentBuffer]);
			iTime += KBlockTime;
		} else {
			DEBUGPRINT(_L("lag: %i"), I64LOW(delta));
		}
	}

	iFrameCount = 0;
	if (++iCurrentBuffer == KSoundBuffers)
		iCurrentBuffer = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
	iCurrentBufferSize = 0;
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
	iListener.iLastError = 0;
	iFrameCount = 0;
	iCurrentBufferSize = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
	return iCurrentPosition;
}

// handles underflow condition
void CGameAudioMS::UnderflowedL()
{
	DEBUGPRINT(_L("UnderflowedL()"));

	if (iListener.iLastError != KErrUnderflow)
	{
		// recreate the stream
		//iMdaAudioOutputStream->Stop();
		if(iMdaAudioOutputStream) delete iMdaAudioOutputStream;
		iMdaAudioOutputStream = CMdaAudioOutputStream::NewL(iListener, iServer);
		iMdaAudioOutputStream->Open(&iMdaAudioDataSettings);
		iMdaAudioOutputStream->SetAudioPropertiesL(iMdaAudioDataSettings.iSampleRate, iMdaAudioDataSettings.iChannels);
		iMdaAudioOutputStream->SetVolume(iVolume); // new in UIQ3

		iListener.iIsOpen = EFalse;   // wait for it to open
		//iListener.iHasCopied = ETrue; // but don't wait for last copy to complete
		// let it open and feed some stuff to make it happy
		User::After(0);
		iScheduler->Schedule();
		iListener.iLastError = 0;
		if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	} else {
		iListener.iLastError = iListener.iUnderflowed = 0;
	}
	iTime = iMdaAudioOutputStream->Position().Int64();
}

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
	//DEBUGPRINT(_L("CGameAudioMS::ChangeVolume(%i)"), aUp);

	if (iMdaAudioOutputStream) {
		if (aUp) {
			if (iVolume < iMdaAudioOutputStream->MaxVolume()) iVolume+=5;
		} else {
			if (iVolume > 0) iVolume-=5;
		}
		iMdaAudioOutputStream->SetVolume(iVolume);
	}
}

void TGameAudioEventListener::MaoscOpenComplete(TInt aError)
{
	DEBUGPRINT(_L("CGameAudioMS::MaoscOpenComplete, error=%d"), aError);

	iIsOpen = ETrue;
	if(aError) {
		iLastError = aError;
		iUnderflowed++;
	}
	else iUnderflowed = 0;
}

void TGameAudioEventListener::MaoscBufferCopied(TInt aError, const TDesC8& aBuffer)
{
	if (aError)
		DEBUGPRINT(_L("CGameAudioMS::MaoscBufferCopied, error=%d"), aError);

//	iHasCopied = ETrue;

	if(aError) { // shit!
		iLastError = aError;
		iUnderflowed++;
	}
}

void TGameAudioEventListener::MaoscPlayComplete(TInt aError)
{
	DEBUGPRINT(_L("CGameAudioMS::MaoscPlayComplete: %i"), aError);
	if(aError) {
		iLastError = aError;
		iUnderflowed++; // never happened to me while testing, but just in case
	}
}

