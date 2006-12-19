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

//#include "audio.h"
#include "polledas.h"

const TInt KSoundBuffers = 4;


class TGameAudioEventListener : public MMdaAudioOutputStreamCallback
{
public: // implements MMdaAudioOutputStreamCallback
	void MaoscOpenComplete(TInt aError);
	void MaoscBufferCopied(TInt aError, const TDesC8& );
	void MaoscPlayComplete(TInt aError);

	TBool					iIsOpen;
//	TBool					iHasCopied;
	TInt					iUnderflowed;
	TInt					iLastError;
};


class CGameAudioMS // : public IGameAudio // IGameAudio MUST be specified first!
{
public:	// implements IGameAudio
	TInt16 *NextFrameL(TInt aPcmFrames);
	TInt16 *ResumeL();
	void Pause();
	void ChangeVolume(TInt aUp);

public:
	~CGameAudioMS();
	CGameAudioMS(TInt aRate, TBool aStereo, TInt aWritesPerSec);
	static CGameAudioMS* NewL(TInt aRate, TBool aStereo, TInt aWritesPerSec);

protected:
	void WriteBlockL();
	void UnderflowedL();
	void ConstructL();

protected:
	void WaitForOpenToCompleteL();

	TInt					iRate;
	TBool					iStereo;

	CMdaAudioOutputStream	*iMdaAudioOutputStream;
	TMdaAudioDataSettings	iMdaAudioDataSettings;

	TGameAudioEventListener	iListener;

	CPolledActiveScheduler  *iScheduler;

	HBufC8*					iSoundBuffers[KSoundBuffers];
	TInt					iWritesPerSec;
	TInt					iBufferedFrames;
	TInt16*					iCurrentPosition;
	TInt					iCurrentBuffer;
	TInt					iCurrentBufferSize;
	TInt					iFrameCount;
	CMdaServer*				iServer;

	TInt64					iTime;
	TInt					iVolume;
};

#endif			/* __AUDIO_MEDIASERVER_H */
