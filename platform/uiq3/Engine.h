/*******************************************************************
 *
 *	File:		Engine.h
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *  Modified/adapted for picodriveN by notaz, 2006
 *
 *  (c) Copyright 2006, notaz
 *	(c) Copyright 2002, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#ifndef __ENGINE_H
#define __ENGINE_H

#include <e32base.h>

class RReadStream;
class RWriteStream;


// engine states
enum TPicoGameState {
	PGS_Running = 1,
	PGS_Paused,
	PGS_Quit,
	PGS_KeyConfig,
	PGS_ReloadRom,
	PGS_Reset,
};

enum TPicoServRqst {
	PicoMsgLoadState,
	PicoMsgSaveState,
	PicoMsgLoadROM,
	PicoMsgResume,
	PicoMsgReset,
	PicoMsgKeys,
	PicoMsgPause,
	PicoMsgQuit,
	PicoMsgConfigChange,
	PicoMsgSetAppView,
	kDefaultMessageSlots // this is how many messages we need :)
};

enum TPicoGenErrors { // generic errors
	PicoErrNoErr = 0, // OK
	PicoErrRomOpenFailed,
	PicoErrOutOfMem,
	PicoErrOutOfMemSnd,
	PicoErrGenSnd, // generic sound system error
	PicoErrEmuThread
};


// needed for creating server thread.
const TUint KPicoMaxHeapSize=0x00800000;

// key config entry (touchpad areas)
struct TPicoAreaConfigEntry {
	TRect rect;
	//unsigned long actions;
};

struct TPicoKeyConfigEntry
{
	unsigned short keyCode;
	unsigned char scanCode;
	unsigned char flags; // lsb->msb: key_down, pulse_only, ?, ?,  ?, ?, not_configurable, disabled
	TInt32 handle1; // for CancelCaptureKeyUpAndDowns()
	TInt32 handle2; // for CancelCaptureKey()
	char *name;
};


// configuration data
class TPicoConfig
{
public:
//	void SetDefaults();
//	void InternalizeL(RReadStream &aStream);
//	void ExternalizeL(RWriteStream &aStream) const;

	enum TPicoScreenRotation {
		PRot0,
		PRot90,
		PRot180,
		PRot270
	};
	enum TPicoScreenMode {
		PMCenter,
		PMFit,
		PMFit2
	};
	enum TPicoFrameSkip {
		PFSkipAuto = -1,
		PFSkip0
	};

public:
	TFileName			iLastROMFile;	// used as tmp only
};


class CThreadWatcher : public CActive
{
public:
	static CThreadWatcher* NewL(const TThreadId& aTid);
	~CThreadWatcher();

	TThreadId			iTid; // thread id

protected:
	CThreadWatcher(const TThreadId& aTid);
	void ConstructL();

	virtual void RunL();
	virtual void DoCancel();
};


class CPicoGameSession
{
public:
	static TInt Do(const TPicoServRqst what, TAny *param=0);
	static void freeResources();

	static TBool iEmuRunning;
	static TBuf<150> iRomInternalName;

private:
	// services available
	static TInt StartEmuThread();
	static TInt ChangeRunState(TPicoGameState newstate, TPicoGameState newstate_next=(TPicoGameState)0);
	static TInt loadROM(TPtrC16 *pptr);
	static TInt changeConfig(TPicoConfig *aConfig);

	static CThreadWatcher *iThreadWatcher;
};

#endif
