/*******************************************************************
 *
 *	File:		Engine.cpp
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


#include "Engine.h"
#include <w32std.h>
#include <eikenv.h>
#include <e32svr.h>
#include <e32math.h>
#include <e32uid.h>

#include <string.h>

#include "version.h"
#include "../../pico/picoInt.h"
#include "engine/debug.h"
#include "app.h"

// this is where we start to break a bunch of symbian rules
extern TInt machineUid;
extern int gamestate, gamestate_next;
extern TPicoConfig *currentConfig;
extern const char *actionNames[];
RSemaphore pauseSemaphore;
RSemaphore initSemaphore;
const char *RomFileName = 0;
int pico_was_reset = 0;
unsigned char *rom_data = 0;
static CPicolAppView *appView = 0;


TInt CPicoGameSession::Do(const TPicoServRqst what, TAny *param)
{
	switch (what) {
		case PicoMsgLoadState: 
			if(!rom_data) return -1; // no ROM
			return saveLoadGame(1);

		case PicoMsgSaveState:
			if(!rom_data) return -1;
			return saveLoadGame(0);

		case PicoMsgLoadROM:
			return loadROM((TPtrC16 *)param);
		
		case PicoMsgResume:
			DEBUGPRINT(_L("resume with rom %08x"), rom_data);
			if(rom_data) {
				return ChangeRunState(PGS_Running);
			}
			return 1;

		case PicoMsgReset: 
			if(rom_data) {
				PicoReset(0);
				pico_was_reset = 1;
				return ChangeRunState(PGS_Running);
			}
			return 1;

		case PicoMsgKeys:
			return ChangeRunState(PGS_KeyConfig);

		case PicoMsgPause:
			return ChangeRunState(PGS_Paused);

		case PicoMsgQuit:
			DEBUGPRINT(_L("got quit msg."));
			return ChangeRunState(PGS_Quit);

		// config change
		case PicoMsgConfigChange:
			return changeConfig((TPicoConfig *)param);

		case PicoMsgSetAppView:
			appView = (CPicolAppView *)param;
			return 1;

		default:
			return 1;
	}
}

TInt EmuThreadFunction(TAny* anArg);

TInt CPicoGameSession::StartEmuThread()
{
	TInt res=KErrNone;
	iEmuRunning = EFalse;

	if (initSemaphore.Handle() > 0)
		initSemaphore.Close();
	initSemaphore.CreateLocal(0);
	if (pauseSemaphore.Handle() <= 0)
		pauseSemaphore.CreateLocal(0);

	RThread thread;
	if(iThreadWatcher && (res = thread.Open(iThreadWatcher->iTid)) == KErrNone) {
		// should be a dead thread in some strange state.
		DEBUGPRINT(_L("found thread with the same id (id=%i, RequestCount=%i), killing.."),
				(TInt32)thread.Id(), thread.RequestCount());
		// what can we do in this situation? Nothing seems to help, it just stays in this state.
		delete iThreadWatcher;
		iThreadWatcher = 0;
		thread.Kill(1);
		thread.Terminate(1);
		thread.Close();
	}

	//semaphore.CreateLocal(0); // create a semaphore so we know when thread init is finished
	res=thread.Create(_L("PicoEmuThread"),   // create new server thread
		EmuThreadFunction, // thread's main function
		KDefaultStackSize,
		KMinHeapSize,
		KPicoMaxHeapSize,
		0 // &semaphore // passed as TAny* argument to thread function
		);

	if(res == KErrNone) { // thread created ok - now start it going
		thread.SetPriority(EPriorityMore);
		iEmuRunning = ETrue;
		if (iThreadWatcher) delete iThreadWatcher;
		iThreadWatcher = CThreadWatcher::NewL(thread.Id());
		thread.Resume(); // start it going
		DEBUGPRINT(_L("initSemaphore.Wait()"));
		res = initSemaphore.Wait(1000*1000); // wait until it's initialized
		DEBUGPRINT(_L("initSemaphore resume, ExitReason() == %i"), thread.ExitReason());
		res |= thread.ExitReason();
		thread.Close(); // we're no longer interested in the other thread
		if(res != KErrNone) iEmuRunning = EFalse;
		return res;
	}

	return res;
}

TInt CPicoGameSession::ChangeRunState(TPicoGameState newstate, TPicoGameState newstate_next)
{
	if (!iEmuRunning) {
		gamestate = PGS_Paused;
		TInt res = StartEmuThread();
		if(res != KErrNone) DEBUGPRINT(_L("StartEmuThread() returned %i"), res);
		if (!iEmuRunning) return PicoErrEmuThread;
	}

	int oldstate = gamestate;
	gamestate = newstate;
	gamestate_next = newstate_next ? newstate_next : PGS_Paused;
	if (oldstate == PGS_Paused) pauseSemaphore.Signal();
	return 0;
}


TInt CPicoGameSession::loadROM(TPtrC16 *pptr)
{
	TInt res, i;
	char buff[0x31];

	if(rom_data) {
		// save SRAM for previous ROM
		if(currentConfig->iFlags & 1)
			saveLoadGame(0, 1);
	}

	RomFileName = 0;
	if(rom_data) {
		free(rom_data);
		rom_data = 0;
	}

	// read the contents of the client pointer into a TPtr.
	static TBuf8<KMaxFileName> writeBuf;
	writeBuf.Copy(*pptr);

	// detect wrong extensions (.srm and .mds)
	TBuf8<5> ext;
	ext.Copy(writeBuf.Right(4));
	ext.LowerCase();
	if(!strcmp((char *)ext.PtrZ(), ".srm") || !strcmp((char *)ext.PtrZ(), "s.gz") || // .mds.gz
	   !strcmp((char *)ext.PtrZ(), ".mds")) {
		return PicoErrNotRom;
	}

	FILE *rom = fopen((char *) writeBuf.PtrZ(), "rb");
	if(!rom) {
		DEBUGPRINT(_L("failed to open rom."));
		return PicoErrRomOpenFailed;
	}

	// make sure emu thread is ok
	res = ChangeRunState(PGS_Paused);
	if(res) {
		fclose(rom);
		return res;
	}

	unsigned int rom_size = 0;
	// zipfile support
	if(!strcmp((char *)ext.PtrZ(), ".zip")) {
		fclose(rom);
		res = CartLoadZip((const char *) writeBuf.PtrZ(), &rom_data, &rom_size);
		if(res) {
			DEBUGPRINT(_L("CartLoadZip() failed (%i)"), res);
			return res;
		}
	} else {
		if( (res = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
			DEBUGPRINT(_L("PicoCartLoad() failed (%i)"), res);
			fclose(rom);
			return PicoErrOutOfMem;
		}
		fclose(rom);
	}

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if(rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(TUint16 *)(rom_data+4)<<16)|(*(TUint16 *)(rom_data+6))) >= (int)rom_size) {
		free(rom_data);
		rom_data = 0;
		return PicoErrNotRom;
	}

	DEBUGPRINT(_L("PicoCartInsert(0x%08X, %d);"), rom_data, rom_size);
	if(PicoCartInsert(rom_data, rom_size)) {
		return PicoErrOutOfMem;
	}

	pico_was_reset = 1;

	// global ROM file name for later use
	RomFileName = (const char *) writeBuf.PtrZ();

	// name from the ROM itself
	for(i = 0; i < 0x30; i++)
		buff[i] = rom_data[0x150 + (i^1)]; // unbyteswap
	for(buff[i] = 0, i--; i >= 0; i--) {
		if(buff[i] != ' ') break;
		buff[i] = 0;
	}
	TPtrC8 buff8((TUint8*) buff);
	iRomInternalName.Copy(buff8);

	// load SRAM for this ROM
	if(currentConfig->iFlags & 1)
		saveLoadGame(1, 1);

	// debug
	#ifdef __DEBUG_PRINT
	TInt cells = User::CountAllocCells();
	TInt mem;
	User::AllocSize(mem);
	DEBUGPRINT(_L("comm:   cels=%d, size=%d KB"), cells, mem/1024);
	ChangeRunState(PGS_DebugHeap, PGS_Running);
	#else
	ChangeRunState(PGS_Running);
	#endif

	return 0;
}


TInt CPicoGameSession::changeConfig(TPicoConfig *aConfig)
{
	DEBUGPRINT(_L("got new config."));

	currentConfig = aConfig;

	// set PicoOpt and rate
	PicoRegionOverride = currentConfig->PicoRegion;
	PicoOpt = currentConfig->iPicoOpt;
	switch((currentConfig->iFlags>>3)&7) {
		case 1:  PsndRate=11025; break;
		case 2:  PsndRate=16000; break;
		case 3:  PsndRate=22050; break;
		case 4:  PsndRate=44100; break;
		default: PsndRate= 8000; break;
	}

	// 6 button pad, enable XYZM config if needed
	if(PicoOpt & 0x20) {
		actionNames[8]  = "Z";
		actionNames[9]  = "Y";
		actionNames[10] = "X";
		actionNames[11] = "MODE";
	} else {
		actionNames[8] = actionNames[9] = actionNames[10] = actionNames[11] = 0;
	}

	// if we are in center 90||270 modes, we can bind renderer switcher
	if(currentConfig->iScreenMode == TPicoConfig::PMFit &&
		(currentConfig->iScreenRotation == TPicoConfig::PRot0 || currentConfig->iScreenRotation == TPicoConfig::PRot180))
				 actionNames[25] = 0;
			else actionNames[25] = "RENDERER";

	return 0;
}


void MainOldCleanup(); // from main.cpp
#ifdef __DEBUG_PRINT_FILE
extern RMutex logMutex;
#endif

void CPicoGameSession::freeResources()
{
	RThread thread;
	TInt i;

	DEBUGPRINT(_L("CPicoGameSession::freeResources()"));

	if(iThreadWatcher && thread.Open(iThreadWatcher->iTid) == KErrNone)
	{
		// try to stop our emu thread
		gamestate = PGS_Quit;
		if(pauseSemaphore.Handle() > 0)
			pauseSemaphore.Signal();

		if(thread.Handle() > 0)
		{
			// tried reopening thread handle here over time intervals to detect if thread is alive,
			// but would run into handle panics.

			for(i = 0; i < 8; i++) {
				User::After(100 * 1000);
				if(thread.ExitReason() != 0) break;
			}

			if(thread.ExitReason() == 0) {
				// too late, time to die
				DEBUGPRINT(_L("thread %i not responding, killing.."), (TInt32) thread.Id());
				thread.Terminate(1);
			}
			thread.Close();
		}

	}

	if(iThreadWatcher != NULL)
	{
		DEBUGPRINT(_L("delete iThreadWatcher"));
		delete iThreadWatcher;
		DEBUGPRINT(_L("after delete iThreadWatcher"));
		iThreadWatcher = NULL;
	}

	MainOldCleanup();

	if (initSemaphore.Handle() > 0)
		initSemaphore.Close();
	if (pauseSemaphore.Handle() > 0)
		pauseSemaphore.Close();
#ifdef __DEBUG_PRINT_FILE
	if (logMutex.Handle() > 0)
		logMutex.Close();
#endif
}

TBool CPicoGameSession::iEmuRunning = EFalse;
CThreadWatcher *CPicoGameSession::iThreadWatcher = 0;
TBuf<0x30> CPicoGameSession::iRomInternalName;


void TPicoConfig::SetDefaults()
{
	iLastROMFile.SetLength(0);
	iScreenRotation = PRot270;
	iScreenMode     = PMCenter;
	iFlags          = 1; // use_sram
	iPicoOpt        = 0; // all off
	iFrameskip      = PFSkipAuto;

	Mem::FillZ(iKeyBinds,  sizeof(iKeyBinds));
	Mem::FillZ(iAreaBinds, sizeof(iAreaBinds));
	iKeyBinds[0xd5] = 1<<26; // bind back
}

// load config
void TPicoConfig::InternalizeL(RReadStream &aStream)
{
	TInt32 version, fname_len;
	version = aStream.ReadInt32L();
	fname_len       = aStream.ReadInt32L();

	// not sure if this is safe
	iLastROMFile.SetMax();
	aStream.ReadL((TUint8 *) iLastROMFile.Ptr(), KMaxFileName*2);
	iLastROMFile.SetLength(fname_len);

	iScreenRotation = aStream.ReadInt32L();
	iScreenMode     = aStream.ReadInt32L();
	iFlags          = aStream.ReadUint32L();
	iPicoOpt        = aStream.ReadInt32L();
	iFrameskip      = aStream.ReadInt32L();

	aStream.ReadL((TUint8 *)iKeyBinds,  sizeof(iKeyBinds));
	aStream.ReadL((TUint8 *)iAreaBinds, sizeof(iAreaBinds));

	PicoRegion      = aStream.ReadInt32L();
}

// save config
void TPicoConfig::ExternalizeL(RWriteStream &aStream) const
{
	TInt version = (KPicoMajorVersionNumber<<24)+(KPicoMinorVersionNumber<<16);

	aStream.WriteInt32L(version);
	aStream.WriteInt32L(iLastROMFile.Length());
	aStream.WriteL((const TUint8 *)iLastROMFile.Ptr(), KMaxFileName*2);

	aStream.WriteInt32L(iScreenRotation);
	aStream.WriteInt32L(iScreenMode);
	aStream.WriteUint32L(iFlags);
	aStream.WriteInt32L(iPicoOpt);
	aStream.WriteInt32L(iFrameskip);

	aStream.WriteL((const TUint8 *)iKeyBinds,  sizeof(iKeyBinds));
	aStream.WriteL((const TUint8 *)iAreaBinds, sizeof(iAreaBinds));

	aStream.WriteInt32L(PicoRegion);
}


// CThreadWatcher
CThreadWatcher::~CThreadWatcher()
{
	Cancel();
	DEBUGPRINT(_L("after CThreadWatcher::Cancel();"));
}

CThreadWatcher::CThreadWatcher(const TThreadId& aTid)
: CActive(CActive::EPriorityStandard), iTid(aTid)
{
}


CThreadWatcher* CThreadWatcher::NewL(const TThreadId& aTid)
{
	CThreadWatcher* self = new(ELeave) CThreadWatcher(aTid);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();			// self
	return self;
}

void CThreadWatcher::ConstructL()
{
	CActiveScheduler::Add(this);
	RThread thread;
	if(thread.Open(iTid) == KErrNone) {
		thread.Logon(iStatus);
		thread.Close();
		SetActive();
	}
}

void CThreadWatcher::RunL()
{
	DEBUGPRINT(_L("CThreadWatcher::RunL()"));
	CPicoGameSession::iEmuRunning = EFalse;
	if(appView) appView->UpdateCommandList();
	//initSemaphore.Signal(); // no point to do that here, AS can't get here if it is waiting
}

void CThreadWatcher::DoCancel()
{
	RThread thread;
	DEBUGPRINT(_L("CThreadWatcher::DoCancel()"));
	if(thread.Open(iTid) == KErrNone) {
		DEBUGPRINT(_L("thread.LogonCancel(iStatus);"));
		thread.LogonCancel(iStatus);
		thread.Close();
	}
}
