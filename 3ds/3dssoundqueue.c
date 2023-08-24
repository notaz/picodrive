// TemperPCE for 3DS
//

#include "3dssoundqueue.h"
#define true 1
#define false 0



//=============================================================================
// Sound command queues
//=============================================================================

//-----------------------------------------------------------------------------
// Adds a command to the sound queue if not full. 
//
// This doesn't increment the flushPtr.
//-----------------------------------------------------------------------------
void soundQueueAdd(SSoundQueue *queue, int64_t time, int data, int addr, int value)
{
    if (soundQueueIsFull(queue))
        return;

    queue->queue[queue->writePtr].time = time;
    queue->queue[queue->writePtr].addr = addr;
    queue->queue[queue->writePtr].value = value;
    queue->queue[queue->writePtr].data = data;

    queue->writePtr = (queue->writePtr + 1) % SOUNDQUEUE_SIZE;
}


//-----------------------------------------------------------------------------
// Flushes the queue by setting the flushPtr to be the same as the writePtr.
//-----------------------------------------------------------------------------
void soundQueueFlush(SSoundQueue *queue)
{
    queue->flushPtr = queue->writePtr;
}


//-----------------------------------------------------------------------------
// Reads the next flushed command from the queue but only if the command's 
// timestamp is earlier than the time passed into the parameter.
//-----------------------------------------------------------------------------
int soundQueueRead(SSoundQueue *queue, int64_t time, int *data, int *addr, int *value)
{
    if (soundQueueIsEmpty(queue))
        return false;

    if (time > queue->queue[queue->readPtr].time)
    {
        *addr = queue->queue[queue->readPtr].addr;
        *value = queue->queue[queue->readPtr].value;
        *data = queue->queue[queue->readPtr].data;
        queue->readPtr = (queue->readPtr + 1) % SOUNDQUEUE_SIZE;
        return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// Reads the next flushed command from the queue, regardless of the timestamp.
//-----------------------------------------------------------------------------
int soundQueueReadNext(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value)
{
    if (soundQueueIsEmpty(queue))
        return false;

    *time = queue->queue[queue->readPtr].time;
    *addr = queue->queue[queue->readPtr].addr;
    *value = queue->queue[queue->readPtr].value;
    *data = queue->queue[queue->readPtr].data;
    queue->readPtr = (queue->readPtr + 1) % SOUNDQUEUE_SIZE;
    return true;
}

//-----------------------------------------------------------------------------
// Peeks at the next flushed command without incrementing the readPtr.
//-----------------------------------------------------------------------------
int soundQueuePeekNext(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value)
{
    if (soundQueueIsEmpty(queue))
        return false;

    *time = queue->queue[queue->readPtr].time;
    *addr = queue->queue[queue->readPtr].addr;
    *value = queue->queue[queue->readPtr].value;
    *data = queue->queue[queue->readPtr].data;
    return true;
}

//-----------------------------------------------------------------------------
// Peeks at the last flushed command without incrementing the readPtr.
//-----------------------------------------------------------------------------
int soundQueuePeekLast(SSoundQueue *queue, int64_t *time, int *data, int *addr, int *value)
{
    if (soundQueueIsEmpty(queue))
        return false;

    int flushPtr = queue->flushPtr - 1;
    if (flushPtr < 0)
        flushPtr += SOUNDQUEUE_SIZE;
    *time = queue->queue[flushPtr].time;
    *addr = queue->queue[flushPtr].addr;
    *value = queue->queue[flushPtr].value;
    *data = queue->queue[flushPtr].data;
    return true;
}

//-----------------------------------------------------------------------------
// Counts how many items are in the queue.
//-----------------------------------------------------------------------------
int soundQueueGetLength(SSoundQueue *queue)
{
    int length = queue->flushPtr - queue->readPtr;
    if (length < 0)
        length += SOUNDQUEUE_SIZE;
    return length;
}


//-----------------------------------------------------------------------------
// Checks if the sound queue is empty.
//-----------------------------------------------------------------------------
int soundQueueIsEmpty(SSoundQueue *queue)
{
    return (queue->readPtr == queue->flushPtr);
}

//-----------------------------------------------------------------------------
// Checks if the sound queue is full.
//-----------------------------------------------------------------------------
int soundQueueIsFull(SSoundQueue *queue)
{
    return (((queue->writePtr + 1) % SOUNDQUEUE_SIZE) == queue->readPtr);
}


//-----------------------------------------------------------------------------
// Resets the sound queue.
//-----------------------------------------------------------------------------
void soundQueueReset(SSoundQueue *queue)
{
    queue->readPtr = 0;
    queue->writePtr = 0;
    queue->flushPtr = 0;
    memset(queue->queue, 0, sizeof(SOUNDQUEUE_SIZE * 2));
}



//=============================================================================
// Digital sample output queues.
//=============================================================================

//-----------------------------------------------------------------------------
// Add a digital sample to the queue if not full.
//-----------------------------------------------------------------------------
void dacQueueAdd(SDACQueue *queue, short sample)
{
    if (dacQueueIsFull(queue))
        return;

    queue->queueLeft[queue->writePtr] = sample;
    queue->writePtr = (queue->writePtr + 1) % DACQUEUE_SIZE;
}

//-----------------------------------------------------------------------------
// Add a digital stereo sample to the queue if not full.
//-----------------------------------------------------------------------------
void dacQueueAddStereo(SDACQueue *queue, short leftSample, short rightSample)
{
    if (dacQueueIsFull(queue))
        return;

    queue->queueLeft[queue->writePtr] = leftSample;
    queue->queueRight[queue->writePtr] = rightSample;
    queue->writePtr = (queue->writePtr + 1) % DACQUEUE_SIZE;
}

//-----------------------------------------------------------------------------
// Read a digital sample from the queue if not empty.
//-----------------------------------------------------------------------------
int dacQueueRead(SDACQueue *queue, short *sample)
{
    if (dacQueueIsEmpty(queue))
        return false;

    *sample = queue->queueLeft[queue->readPtr];
    queue->readPtr = (queue->readPtr + 1) % DACQUEUE_SIZE;
}

//-----------------------------------------------------------------------------
// Read a digital sample from the queue if not empty.
//-----------------------------------------------------------------------------
int dacQueueReadStereo(SDACQueue *queue, short *leftSample, short *rightSample)
{
    if (dacQueueIsEmpty(queue))
        return false;

    *leftSample = queue->queueLeft[queue->readPtr];
    *rightSample = queue->queueRight[queue->readPtr];
    queue->readPtr = (queue->readPtr + 1) % DACQUEUE_SIZE;
}

//-----------------------------------------------------------------------------
// Counts how many samples are in the queue.
//-----------------------------------------------------------------------------
int dacQueueGetLength(SDACQueue *queue)
{
    int length = queue->writePtr - queue->readPtr;
    if (length < 0)
        length += DACQUEUE_SIZE;
    return length;
}


//-----------------------------------------------------------------------------
// Checks if the digital sample queue is empty.
//-----------------------------------------------------------------------------
int dacQueueIsEmpty(SDACQueue *queue)
{
    return queue->writePtr == queue->readPtr;
}


//-----------------------------------------------------------------------------
// Checks if the digital sample queue is full.
//-----------------------------------------------------------------------------
int dacQueueIsFull(SDACQueue *queue)
{
    return ((queue->writePtr + 1) % DACQUEUE_SIZE) == queue->readPtr;
}


//-----------------------------------------------------------------------------
// Waits until the dac queue length is less than or equals to
//  the specified value.
//-----------------------------------------------------------------------------
int dacQueueWaitUntilLength(SDACQueue *queue, int length, int numberOfRetries, int64_t nanosecondsToWait)
{
    while (numberOfRetries > 0 && dacQueueGetLength(queue) > length)
    {
        svcSleepThread(nanosecondsToWait);
        numberOfRetries--;
    }
}

//-----------------------------------------------------------------------------
// Waits until the dac queue is not full.
//-----------------------------------------------------------------------------
int dacQueueWaitUntilNotFull(SDACQueue *queue, int numberOfRetries, int64_t nanosecondsToWait)
{
    while (numberOfRetries > 0 && ((queue->writePtr + 1) % DACQUEUE_SIZE) == queue->readPtr)
    {
        svcSleepThread(nanosecondsToWait);
        numberOfRetries--;
    }
}


//-----------------------------------------------------------------------------
// Resets the digital sample queue.
//-----------------------------------------------------------------------------
void dacQueueReset(SDACQueue *queue)
{
    queue->readPtr = 0;
    queue->writePtr = 0;
    memset(queue->queueLeft, 0, sizeof(DACQUEUE_SIZE * 2));
    memset(queue->queueRight, 0, sizeof(DACQUEUE_SIZE * 2));
}
