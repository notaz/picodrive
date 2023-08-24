#ifndef _3DSSOUNDQUEUE_H
#define _3DSSOUNDQUEUE_H

#include "inttypes.h"

#define SOUNDQUEUE_SIZE         8192
#define DACQUEUE_SIZE           4096

typedef struct
{
    int64_t     time;
    int         addr;
    int         value;
    int         data;
} SSoundItem;

typedef struct
{
    int             writePtr;
    int             readPtr;
    int             flushPtr;
    SSoundItem      queue[SOUNDQUEUE_SIZE];
} SSoundQueue;

typedef struct
{
    int             writePtr;
    int             readPtr;
    int16_t         queueLeft[DACQUEUE_SIZE];
    int16_t         queueRight[DACQUEUE_SIZE];
} SDACQueue;

extern SSoundQueue soundQueue;

#ifdef EXTERN_C_START
EXTERN_C_START
#endif 

//=============================================================================
// Sound command queues
//=============================================================================
//-----------------------------------------------------------------------------
// Adds a command to the sound queue if not full. 
//
// This doesn't increment the flushPtr.
//-----------------------------------------------------------------------------
void soundQueueAdd(SSoundQueue *queue, int64_t time, int data, int addr, int value);

//-----------------------------------------------------------------------------
// Flushes the queue by setting the flushPtr to be the same as the writePtr.
//-----------------------------------------------------------------------------
void soundQueueFlush(SSoundQueue *queue);

//-----------------------------------------------------------------------------
// Reads the next flushed command from the queue but only if the command's 
// timestamp is earlier than the time passed into the parameter.
//-----------------------------------------------------------------------------
int soundQueueRead(SSoundQueue *queue, int64_t time, int *data, int *addr, int *value);

//-----------------------------------------------------------------------------
// Reads the next flushed command from the queue, regardless of the timestamp.
//-----------------------------------------------------------------------------
int soundQueueReadNext(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value);

//-----------------------------------------------------------------------------
// Peeks at the next flushed command without incrementing the readPtr.
//-----------------------------------------------------------------------------
int soundQueuePeekNext(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value);

//-----------------------------------------------------------------------------
// Peeks at the last flushed command without incrementing the readPtr.
//-----------------------------------------------------------------------------
int soundQueuePeekLast(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value);

//-----------------------------------------------------------------------------
// Counts how many items are in the queue.
//-----------------------------------------------------------------------------
int soundQueueGetLength(SSoundQueue *queue);

//-----------------------------------------------------------------------------
// Checks if the sound queue is empty.
//-----------------------------------------------------------------------------
int soundQueueIsEmpty(SSoundQueue *queue);

//-----------------------------------------------------------------------------
// Checks if the sound queue is full.
//-----------------------------------------------------------------------------
int soundQueueIsFull(SSoundQueue *queue);

//-----------------------------------------------------------------------------
// Resets the sound queue.
//-----------------------------------------------------------------------------
void soundQueueReset(SSoundQueue *queue);


//=============================================================================
// Digital sample output queues.
//=============================================================================
//-----------------------------------------------------------------------------
// Add a digital sample to the queue if not full.
//-----------------------------------------------------------------------------
void dacQueueAdd(SDACQueue *queue, short sample);

//-----------------------------------------------------------------------------
// Add a digital stereo sample to the queue if not full.
//-----------------------------------------------------------------------------
void dacQueueAddStereo(SDACQueue *queue, short leftSample, short rightSample);

//-----------------------------------------------------------------------------
// Read a digital sample from the queue if not empty.
//-----------------------------------------------------------------------------
int dacQueueRead(SDACQueue *queue, short *sample);

//-----------------------------------------------------------------------------
// Read a digital stereo sample from the queue if not empty.
//-----------------------------------------------------------------------------
int dacQueueReadStereo(SDACQueue *queue, short *leftSample, short *rightSample);

//-----------------------------------------------------------------------------
// Counts how many samples are in the queue.
//-----------------------------------------------------------------------------
int dacQueueGetLength(SDACQueue *queue);

//-----------------------------------------------------------------------------
// Checks if the digital sample queue is empty.
//-----------------------------------------------------------------------------
int dacQueueIsEmpty(SDACQueue *queue);

//-----------------------------------------------------------------------------
// Checks if the digital sample queue is full.
//-----------------------------------------------------------------------------
int dacQueueIsFull(SDACQueue *queue);

//-----------------------------------------------------------------------------
// Waits until the dac queue length is less than the specified value.
//-----------------------------------------------------------------------------
int dacQueueWaitUntilLength(SDACQueue *queue, int length, int numberOfRetries, int64_t nanosecondsToWait);

//-----------------------------------------------------------------------------
// Waits until the dac queue is not full.
//-----------------------------------------------------------------------------
int dacQueueWaitUntilNotFull(SDACQueue *queue, int numberOfRetries, int64_t nanosecondsToWait);

//-----------------------------------------------------------------------------
// Resets the digital sample queue.
//-----------------------------------------------------------------------------
void dacQueueReset(SDACQueue *queue);

#ifdef EXTERN_C_END
EXTERN_C_END
#endif 

#endif
