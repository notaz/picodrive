/*******************************************************************
 *
 *	File:		Audio_motorola.cpp
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

// if only I had Motorola to test this on..


#include "audio_motorola.h"

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
 * CGameAudioMot
 *
 *******************************************/

CGameAudioMot::CGameAudioMot(TInt aRate, TBool aStereo, TInt aPcmFrames,  TInt aBufferedFrames)
: iRate(aRate), iStereo(aStereo), iBufferedFrames(aBufferedFrames), iPcmFrames(aPcmFrames)
{
	DEBUGPRINT(_L("CGameAudioMot::CGameAudioMot"));
}


CGameAudioMot* CGameAudioMot::NewL(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames)
{
	DEBUGPRINT(_L("CGameAudioMot::NewL(%i, %i, %i, %i)"),aRate, aStereo, aPcmFrames, aBufferedFrames);
	CGameAudioMot*		self = new(ELeave) CGameAudioMot(aRate, aStereo, aPcmFrames, aBufferedFrames);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();		// self
	return self;
}


CGameAudioMot::~CGameAudioMot()
{
	DEBUGPRINT(_L("CGameAudioMot::~CGameAudioMot()"));
	if(iAudioOutputStream) {
		iScheduler->Schedule(); // let it finish it's stuff
		//iAudioOutputStream->Stop();
		delete iAudioOutputStream;
	}

	if(iAudioControl) delete iAudioControl;

	for (TInt i=0 ; i < KSoundBuffers+1; i++) {
		delete iSoundBufferPtrs[i];
		delete iSoundBuffers[i];
	}

	// Polled AS
	if(iScheduler) delete iScheduler;
}


void CGameAudioMot::ConstructL()
{
	iScheduler = CPolledActiveScheduler::NewL();

	iSettings.iPCMSettings.iSamplingFreq = (TMSampleRate) iRate;
	iSettings.iPCMSettings.iStereo       = iStereo;

	TInt	bytesPerFrame = iStereo ? iPcmFrames << 2 : iPcmFrames << 1;
	for (TInt i=0 ; i<KSoundBuffers ; i++)
	{
		iSoundBuffers[i] = HBufC8::NewL(bytesPerFrame * iBufferedFrames);
		iSoundBuffers[i]->Des().FillZ  (bytesPerFrame * iBufferedFrames);
		iSoundBufferPtrs[i] = new TPtr8( iSoundBuffers[i]->Des() );
	}
	// because feeding 2 buffers after an underflow is a little too much, but feeding 1 may be not enough,
	// prepare this ~50ms empty buffer to additionaly feed after every underflow.
	iSoundBuffers[KSoundBuffers] = HBufC8::NewL(bytesPerFrame * (iBufferedFrames / 4));
	iSoundBuffers[KSoundBuffers]->Des().FillZ  (bytesPerFrame * (iBufferedFrames / 4));
	iSoundBufferPtrs[KSoundBuffers] = new TPtr8( iSoundBuffers[KSoundBuffers]->Des() );

	iCurrentBuffer = 0;
	iListener.iFatalError = iListener.iIsOpen = iListener.iIsCtrlOpen = EFalse;

	// here we actually test if we can create and open CMdaAudioOutputStream at all, but really create and use it later.
	iAudioOutputStream = CMAudioFB::NewL(EMAudioFBRequestTypeDecode, EMAudioFBFormatPCM, iSettings, iListener);
	if(iAudioOutputStream) {
		delete iAudioOutputStream;
		iAudioOutputStream = 0;
	}

	// ceate audio control object
	iAudioControl = CMAudioAC::NewL(iListener);
}


// returns a pointer to buffer for next frame,
// to be used when iSoundBuffers are used directly
TInt16 *CGameAudioMot::NextFrameL()
{
	iCurrentPosition += iPcmFrames << (iStereo?1:0);

	if (++iFrameCount == iBufferedFrames)
	{
		WriteBlockL();
	}

	iScheduler->Schedule();

	if(iListener.iFatalError || iListener.iUnderflowed > KMaxUnderflows) {
		if(iAudioOutputStream) delete iAudioOutputStream;
		iAudioOutputStream = 0;
		return 0;
	}
	else if(iListener.iUnderflowed) UnderflowedL();

	return iCurrentPosition;
}

TInt16 *CGameAudioMot::DupeFrameL(TInt &aUnderflowed)
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

	if(iListener.iFatalError || iListener.iUnderflowed > KMaxUnderflows) {
		if(iAudioOutputStream) delete iAudioOutputStream;
		iAudioOutputStream = 0;
		return 0;
	}
	else if((aUnderflowed = iListener.iUnderflowed)) UnderflowedL(); // not again!

	return iCurrentPosition;
}

void CGameAudioMot::WriteBlockL()
{
	iScheduler->Schedule();

	// do not write until stream is open
	if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	//if(!iListener.iHasCopied) WaitForCopyToCompleteL(); // almost never happens anyway and sometimes even deadlocks?
	//iListener.iHasCopied = EFalse;
	

	if(!iListener.iUnderflowed) {
		iAudioOutputStream->QueueBufferL(iSoundBufferPtrs[iCurrentBuffer]);
		// it is certain we already Queued at least 2 buffers (one just after underflow, another above)
		if(!iDecoding) {
			iAudioOutputStream->DecodeL();
			iDecoding = ETrue;
		}
	}

	iFrameCount = 0;
	if (++iCurrentBuffer == KSoundBuffers)
		iCurrentBuffer = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
}

void CGameAudioMot::Pause()
{
	if(!iAudioOutputStream) return;

	iScheduler->Schedule();
	// iAudioOutputStream->Stop(); // may be this breaks everything in A925?
	delete iAudioOutputStream;
	iAudioOutputStream = 0;
}

// call this before doing any playback!
TInt16 *CGameAudioMot::ResumeL()
{
	DEBUGPRINT(_L("CGameAudioMot::Resume()"));
	iScheduler->Schedule();

	// we act a bit strange here: simulate buffer underflow, which actually starts audio
	iListener.iIsOpen = ETrue;
	iListener.iUnderflowed = 1;
	iListener.iFatalError = EFalse;
	iFrameCount = 0;
	iCurrentPosition = (TInt16*) iSoundBuffers[iCurrentBuffer]->Ptr();
	return iCurrentPosition;
}

// handles underflow condition
void CGameAudioMot::UnderflowedL()
{
	// recreate the stream
	if(iAudioOutputStream) delete iAudioOutputStream;
	if(iListener.iUnderflowed > 4) {
		// HACK: A925 user said sound works for the first time, but fails after pause/resume, etc.
		// at the very beginning we create and delete CMAudioFB object, maybe we should do this every time?
		iAudioOutputStream = CMAudioFB::NewL(EMAudioFBRequestTypeDecode, EMAudioFBFormatPCM, iSettings, iListener);
		if(iAudioOutputStream) delete iAudioOutputStream;
	}

	iAudioOutputStream = CMAudioFB::NewL(EMAudioFBRequestTypeDecode, EMAudioFBFormatPCM, iSettings, iListener);
	iListener.iIsOpen = EFalse;   // wait for it to open
	iDecoding = EFalse;
	//iListener.iHasCopied = ETrue; // but don't wait for last copy to complete
	// let it open and feed some stuff to make it happy
	User::After(0);
	//TInt lastBuffer = iCurrentBuffer;
	//if(--lastBuffer < 0) lastBuffer = KSoundBuffers - 1;
	iScheduler->Schedule();
	if(!iListener.iIsOpen) WaitForOpenToCompleteL();
	if(iListener.iUnderflowed) {
		// something went wrong again. May be it needs time? Trying to fix something without ability to test is hell.
		if(iAudioOutputStream) delete iAudioOutputStream;
		iAudioOutputStream = 0;
		User::After(50*000);
		iScheduler->Schedule();
		return;
	}

	iAudioOutputStream->QueueBufferL(iSoundBufferPtrs[KSoundBuffers]); // try a short buffer with hope to reduce lag
}


void CGameAudioMot::ChangeVolume(TInt aUp)
{
	if(iAudioControl && iListener.iIsCtrlOpen)
	{
		TInt vol = iAudioControl->GetMasterVolume();
		TInt max = iAudioControl->GetMaxMasterVolume();

		if(aUp) vol++; // adjust volume
		else    vol--;

		if(vol >= 0 && vol <= max)
		{
			iAudioControl->SetMasterVolume(vol);
		}
	}
}


void CGameAudioMot::WaitForOpenToCompleteL()
{
	DEBUGPRINT(_L("CGameAudioMot::WaitForOpenToCompleteL"));
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



void TGameAudioEventListener::OnEvent(TMAudioFBCallbackState aState, TInt aError)
{
	switch ( aState )
	{
	case EMAudioFBCallbackStateReady:
		iIsOpen = ETrue;
		iUnderflowed = 0;
		break;

	case EMAudioFBCallbackStateDecodeCompleteStopped:
		break;

	//case EMAudioFBCallbackStateDecodeFileSystemError:
	case EMAudioFBCallbackStateDecodeError:
		switch( aError )
		{
		case EMAudioFBCallbackErrorBufferFull:
		case EMAudioFBCallbackErrorForcedStop:
		case EMAudioFBCallbackErrorForcedClose:
		//case EMAudioFBCallbackErrorForcedPause:
		case EMAudioFBCallbackErrorPriorityRejection:
		case EMAudioFBCallbackErrorAlertModeRejection:
		case EMAudioFBCallbackErrorResourceRejection:
		case EMAudioFBCallbackErrorUnknown:
			iUnderflowed++;
			break;

		// these look like really bad errors
		case EMAudioFBCallbackErrorInvalidParameter:
		case EMAudioFBCallbackErrorWrongState:
		case EMAudioFBCallbackErrorFormatNotSupported:
		case EMAudioFBCallbackErrorFunctionNotSupported:
		case EMAudioFBCallbackErrorNoBuffer:
		case EMAudioFBCallbackErrorSampleOrBitRateNotSupported:
		//case EMAudioFBCallbackErrorPriorityOrPreferenceNotSupported:
		//case EMAudioFBCallbackErrorFileSystemFull:
			//iFatalError = ETrue;
			// who cares, just keep retrying
			iUnderflowed++;
			break;

		default:
			iUnderflowed++;
			break;
		}
		// in error condition we also set to open, so that the
		// framework would not leave, catch the error and retry
		iIsOpen = ETrue;
		break;

	default:
		break;
	}
}

void TGameAudioEventListener::OnEvent(TMAudioFBCallbackState aState, TInt aError, TDes8* aBuffer)
{
   switch( aState )
   {
   case EMAudioFBCallbackStateDecodeBufferDecoded:
	  break;

   default:
      OnEvent( aState, aError );
      break;
   }
}

void TGameAudioEventListener::OnEvent(TMAudioACCallbackState aState, TInt aError)
{
	if(aState == EMAudioACCallbackStateReady) iIsCtrlOpen = ETrue;
}

