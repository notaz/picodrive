/*******************************************************************
 *
 *	File:		Audio_motorola.h
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

#ifndef __AUDIO_MEDIASERVER_H
#define __AUDIO_MEDIASERVER_H

#include <cmaudiofb.h>

#include "audio.h"
#include "polledas.h"

const TInt KSoundBuffers = 8;
const TInt KMaxUnderflows = 20; // max underflows/API errors we are going allow in a row (to prevent lockups)


class TGameAudioEventListener : public MMAudioFBObserver, public MMAudioACObserver
{
public:
	// Implementation of MMAudioFBObserver
	void OnEvent(TMAudioFBCallbackState aState, TInt aError);
	void OnEvent(TMAudioFBCallbackState aState, TInt aError, TDes8* aBuffer);
	// Implementation of MMAudioACObserver
	void OnEvent(TMAudioACCallbackState aState, TInt aError);

	TBool					iIsOpen;
	TBool					iIsCtrlOpen;
//	TBool					iHasCopied;
	TInt					iUnderflowed;
	TBool					iFatalError;
};


class CGameAudioMot : public IGameAudio // IGameAudio MUST be specified first!
{
public:	// implements IGameAudio
	TInt16 *NextFrameL();
	TInt16 *DupeFrameL(TInt &aUnderflowed);
	TInt16 *ResumeL();
	void Pause();
	void ChangeVolume(TInt aUp);

public:
	~CGameAudioMot();
	CGameAudioMot(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames);
	void ConstructL();
	EXPORT_C static CGameAudioMot* NewL(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames);

protected:
	void WriteBlockL();
	void UnderflowedL();

protected:
	void WaitForOpenToCompleteL();

	TInt					iRate;
	TBool					iStereo;

	CMAudioFB				*iAudioOutputStream;
	CMAudioAC				*iAudioControl;
    TMAudioFBBufSettings	iSettings;

	TGameAudioEventListener	iListener;

	CPolledActiveScheduler  *iScheduler;

	HBufC8*					iSoundBuffers[KSoundBuffers+1];
	TPtr8*					iSoundBufferPtrs[KSoundBuffers+1];

	TInt					iBufferedFrames;
	TInt16*					iCurrentPosition;
	TInt					iCurrentBuffer;
	TInt					iFrameCount;
	TInt					iPcmFrames;

	TBool					iDecoding;

	//TInt64					iTime; // removed because can't test
};

#endif			/* __AUDIO_MEDIASERVER_H */
