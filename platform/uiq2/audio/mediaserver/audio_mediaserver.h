/*******************************************************************
 *
 *	File:		Audio_mediaserver.h
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

#include <Mda\Common\Audio.h>
#include <MdaAudioOutputStream.h>

#include "audio.h"
#include "polledas.h"

const TInt KSoundBuffers = 4;
const TInt KBlockTime = 1000000 / 5; // hardcoded: 5 updates/sec
const TInt KMaxLag = 260000; // max sound lag, lower values increase chanse of underflow
const TInt KMaxUnderflows = 50; // max underflows/API errors we are going allow in a row (to prevent lockups)


class TGameAudioEventListener : public MMdaAudioOutputStreamCallback
{
public: // implements MMdaAudioOutputStreamCallback
	void MaoscOpenComplete(TInt aError);
	void MaoscBufferCopied(TInt aError, const TDesC8& );
	void MaoscPlayComplete(TInt aError);

	TBool					iIsOpen;
//	TBool					iHasCopied;
	TInt					iUnderflowed;
};


class CGameAudioMS : public IGameAudio // IGameAudio MUST be specified first!
{
public:	// implements IGameAudio
	TInt16 *NextFrameL();
	TInt16 *DupeFrameL(TInt &aUnderflowed);
	TInt16 *ResumeL();
	void Pause();
	void ChangeVolume(TInt aUp);

public:
	~CGameAudioMS();
	CGameAudioMS(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames);
	void ConstructL();
	EXPORT_C static CGameAudioMS* NewL(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames);

protected:
	void WriteBlockL();
	void UnderflowedL();

protected:
	void WaitForOpenToCompleteL();
//	void WaitForCopyToCompleteL();

	TInt					iRate;
	TBool					iStereo;

	CMdaAudioOutputStream	*iMdaAudioOutputStream;
	TMdaAudioDataSettings	iMdaAudioDataSettings;

	TGameAudioEventListener	iListener;

	CPolledActiveScheduler  *iScheduler;

	HBufC8*					iSoundBuffers[KSoundBuffers+1];
	TInt					iBufferedFrames;
	TInt16*					iCurrentPosition;
	TInt					iCurrentBuffer;
	TInt					iFrameCount;
	TInt					iPcmFrames;
	CMdaServer*				iServer;

	TInt64					iTime;
};

#endif			/* __AUDIO_MEDIASERVER_H */
