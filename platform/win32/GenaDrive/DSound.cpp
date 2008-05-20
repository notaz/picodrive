
#include "app.h"

#ifndef _XBOX
#pragma warning (disable:4201)
#include <mmsystem.h>
#include <dsound.h>
#endif

static IDirectSound *DSound=NULL;
static IDirectSoundBuffer *LoopBuffer=NULL;
static int LoopLen=0,LoopWrite=0; // Next position in loop to write

short *DSoundNext=NULL; // Buffer for next sound data to put in loop

static int LoopBlank()
{
  void *mema=NULL,*memb=NULL;
  DWORD sizea=0,sizeb=0;

  LoopBuffer->Lock(0,LoopLen<<((PicoOpt&8) ? 2 : 1), &mema,&sizea, &memb,&sizeb, 0);
  
  if (mema) memset(mema,0,sizea);

  LoopBuffer->Unlock(mema,sizea, memb,sizeb);

  return 0;
}

int DSoundInit()
{
  DSBUFFERDESC dsbd;
  WAVEFORMATEX wfx;

  memset(&dsbd,0,sizeof(dsbd));
  memset(&wfx,0,sizeof(wfx));

  // Make wave format:
  wfx.wFormatTag=WAVE_FORMAT_PCM;
  wfx.nChannels=(unsigned short)((PicoOpt&8) ? 2 : 1); // Stereo/mono
  wfx.nSamplesPerSec=PsndRate;
  wfx.wBitsPerSample=16;

  wfx.nBlockAlign=(WORD)((wfx.nChannels*wfx.wBitsPerSample)>>3);
  wfx.nAvgBytesPerSec=wfx.nBlockAlign*wfx.nSamplesPerSec;

  // Make buffer for the next seg to put into the loop:
  DSoundNext=(short *)malloc((PsndLen<<2)+64); if (DSoundNext==NULL) return 1;
  memset(DSoundNext,0,PsndLen<<2);
//  lprintf("p %p\n", DSoundNext);

  // Create the DirectSound interface:
  DirectSoundCreate(NULL,&DSound,NULL);
  if (DSound==NULL) return 1;

  LoopLen=PsndLen<<1; // 2 segs

#ifndef _XBOX
  LoopLen<<=1; // 4 segs
  DSound->SetCooperativeLevel(FrameWnd,DSSCL_PRIORITY);
  dsbd.dwFlags=DSBCAPS_GLOBALFOCUS;  // Play in background
#endif

  // Create the looping buffer:
  dsbd.dwSize=sizeof(dsbd);
  dsbd.dwBufferBytes=LoopLen<<wfx.nChannels; // 16bit stereo?
  dsbd.lpwfxFormat=&wfx;

  DSound->CreateSoundBuffer(&dsbd,&LoopBuffer,NULL);
  if (LoopBuffer==NULL) return 1;

  LoopBlank();
  LoopBuffer->Play(0,0,DSBPLAY_LOOPING);
  return 0;
}

void DSoundExit()
{
  if (LoopBuffer) LoopBuffer->Stop();
  RELEASE(LoopBuffer)
  RELEASE(DSound)
  DSound=0;
  if (DSoundNext) free(DSoundNext); DSoundNext=NULL;
}

static int WriteSeg()
{
  void *mema=NULL,*memb=NULL;
  DWORD sizea=0,sizeb=0;
  int ret;

  // Lock the segment at 'LoopWrite' and copy the next segment in
  ret = LoopBuffer->Lock(LoopWrite<<((PicoOpt&8) ? 2 : 1),PsndLen<<((PicoOpt&8) ? 2 : 1), &mema,&sizea, &memb,&sizeb, 0);
  if (ret) lprintf("LoopBuffer->Lock() failed: %i\n", ret);

  if (mema) memcpy(mema,DSoundNext,sizea);
//  if (memb) memcpy(memb,DSoundNext+sizea,sizeb);
  if (sizeb != 0) lprintf("sizeb is not 0! (%i)\n", sizeb);

  ret = LoopBuffer->Unlock(mema,sizea, memb,0);
  if (ret) lprintf("LoopBuffer->Unlock() failed: %i\n", ret);

  return 0;
}

static int DSoundFake()
{
  static int ticks_old = 0;
  int ticks = GetTickCount() * 1000;
  int diff;

  diff = ticks - ticks_old;
  if (diff >= 0 && diff < 1000000/60*4)
  {
    while (diff >= 0 && diff < 1000000/60)
    {
      Sleep(1);
      diff = GetTickCount()*1000 - ticks_old;
    }
    ticks_old += 1000000/60;
  }
  else
    ticks_old = ticks;
  return 0;
}

int DSoundUpdate()
{
  DWORD play=0;
  int pos=0;

  if (LoopBuffer==NULL) return DSoundFake();

  LoopBuffer->GetCurrentPosition(&play,NULL);
  pos=play>>((PicoOpt&8) ? 2 : 1);

  // 'LoopWrite' is the next seg in the loop that we want to write
  // First check that the sound 'play' pointer has moved out of it:
  if (pos>=LoopWrite && pos<LoopWrite+PsndLen) return 1; // No, it hasn't

  WriteSeg();

  // Advance LoopWrite to next seg:
  LoopWrite+=PsndLen; if (LoopWrite+PsndLen>LoopLen) LoopWrite=0;

  return 0;
}

