// mainloop with window server event handling
// event polling mechnism was taken from
// Peter van Sebille's projects

// (c) Copyright 2006, notaz
// All Rights Reserved

#include <e32base.h>
#include <hal.h>
#include <e32keys.h>
#include <w32std.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "debug.h"
#include "../Engine.h"

#include <Pico/PicoInt.h>
#include "../../common/emu.h"
#include "../emu.h"
#include "vid.h"
#include "PolledAS.h"
//#include "audio.h"
#include "audio_mediaserver.h"

//#include <ezlib.h>
#include <zlib/zlib.h>


//#define BENCHMARK


// scancodes we care about
enum TUsedScanCodes {
	EStdKeyM600JogUp   = EStdKeyDevice1,
	EStdKeyM600JogDown = EStdKeyDevice2,
};

static unsigned char keyFlags[256];   // lsb->msb: key_down, pulse_only, ?, ?,  ?, ?, not_configurable, disabled
static unsigned char pressedKeys[11]; // List of pressed key scancodes, up to 10

// list of areas
TPicoAreaConfigEntry areaConfig[] = {
	{ TRect(  0,   0,   0,   0) },
	// small corner bottons
	{ TRect(  0,   0,  15,  15) },
	{ TRect(224,   0, 239,  15) },
	{ TRect(  0, 304,  15, 319) },
	{ TRect(224, 304, 239, 319) },
	// normal buttons
	{ TRect(  0,   0,  79,  63) },
	{ TRect( 80,   0, 159,  63) },
	{ TRect(160,   0, 239,  63) },
	{ TRect(  0,  64,  79, 127) },
	{ TRect( 80,  64, 159, 127) },
	{ TRect(160,  64, 239, 127) },
	{ TRect(  0, 128,  79, 191) },
	{ TRect( 80, 128, 159, 191) },
	{ TRect(160, 128, 239, 191) },
	{ TRect(  0, 192,  79, 255) },
	{ TRect( 80, 192, 159, 255) },
	{ TRect(160, 192, 239, 255) },
	{ TRect(  0, 256,  79, 319) },
	{ TRect( 80, 256, 159, 319) },
	{ TRect(160, 256, 239, 319) },
	{ TRect(  0,   0,   0,   0) }
};

// PicoPad[] format: SACB RLDU
const char *actionNames[] = {
	"UP", "DOWN", "LEFT", "RIGHT", "B", "C", "A", "START",
	0, 0, 0, 0, 0, 0, 0, 0, // Z, Y, X, MODE (enabled only when needed), ?, ?, ?, ?
	0, 0, 0, 0, "VOLUME@UP", "VOLUME@DOWN", "NEXT@SAVE@SLOT", "PREV@SAVE@SLOT", // ?, ?, ?, ?, vol_up, vol_down, next_slot, prev_slot
	0, 0, "PAUSE@EMU", "SAVE@STATE", "LOAD@STATE", 0, 0, "DONE" // ?, switch_renderer, [...], "FRAMESKIP@8", "AUTO@FRAMESKIP"
};


// globals are allowed, so why not to (ab)use them?
//TInt machineUid = 0;
int gamestate = PGS_Paused, gamestate_next = PGS_Paused;
char *loadrom_fname = NULL;
int   loadrom_result = 0;
static timeval noticeMsgTime = { 0, 0 };	// when started showing
static CGameAudioMS *gameAudio = 0;			// the audio object itself
static int reset_timing;
extern int pico_was_reset;
extern RSemaphore initSemaphore;
extern RSemaphore pauseSemaphore;
extern RSemaphore loadWaitSemaphore;

// some forward declarations
static void MainInit();
static void MainExit();
static void DumpMemInfo();


class TPicoDirectScreenAccess : public MDirectScreenAccess
{
public: // implements MDirectScreenAccess
	void Restart(RDirectScreenAccess::TTerminationReasons aReason);
public: // implements MAbortDirectScreenAccess
	void AbortNow(RDirectScreenAccess::TTerminationReasons aReason);
};


// just for a nicer grouping of WS related stuff
class CGameWindow
{
public:
	static void ConstructResourcesL(void);
	static void FreeResources(void);
	static void DoKeys(void);
	static void DoKeysConfig(TUint &which);
	static void RunEvents(TUint32 which);

	static RWsSession*				iWsSession;
	static RWindowGroup				iWsWindowGroup;
	static RWindow					iWsWindow;
	static CWsScreenDevice*			iWsScreen;
	static CWindowGc*				iWindowGc;
	static TRequestStatus			iWsEventStatus;
//	static TThreadId				iLauncherThreadId;
//	static RDirectScreenAccess*		iDSA;
//	static TRequestStatus			iDSAstatus;
	static TPicoDirectScreenAccess	iPDSA;
	static CDirectScreenAccess*		iDSA;
};


static void updateSound(int len)
{
	PsndOut = gameAudio->NextFrameL(len);
	if(!PsndOut) { // sound output problems?
		strcpy(noticeMsg, "SOUND@OUTPUT@ERROR;@SOUND@DISABLED");
		gettimeofday(&noticeMsgTime, 0);
	}
}


static void SkipFrame(void)
{
	PicoSkipFrame=1;
	PicoFrame();
	PicoSkipFrame=0;
}


static void simpleWait(int thissec, int lim_time)
{
	struct timeval tval;
	int sleep = 0;

	gettimeofday(&tval, 0);
	if(thissec != tval.tv_sec) tval.tv_usec+=1000000;

	sleep = lim_time - tval.tv_usec - 2000;
	if (sleep > 0) {
//		User::After((sleep = lim_time - tval.tv_usec));
		User::AfterHighRes(sleep);
	}
}


static void TargetEpocGameL()
{
	char buff[24]; // fps count c string
	struct timeval tval; // timing
	int thissec = 0, frames_done = 0, frames_shown = 0;
	int target_fps, target_frametime;
	int i, lim_time;
	//TRawEvent blevent;

	MainInit();
	buff[0] = 0;

	PicoInit();

	// just to keep the backlight on (works only on UIQ2)
	//blevent.Set(TRawEvent::EActive);

	// loop?
	for(;;)
	{
		if (gamestate == PGS_Running)
		{
			// switch context to other thread
			User::After(50000);
			// prepare window and stuff
			CGameWindow::ConstructResourcesL();

			// if the system has something to do, it should better do it now
			User::After(50000);
			//CPolledActiveScheduler::Instance()->Schedule();

			// pal/ntsc might have changed, reset related stuff
			if(Pico.m.pal) {
				target_fps = 50;
				if(!noticeMsgTime.tv_sec) strcpy(noticeMsg, "PAL@SYSTEM@/@50@FPS");
			} else {
				target_fps = 60;
				if(!noticeMsgTime.tv_sec) strcpy(noticeMsg, "NTSC@SYSTEM@/@60@FPS");
			}
			target_frametime = 1000000/target_fps;
			if(!noticeMsgTime.tv_sec && pico_was_reset)
				gettimeofday(&noticeMsgTime, 0);

			// prepare CD buffer
			if (PicoAHW & PAHW_MCD) PicoCDBufferInit();

			pico_was_reset = 0;
			reset_timing = 1;

			while (gamestate == PGS_Running)
			{
				gettimeofday(&tval, 0);
				if(reset_timing) {
					reset_timing = 0;
					thissec = tval.tv_sec;
					frames_done = tval.tv_usec/target_frametime;
				}

				// show notice message?
				char *notice = 0;
				if(noticeMsgTime.tv_sec) {
					if((tval.tv_sec*1000000+tval.tv_usec) - (noticeMsgTime.tv_sec*1000000+noticeMsgTime.tv_usec) > 2000000) // > 2.0 sec
						 noticeMsgTime.tv_sec = noticeMsgTime.tv_usec = 0;
					else notice = noticeMsg;
				}

				// second changed?
				if (thissec != tval.tv_sec)
				{
#ifdef BENCHMARK
					static int bench = 0, bench_fps = 0, bench_fps_s = 0, bfp = 0, bf[4];
					if(++bench == 10) {
						bench = 0;
						bench_fps_s = bench_fps;
						bf[bfp++ & 3] = bench_fps;
						bench_fps = 0;
					}
					bench_fps += frames_shown;
					sprintf(buff, "%02i/%02i/%02i", frames_shown, bench_fps_s, (bf[0]+bf[1]+bf[2]+bf[3])>>2);
#else
					if (currentConfig.EmuOpt & EOPT_SHOW_FPS) 
						sprintf(buff, "%02i/%02i", frames_shown, frames_done);
#endif


					thissec = tval.tv_sec;

					if(PsndOut == 0 && currentConfig.Frameskip >= 0) {
						frames_done = frames_shown = 0;
					} else {
						// it is quite common for this implementation to leave 1 fame unfinished
						// when second changes, but we don't want buffer to starve.
						if(PsndOut && frames_done < target_fps && frames_done > target_fps-5) {
							SkipFrame(); frames_done++;
						}

						frames_done  -= target_fps; if (frames_done  < 0) frames_done  = 0;
						frames_shown -= target_fps; if (frames_shown < 0) frames_shown = 0;
						if (frames_shown > frames_done) frames_shown = frames_done;
					}
				}


				lim_time = (frames_done+1) * target_frametime;
				if (currentConfig.Frameskip >= 0) // frameskip enabled
				{
					for (i = 0; i < currentConfig.Frameskip && gamestate == PGS_Running; i++)
					{
						CGameWindow::DoKeys();
						SkipFrame(); frames_done++;
						if (PsndOut) { // do framelimitting if sound is enabled
							gettimeofday(&tval, 0);
							if(thissec != tval.tv_sec) tval.tv_usec+=1000000;
							if(tval.tv_usec < lim_time) { // we are too fast
								simpleWait(thissec, lim_time);
							}
						}
						lim_time += target_frametime;
					}
				}
				else if(tval.tv_usec > lim_time) { // auto frameskip
					// no time left for this frame - skip
					CGameWindow::DoKeys();
					SkipFrame(); frames_done++;
					continue;
				}

				// we might have lost focus already
				if (gamestate != PGS_Running) break;

				CGameWindow::DoKeys();
				PicoFrame();

				// check time
				gettimeofday(&tval, 0);
				if(thissec != tval.tv_sec) tval.tv_usec+=1000000;

				// sleep if we are still too fast
				if(PsndOut != 0 || currentConfig.Frameskip < 0)
				{
					// TODO: check if User::After() is accurate
					gettimeofday(&tval, 0);
					if(thissec != tval.tv_sec) tval.tv_usec+=1000000;
					if(tval.tv_usec < lim_time)
					{
						// we are too fast
						simpleWait(thissec, lim_time);
					}
				}

				CPolledActiveScheduler::Instance()->Schedule();

				if (gamestate != PGS_Paused)
					vidDrawFrame(notice, buff, frames_shown);

				frames_done++; frames_shown++;
			} // while

			if (PicoAHW & PAHW_MCD) PicoCDBufferFree();

			// save SRAM
			if ((currentConfig.EmuOpt & EOPT_USE_SRAM) && SRam.changed) {
				emu_SaveLoadGame(0, 1);
				SRam.changed = 0;
			}
			CPolledActiveScheduler::Instance()->Schedule();
			CGameWindow::FreeResources();
		}
		else if(gamestate == PGS_ReloadRom)
		{
			loadrom_result = emu_ReloadRom(loadrom_fname);
			pico_was_reset = 1;
			if (loadrom_result)
				gamestate = PGS_Running;
			else
				gamestate = PGS_Paused;
			DEBUGPRINT(_L("done loading ROM, retval=%i"), loadrom_result);
			loadWaitSemaphore.Signal();
			User::After(50000);
		}
		else if(gamestate == PGS_Paused) {
			DEBUGPRINT(_L("pausing.."));
			pauseSemaphore.Wait();
		}
		else if(gamestate == PGS_KeyConfig)
		{
			// switch context to other thread
			User::After(50000);
			// prepare window and stuff
			CGameWindow::ConstructResourcesL();

			TUint whichAction = 0;
			while(gamestate == PGS_KeyConfig) {
				CGameWindow::DoKeysConfig(whichAction);
				CPolledActiveScheduler::Instance()->Schedule();
				if (gamestate != PGS_Paused)
					vidKeyConfigFrame(whichAction);
				User::After(150000);
			}

			CGameWindow::FreeResources();
		} else if(gamestate == PGS_DebugHeap) {
			#ifdef __DEBUG_PRINT
			TInt cells = User::CountAllocCells();
			TInt mem;
			User::AllocSize(mem);
			DEBUGPRINT(_L("worker: cels=%d, size=%d KB"), cells, mem/1024);
			gamestate = gamestate_next;
			#endif
		} else if(gamestate == PGS_Quit) {
			break;
		}
	}

	// this thread has to close it's own handles,
	// other one will crash trying to do that
	PicoExit();

	MainExit();
}


// main initialization
static void MainInit()
{
	DEBUGPRINT(_L("\r\n\r\nstarting.."));

	DEBUGPRINT(_L("CPolledActiveScheduler::NewL()"));
	CPolledActiveScheduler::NewL(); // create Polled AS for the sound engine

//	HAL::Get(HALData::EMachineUid, machineUid); // find out the machine UID

	DumpMemInfo();

	// try to start pico
	DEBUGPRINT(_L("PicoInit()"));
	PicoDrawSetColorFormat(2);
	PicoWriteSound = updateSound;

//	if (pauseSemaphore.Handle() <= 0)
//		pauseSemaphore.CreateLocal(0);
	DEBUGPRINT(_L("initSemaphore.Signal()"));
	initSemaphore.Signal();
}


// does not return
static void MainExit()
{
	RThread thisThread;

	DEBUGPRINT(_L("%i: cleaning up.."), (TInt32) thisThread.Id());

//	pauseSemaphore.Close();

	if(gameAudio) delete gameAudio;

	// Polled AS
	delete CPolledActiveScheduler::Instance();
}

static void DumpMemInfo()
{
	TInt	ramSize, ramSizeFree, romSize;
	
	HAL::Get(HALData::EMemoryRAM, ramSize);
	HAL::Get(HALData::EMemoryRAMFree, ramSizeFree);
	HAL::Get(HALData::EMemoryROM, romSize);

	DEBUGPRINT(_L("ram=%dKB, ram_free=%dKB, rom=%dKB"), ramSize/1024, ramSizeFree/1024, romSize/1024);
}


extern "C" TInt my_SetExceptionHandler(TInt, TExceptionHandler, TUint32);

TInt EmuThreadFunction(TAny*)
{
	TInt ret;
	const TUint32 exs = KExceptionAbort|KExceptionKill|KExceptionUserInterrupt|KExceptionFpe|KExceptionFault|KExceptionInteger|KExceptionDebug;
	
	DEBUGPRINT(_L("EmuThreadFunction(), def ExceptionHandler %08x, my %08x"),
		User::ExceptionHandler(), ExceptionHandler);
	User::SetJustInTime(1);
	ret = User::SetExceptionHandler(ExceptionHandler, exs/*(TUint32) -1*/); // does not work :(
	// my_SetExceptionHandler(KCurrentThreadHandle, ExceptionHandler, 0xffffffff);
	DEBUGPRINT(_L("SetExceptionHandler %i, %08x"), ret, User::ExceptionHandler());
	User::ModifyExceptionMask(0, exs);

	//TInt pc, sp;
	//asm volatile ("str pc, %0" : "=m" (pc) );
	//asm volatile ("str sp, %0" : "=m" (sp) );
	//RDebug::Print(_L("executing @ 0x%08x, sp=0x%08x"), pc, sp);

/*
	RDebug::Print(_L("Base     Bottom   Top      Size     RW Name"));
	TBuf<4> l_r(_L("R")), l_w(_L("W")), l_d(_L("-"));
	RChunk chunk;
	TFullName chunkname;
	TFindChunk findChunk(_L("*"));
	while( findChunk.Next(chunkname) != KErrNotFound ) {
		chunk.Open(findChunk);
		RDebug::Print(_L("%08x %08x %08x %08x %S%S %S"), chunk.Base(), chunk.Base()+chunk.Bottom(), chunk.Base()+chunk.Top(), chunk.Size(), chunk.IsReadable() ? &l_r : &l_d, chunk.IsWritable() ? &l_w : &l_d, &chunkname);
		chunk.Close();
	}
*/

	// can't do that, will crash here
//	if(cleanup) {
//		DEBUGPRINT(_L("found old CTrapCleanup, deleting.."));
//		delete cleanup;
//	}
	
	CTrapCleanup *cleanup = CTrapCleanup::New();

	TRAPD(error, TargetEpocGameL());

	__ASSERT_ALWAYS(!error, User::Panic(_L("PicoDrive"), error));
	delete cleanup;

	DEBUGPRINT(_L("exitting.."));	
	return 1;
}


void TPicoDirectScreenAccess::Restart(RDirectScreenAccess::TTerminationReasons aReason)
{
	DEBUGPRINT(_L("TPicoDirectScreenAccess::Restart(%i)"), aReason);

//	if (CGameWindow::iDSA) {
//		TRAPD(error, CGameWindow::iDSA->StartL());
//		if (error) DEBUGPRINT(_L("iDSA->StartL() error: %i"), error);
//	}
}


void TPicoDirectScreenAccess::AbortNow(RDirectScreenAccess::TTerminationReasons aReason)
{
	DEBUGPRINT(_L("TPicoDirectScreenAccess::AbortNow(%i)"), aReason);

	// the WS wants us to stop, so let's obey
	gamestate = PGS_Paused;
}


void CGameWindow::ConstructResourcesL()
{
	DEBUGPRINT(_L("ConstructResourcesL()"));
	// connect to window server
	// tried to create it globally and not re-connect everytime,
	// but my window started to lose focus strangely
	iWsSession = new(ELeave) RWsSession();
	User::LeaveIfError(iWsSession->Connect());

	//	 * Tell the Window Server not to mess about with our process priority
	//	 * Also, because of the way legacy games are written, they never sleep
	//	 * and thus never voluntarily yield the CPU. We set our process priority
	//	 * to EPriorityForeground and hope that a Telephony application on
	//	 * this device runs at EPriorityForeground as well. If not, tough! ;-)

	iWsSession->ComputeMode(RWsSession::EPriorityControlDisabled);
	RProcess me;
	me.SetPriority(EPriorityForeground);

	iWsScreen = new(ELeave) CWsScreenDevice(*iWsSession);
	User::LeaveIfError(iWsScreen->Construct());
//	User::LeaveIfError(iWsScreen->CreateContext(iWindowGc));

	iWsWindowGroup = RWindowGroup(*iWsSession);
	User::LeaveIfError(iWsWindowGroup.Construct((TUint32)&iWsWindowGroup));
	//iWsWindowGroup.SetOrdinalPosition(0);
	//iWsWindowGroup.SetName(KServerWGName);
	iWsWindowGroup.EnableScreenChangeEvents(); // flip events (EEventScreenDeviceChanged)
	iWsWindowGroup.EnableFocusChangeEvents(); // EEventFocusGroupChanged
	iWsWindowGroup.SetOrdinalPosition(0, 1); // TInt aPos, TInt aOrdinalPriority

	iWsWindow=RWindow(*iWsSession);
	User::LeaveIfError(iWsWindow.Construct(iWsWindowGroup, (TUint32)&iWsWindow));
	iWsWindow.SetSize(iWsScreen->SizeInPixels());
	iWsWindow.PointerFilter(EPointerFilterDrag, 0);
	iWsWindow.SetPointerGrab(ETrue);
	iWsWindow.SetVisible(ETrue);
	iWsWindow.Activate();

#if 0
	// request access through RDirectScreenAccess api, but don't care about the result
	// hangs?
	RRegion *dsa_region = 0;
	iDSA = new(ELeave) RDirectScreenAccess(*iWsSession);
	if(iDSA->Construct() == KErrNone)
		iDSA->Request(dsa_region, iDSAstatus, iWsWindow);
	DEBUGPRINT(_L("DSA: %i"), dsa_region ? dsa_region->Count() : -1);
#endif

	TInt ret;

	// request access through CDirectScreenAccess
	iDSA = CDirectScreenAccess::NewL(*iWsSession, *iWsScreen, iWsWindow, iPDSA);

	// now get the screenbuffer
	TScreenInfoV01			screenInfo;
	TPckg<TScreenInfoV01>	sI(screenInfo);
	UserSvr::ScreenInfo(sI);

	if(!screenInfo.iScreenAddressValid)
		User::Leave(KErrNotSupported);

	DEBUGPRINT(_L("framebuffer=0x%08x (%dx%d)"), screenInfo.iScreenAddress,
					screenInfo.iScreenSize.iWidth, screenInfo.iScreenSize.iHeight);
	
	// vidInit
	DEBUGPRINT(_L("vidInit()"));
	ret = vidInit((void *)screenInfo.iScreenAddress, 0);
	DEBUGPRINT(_L("vidInit() done (%i)"), ret);

	User::LeaveIfError(ret);

	memset(keyFlags, 0, 256);
	keyFlags[EStdKeyM600JogUp] = keyFlags[EStdKeyM600JogDown] = 2; // add "pulse only" for jog up/down
	keyFlags[EStdKeyOff] = 0x40; // not configurable

	// try to start the audio engine
	static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;

	if (gamestate == PGS_Running && (currentConfig.EmuOpt & EOPT_EN_SOUND))
	{
		TInt err = 0;
		if(PsndRate != PsndRate_old || (PicoOpt&11) != (PicoOpt_old&11) || Pico.m.pal != pal_old) {
			// if rate changed, reset all enabled chips, else reset only those chips, which were recently enabled
			//sound_reset(PsndRate != PsndRate_old ? PicoOpt : (PicoOpt&(PicoOpt^PicoOpt_old)));
			PsndRerate(1);
		}
		if(!gameAudio || PsndRate != PsndRate_old || ((PicoOpt&8) ^ (PicoOpt_old&8)) || Pico.m.pal != pal_old) { // rate or stereo or pal/ntsc changed
			if(gameAudio) delete gameAudio; gameAudio = 0;
			DEBUGPRINT(_L("starting audio: %i len: %i stereo: %i, pal: %i"), PsndRate, PsndLen, PicoOpt&8, Pico.m.pal);
			TRAP(err, gameAudio = CGameAudioMS::NewL(PsndRate, (PicoOpt&8) ? 1 : 0,
						Pico.m.pal ? 50 : 60, currentConfig.volume));
		}
		if( gameAudio) {
			TRAP(err, PsndOut = gameAudio->ResumeL());
		}
		if(err) {
			if(gameAudio) delete gameAudio;
			gameAudio = 0;
			PsndOut = 0;
			strcpy(noticeMsg, "SOUND@STARTUP@FAILED");
			gettimeofday(&noticeMsgTime, 0);
		}
		PsndRate_old = PsndRate;
		PicoOpt_old  = PicoOpt;
		pal_old = Pico.m.pal;
	} else {
		if(gameAudio) delete gameAudio;
		gameAudio = 0;
		PsndOut = 0;
	}

	CPolledActiveScheduler::Instance()->Schedule();

	// start key WS event polling
	iWsSession->EventReady(&iWsEventStatus);

	iWsSession->Flush(); // check: short hang in UIQ2
	User::After(1);

	// I don't know why but the Window server sometimes hangs completely (hanging the phone too) after calling StartL()
	// Is this a sync broblem? weird bug?
	TRAP(ret, iDSA->StartL());
	if (ret) DEBUGPRINT(_L("iDSA->StartL() error: %i"), ret);

//	User::After(1);
//	CPolledActiveScheduler::Instance()->Schedule();

	DEBUGPRINT(_L("CGameWindow::ConstructResourcesL() finished."));
}

// this may be run even if there is nothing to free
void CGameWindow::FreeResources()
{
	if(gameAudio) gameAudio->Pause();

	//DEBUGPRINT(_L("CPolledActiveScheduler::Instance(): %08x"), CPolledActiveScheduler::Instance());
	if(CPolledActiveScheduler::Instance())
		CPolledActiveScheduler::Instance()->Schedule();

#if 0
	// free RDirectScreenAccess stuff (seems to be deleted automatically after crash?)
	if(iDSA) {
		iDSA->Cancel();
		iDSA->Close();
		delete iDSA;
	}
	iDSA = NULL;
#endif
	if(iDSA) delete iDSA;
	iDSA = 0;

	if(iWsSession->WsHandle() > 0 && iWsEventStatus != KRequestPending) // TODO: 2 UIQ2 (?)
		iWsSession->EventReadyCancel();

	if(iWsWindow.WsHandle() > 0)
		iWsWindow.Close();

	if(iWsWindowGroup.WsHandle() > 0)
		iWsWindowGroup.Close();

	// these must be deleted before calling iWsSession->Close()
	if(iWsScreen) {
		delete iWsScreen;
		iWsScreen = NULL;
	}

	if(iWsSession->WsHandle() > 0) {
		iWsSession->Close();
		delete iWsSession;
	}
	
	vidFree();
}


void CGameWindow::DoKeys(void)
{
	TWsEvent iWsEvent;
	TInt iWsEventType;
	unsigned long allActions = 0;
	static unsigned long areaActions = 0, forceUpdate = 0;
	int i, nEvents;

	for(nEvents = 0; iWsEventStatus != KRequestPending; nEvents++)
	{
		iWsSession->GetEvent(iWsEvent);
		iWsEventType = iWsEvent.Type();

		// pointer events?
		if(iWsEventType == EEventPointer) {
			if(iWsEvent.Pointer()->iType == TPointerEvent::EButton1Up) {
				areaActions = 0; // remove all directionals
			} else { // if(iWsEvent.Pointer()->iType == TPointerEvent::EButton1Down) {
				TPoint p = iWsEvent.Pointer()->iPosition;
				const TPicoAreaConfigEntry *e = areaConfig + 1;
				for(i = 0; !e->rect.IsEmpty(); e++, i++)
					if(e->rect.Contains(p)) {
						areaActions = currentConfig.KeyBinds[i+256];
						break;
					}
				//DEBUGPRINT(_L("pointer event: %i %i"), p.iX, p.iY);
			}
		}
		else if(iWsEventType == EEventKeyDown || iWsEventType == EEventKeyUp) {
			TInt iScanCode = iWsEvent.Key()->iScanCode;
			//DEBUGPRINT(_L("key event: 0x%02x"), iScanCode);

			if(iScanCode < 256)
			{
				if(iWsEventType == EEventKeyDown) {
					keyFlags[iScanCode] |=  1;
					for(i=0; i < 10; i++) {
						if( pressedKeys[i] == (TUint8) iScanCode) break;
						if(!pressedKeys[i]) { pressedKeys[i] = (TUint8) iScanCode; break; }
					}
				} else if(!(keyFlags[iScanCode]&2)) {
					keyFlags[iScanCode] &= ~1;
					for(i=0; i < 10; i++) {
						if(pressedKeys[i] == (TUint8) iScanCode) { pressedKeys[i] = 0; break; }
					}
				}

				// power?
				if(iScanCode == EStdKeyOff) gamestate = PGS_Paused;
			} else {
				DEBUGPRINT(_L("weird scancode: 0x%02x"), iScanCode);
			}
		}
		else if(iWsEventType == EEventScreenDeviceChanged) {
			// ???
			//User::After(500000);
			//reset_timing = 1;
			DEBUGPRINT(_L("EEventScreenDeviceChanged, focus: %i, our: %i"),
						iWsSession->GetFocusWindowGroup(), iWsWindowGroup.Identifier());
		}
		else if(iWsEventType == EEventFocusGroupChanged) {
			TInt focusGrpId = iWsSession->GetFocusWindowGroup();
			DEBUGPRINT(_L("EEventFocusGroupChanged: %i, our: %i"),
						focusGrpId, iWsWindowGroup.Identifier());
			// if it is not us and not launcher that got focus, pause emu
			if(focusGrpId != iWsWindowGroup.Identifier())
				gamestate = PGS_Paused;
		}

		iWsEventStatus = KRequestPending;
		iWsSession->EventReady(&iWsEventStatus);
	}

	if(nEvents || forceUpdate) {
		allActions = areaActions;
		forceUpdate = 0;

		// add all pushed button actions
		for(i = 9; i >= 0; i--) {
			int scan = pressedKeys[i];
			if(scan) {
				if(keyFlags[scan] & 1) allActions |= currentConfig.KeyBinds[scan];
				if((keyFlags[scan]& 3)==3) forceUpdate = 1;
				if(keyFlags[scan] & 2) keyFlags[scan] &= ~1;
			}
		}

		PicoPad[0] = (unsigned short) allActions;
		if(allActions & 0xFFFF0000) {
			RunEvents(allActions >> 16);
			areaActions = 0;
		}
	}
}


void CGameWindow::DoKeysConfig(TUint &which)
{
	TWsEvent iWsEvent;
	int i;

	while(iWsEventStatus != KRequestPending)
	{
		TUint currentActCode = 1 << which;

		iWsSession->GetEvent(iWsEvent);

		// pointer events?
		if(iWsEvent.Type() == EEventPointer) {
			TPoint p = iWsEvent.Pointer()->iPosition;
			TRect prev(56,  0, 120, 26);
			TRect next(120, 0, 180, 26);

			if(iWsEvent.Pointer()->iType == TPointerEvent::EButton1Down) {
				     if(prev.Contains(p)) do { which = (which-1) & 0x1F; } while(!actionNames[which]);
				else if(next.Contains(p)) do { which = (which+1) & 0x1F; } while(!actionNames[which]);
				else if(which == 31) gamestate = PGS_Paused; // done
				else {
					const TPicoAreaConfigEntry *e = areaConfig + 1;
					for(i = 0; e->rect != TRect(0,0,0,0); e++, i++)
						if(e->rect.Contains(p)) {
							currentConfig.KeyBinds[i+256] ^= currentActCode;
							break;
						}
				}
			}
		}
		else if(iWsEvent.Type() == EEventKeyDown || iWsEvent.Type() == EEventKeyUp)
		{
			TUint scan = (TUint) iWsEvent.Key()->iScanCode;

			// key events?
			if(iWsEvent.Type() == EEventKeyDown) {
				if(which == 31) {
					gamestate = PGS_Paused;
				} else if (scan < 256) {
					if(!(keyFlags[scan]&0x40)) currentConfig.KeyBinds[scan] ^= currentActCode;
				}
			}

			// power?
			if(iWsEvent.Key()->iScanCode == EStdKeyOff) gamestate = PGS_Paused;
		}
		else if(iWsEvent.Type() == EEventFocusGroupChanged) {
			TInt focusGrpId = iWsSession->GetFocusWindowGroup();
			// if we lost focus, exit config mode
			if(focusGrpId != iWsWindowGroup.Identifier())
				gamestate = PGS_Paused;
		}

//		iWsEventStatus = KRequestPending;
		iWsSession->EventReady(&iWsEventStatus);
	}
}


void CGameWindow::RunEvents(TUint32 which)
{
	if (which & 0x4000) currentConfig.Frameskip = -1;
	if (which & 0x2000) currentConfig.Frameskip =  8;
	if (which & 0x1800) { // save or load (but not both)
		if(PsndOut) gameAudio->Pause(); // this may take a while, so we pause sound output

		vidDrawNotice((which & 0x1000) ? "LOADING@GAME" : "SAVING@GAME");
		emu_SaveLoadGame(which & 0x1000, 0);

		if(PsndOut) PsndOut = gameAudio->ResumeL();
		reset_timing = 1;
	}
	if (which & 0x0400) gamestate = PGS_Paused;
	if (which & 0x0200) { // switch renderer
		if (!(currentConfig.scaling == TPicoConfig::PMFit &&
			(currentConfig.rotation == TPicoConfig::PRot0 || currentConfig.rotation == TPicoConfig::PRot180)))
		{
			PicoOpt^=0x10;
			vidInit(0, 1);

			strcpy(noticeMsg, (PicoOpt&0x10) ? "ALT@RENDERER" : "DEFAULT@RENDERER");
			gettimeofday(&noticeMsgTime, 0);
		}
	}
	if(which & 0x00c0) {
		if(which&0x0080) {
			state_slot -= 1;
			if(state_slot < 0) state_slot = 9;
		} else {
			state_slot += 1;
			if(state_slot > 9) state_slot = 0;
		}
		sprintf(noticeMsg, "SAVE@SLOT@%i@SELECTED", state_slot);
		gettimeofday(&noticeMsgTime, 0);
	}
	if(which & 0x0020) if(gameAudio) currentConfig.volume = gameAudio->ChangeVolume(0);
	if(which & 0x0010) if(gameAudio) currentConfig.volume = gameAudio->ChangeVolume(1);
}


extern "C" void emu_noticeMsgUpdated(void)
{
	char *p = noticeMsg;
	while (*p) {
		if (*p == ' ') *p = '@';
		if (*p < '0' || *p > 'Z') { *p = 0; break; }
		p++;
	}
	gettimeofday(&noticeMsgTime, 0);
}

// static class members
RWsSession*				CGameWindow::iWsSession;
RWindowGroup			CGameWindow::iWsWindowGroup;
RWindow					CGameWindow::iWsWindow;
CWsScreenDevice*		CGameWindow::iWsScreen = NULL;
CWindowGc*				CGameWindow::iWindowGc = NULL;
TRequestStatus			CGameWindow::iWsEventStatus = KRequestPending;
//RDirectScreenAccess*	CGameWindow::iDSA;
//TRequestStatus			CGameWindow::iDSAstatus = KRequestPending;
TPicoDirectScreenAccess	CGameWindow::iPDSA;
CDirectScreenAccess*	CGameWindow::iDSA = NULL;

