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
#include <pico/pico_int.h>
#include "../common/emu.h"
#include "engine/debug.h"
#include "App.h"

// this is where we start to break a bunch of symbian rules
extern TInt machineUid;
extern int gamestate, gamestate_next;
extern char *loadrom_fname;
extern int   loadrom_result;
extern const char *actionNames[];
RSemaphore initSemaphore;
RSemaphore pauseSemaphore;
RSemaphore loadWaitSemaphore;
static CPicolAppView *appView = 0;


TInt CPicoGameSession::Do(const TPicoServRqst what, TAny *param)
{
	switch (what)
	{
		case PicoMsgLoadState: 
			if(!rom_loaded) return -1; // no ROM
			return emu_save_load_game(1, 0);

		case PicoMsgSaveState:
			if(!rom_loaded) return -1;
			return emu_save_load_game(0, 0);

		case PicoMsgLoadROM:
			return loadROM((TPtrC16 *)param);
		
		case PicoMsgResume:
			DEBUGPRINT(_L("resume"));
			if(rom_loaded) {
				return ChangeRunState(PGS_Running);
			}
			return 1;

		case PicoMsgReset: 
			if(rom_loaded) {
				return ChangeRunState(PGS_Reset);
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
	if (loadWaitSemaphore.Handle() <= 0)
		loadWaitSemaphore.CreateLocal(0);

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
		res = initSemaphore.Wait(3*1000*1000); // wait until it's initialized
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
	TInt ret;
	char buff[150];

	// make sure emu thread is ok
	ret = ChangeRunState(PGS_Paused);
	if(ret) return ret;

	// read the contents of the client pointer into a TPtr.
	static TBuf8<KMaxFileName> writeBuf;
	writeBuf.Copy(*pptr);

	// push the emu thead to a load state. This is done so that it owns all file handles.
	// If successful, in will enter PGS_Running state by itself.
	loadrom_fname = (char *)writeBuf.PtrZ();
	loadrom_result = 0;
	loadWaitSemaphore.Wait(1); // make sure sem is not set
	ret = ChangeRunState(PGS_ReloadRom);
	if(ret) return ret;

	loadWaitSemaphore.Wait(60*1000*1000);

	if (loadrom_result == 0)
		return PicoErrRomOpenFailed;

	emu_get_game_name(buff);
	TPtrC8 buff8((TUint8*) buff);
	iRomInternalName.Copy(buff8);

	DEBUGPRINT(_L("done waiting for ROM load"));

	// debug
	#ifdef __DEBUG_PRINT
	TInt mem, cells = User::CountAllocCells();
	User::AllocSize(mem);
	DEBUGPRINT(_L("comm:   cels=%d, size=%d KB"), cells, mem/1024);
	#endif

	return 0;
}


TInt CPicoGameSession::changeConfig(TPicoConfig *aConfig)
{
	// 6 button pad, enable XYZM config if needed
	if (PicoOpt & POPT_6BTN_PAD)
	{
		actionNames[8]  = "Z";
		actionNames[9]  = "Y";
		actionNames[10] = "X";
		actionNames[11] = "MODE";
	} else {
		actionNames[8] = actionNames[9] = actionNames[10] = actionNames[11] = 0;
	}

	// if we are in center 90||270 modes, we can bind renderer switcher
	if (currentConfig.scaling == TPicoConfig::PMFit &&
		(currentConfig.rotation == TPicoConfig::PRot0 || currentConfig.rotation == TPicoConfig::PRot180))
			 actionNames[25] = 0;
	else actionNames[25] = "RENDERER";

	return 0;
}


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

	if (iThreadWatcher != NULL)
	{
		DEBUGPRINT(_L("delete iThreadWatcher"));
		delete iThreadWatcher;
		DEBUGPRINT(_L("after delete iThreadWatcher"));
		iThreadWatcher = NULL;
	}

	if (initSemaphore.Handle() > 0)
		initSemaphore.Close();
	if (pauseSemaphore.Handle() > 0)
		pauseSemaphore.Close();
	if (loadWaitSemaphore.Handle() > 0)
		loadWaitSemaphore.Close();
	DEBUGPRINT(_L("freeResources() returning"));
#ifdef __DEBUG_PRINT_FILE
	if (logMutex.Handle() > 0)
		logMutex.Close();
#endif
}

TBool CPicoGameSession::iEmuRunning = EFalse;
CThreadWatcher *CPicoGameSession::iThreadWatcher = 0;
TBuf<150> CPicoGameSession::iRomInternalName;


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

extern "C" void cache_flush_d_inval_i(const void *start_addr, const void *end_addr)
{
	// TODO
	User::IMB_Range((TAny *)start_addr, (TAny *)end_addr);
}

