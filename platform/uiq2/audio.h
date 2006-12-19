// audio interface, used in picodriveN

#ifndef __AUDIO_H
#define __AUDIO_H

#include <e32std.h>


class IGameAudio : public CBase
{
public:
	virtual TInt16 *NextFrameL() = 0;
	virtual TInt16 *DupeFrameL(TInt &aUnderflowed) = 0;
	virtual TInt16 *ResumeL() = 0;
	virtual void Pause() = 0;
	virtual void ChangeVolume(TInt aUp) = 0; // for Motorolas (experimental)
};


// our audio object maker type
typedef IGameAudio *(*_gameAudioNew)(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames);


#endif			/* __AUDIO_H */
