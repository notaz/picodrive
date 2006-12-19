// mainloop with window server event handling
// event polling mechnism was taken from
// Peter van Sebille's projects

// (c) Copyright 2006, notaz
// All Rights Reserved

#include <hal.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "debug.h"

#include "pico/picoInt.h"
#include "vid.h"
#include "SimpleServer.h"
#include "ClientServer.h"
//#include "polledAS.h"
#include "audio.h"

#include <EZlib.h>
#include "zlib/gzio_symb.h"


#define BENCHMARK
//#define TEST_FRAMEBUFF

// keycodes we care about
enum TPxxxKeyCodes {
    EKeyPxxxPower    = EKeyDevice0,		 //0xF842
    EKeyPxxxBrowser  = EKeyApplication0,
    EKeyPxxxCamera   = EKeyApplication1,
    EKeyPxxxJogUp    = EKeyDevice1,
    EKeyPxxxJogDown  = EKeyDevice2,
    EKeyPxxxJogLeft  = EKeyDevice3,
    EKeyPxxxJogRight = EKeyDevice9,
    EKeyPxxxJogInward= EKeyDevice8,
	// FC keys
    //EKeyPxxxFcOk     = EKeyDevice8, // don't care about FC keycodes
};
// EKeyScreenDimension1 ~ EStdKeyF24 is sent when flip is closed,
// EKeyScreenDimension0 ~ EStdKeyF23 when opened

enum TMotAKeyCodes {
    EKeyMotAUp       = EKeyDevice4,		 //0xF846
    EKeyMotADown     = EKeyDevice5,
    EKeyMotALeft     = EKeyDevice6,
    EKeyMotARight    = EKeyDevice7,
    EKeyMotASelect   = EKeyDevice8,
    EKeyMotAButton1  = EKeyApplicationA,
    EKeyMotAButton2  = EKeyApplicationB,
    EKeyMotAHome     = EKeyApplication0,
    EKeyMotAShortcut = EKeyApplication1,
    EKeyMotAVoice    = EKeyDeviceA,
    EKeyMotACamera   = EKeyDeviceB,
    EKeyMotAVolUp    = EKeyIncVolume,
    EKeyMotAVolDn    = EKeyDecVolume,
    EKeyMotASend     = EKeyYes,
    EKeyMotAEnd      = EKeyNo,
};

// scancodes we care about
enum TPxxxScanCodes {
    EScanPxxxPower    = EStdKeyDevice0,		 // 0xa4
    EScanPxxxBrowser  = EStdKeyApplication0,
    EScanPxxxCamera   = EStdKeyApplication1,
    EScanPxxxJogUp    = EStdKeyDevice1,
    EScanPxxxJogDown  = EStdKeyDevice2,
    EScanPxxxJogLeft  = EStdKeyDeviceE, // not consistent
    EScanPxxxJogRight = EStdKeyDeviceD,
    EScanPxxxJogInward= EStdKeyDevice8,
	// FC keys
	EScanPxxxFcOk      = EStdKeyDeviceF,
	EScanPxxxFcBack    = EStdKeyDevice3,
	EScanPxxxFcC       = EStdKeyDeviceA,
	EScanPxxxFcMenu    = EStdKeyDevice9,
	EScanPxxxFc0       = '0',
	EScanPxxxFc1       = '1',
	EScanPxxxFc2       = '2',
	EScanPxxxFc3       = '3',
	EScanPxxxFc4       = '4',
	EScanPxxxFc5       = '5',
	EScanPxxxFc6       = '6',
	EScanPxxxFc7       = '7',
	EScanPxxxFc8       = '8',
	EScanPxxxFc9       = '9',
	EScanPxxxFcHash    = EStdKeyHash,
	EScanPxxxFcAsterisk= EStdKeyNkpAsterisk,
};

enum TMotAScanCodes {
    EScanMotAUp       = EStdKeyDevice4,
    EScanMotADown     = EStdKeyDevice5,
    EScanMotALeft     = EStdKeyDevice6,
    EScanMotARight    = EStdKeyDevice7,
    EScanMotASelect   = EStdKeyDevice8,
    EScanMotAButton1  = EStdKeyApplicationA,
    EScanMotAButton2  = EStdKeyApplicationB,
    EScanMotAHome     = EStdKeyApplication0,
    EScanMotAShortcut = EStdKeyApplication1,
    EScanMotAVoice    = EStdKeyDeviceA,
    EScanMotACamera   = EStdKeyDeviceB,
    EScanMotAVolUp    = EStdKeyIncVolume,
    EScanMotAVolDn    = EStdKeyDecVolume,
    EScanMotASend     = EStdKeyYes,
    EScanMotAEnd      = EStdKeyNo,
	// some extra codes, don't know if these are actually used
	EScanMotAExtra    = EStdKeyApplicationC,
	EScanMotAEsc      = EStdKeyApplicationD,
	EScanMotAStart    = EStdKeyApplicationE,
	EScanMotASelect2  = EStdKeyApplicationF,
};


// list of key names and codes
TPicoKeyConfigEntry keyConfigPXXX[] = {
	{ EKeyPxxxPower,     EScanPxxxPower,     0, -1, -1, "POWER" },     // 0
	{ EKeyPxxxBrowser,   EScanPxxxBrowser,   0, -1, -1, "BROWSER" },
	{ EKeyPxxxCamera,    EScanPxxxCamera,    0, -1, -1, "CAMERA" },
	{ EKeyPxxxJogUp,     EScanPxxxJogUp,     2, -1, -1, "JOG@UP" },
	{ EKeyPxxxJogDown,   EScanPxxxJogDown,   2, -1, -1, "JOG@DOWN" },
	{ EKeyPxxxJogLeft,	 EScanPxxxJogLeft,   0, -1, -1, "JOG@LEFT" },  // 5
	{ EKeyPxxxJogRight,  EScanPxxxJogRight,  0, -1, -1, "JOG@RIGHT" },
	{ 0,                 EScanPxxxJogInward, 0, -1, -1, "JOG@INWARD" },
	{ 0,                 EScanPxxxFcOk,      0, -1, -1, "FC@OK" },
	{ 0,                 EScanPxxxFcBack,    0, -1, -1, "FC@BACK" },
	{ 0,                 EScanPxxxFcC,       0, -1, -1, "FC@C" },      // 10
	{ 0,                 EScanPxxxFcMenu,    0, -1, -1, "FC@MENU" },
	{ 0,                 EScanPxxxFc0,       0, -1, -1, "FC@0" },
	{ 0,                 EScanPxxxFc1,       0, -1, -1, "FC@1" },
	{ 0,                 EScanPxxxFc2,       0, -1, -1, "FC@2" },
	{ 0,                 EScanPxxxFc3,       0, -1, -1, "FC@3" },
	{ 0,                 EScanPxxxFc4,       0, -1, -1, "FC@4" },
	{ 0,                 EScanPxxxFc5,       0, -1, -1, "FC@5" },
	{ 0,                 EScanPxxxFc6,       0, -1, -1, "FC@6" },
	{ 0,                 EScanPxxxFc7,       0, -1, -1, "FC@7" },
	{ 0,                 EScanPxxxFc8,       0, -1, -1, "FC@8" },
	{ 0,                 EScanPxxxFc9,       0, -1, -1, "FC@9" },
	{ 0,                 EScanPxxxFcHash,    0, -1, -1, "FC@HASH" },
	{ 0,                 EScanPxxxFcAsterisk,0, -1, -1, "FC@AST" },
	{ 0,                 0,                  0,  0,  0, 0 }
};

// Motorola A92x & A1000 support
TPicoKeyConfigEntry keyConfigMotA[] = {
	{ EKeyMotAUp,        EScanMotAUp,        0, -1, -1, "UP" },      // 0
	{ EKeyMotADown,      EScanMotADown,      0, -1, -1, "DOWN" },
	{ EKeyMotALeft,      EScanMotALeft,      0, -1, -1, "LEFT" },
	{ EKeyMotARight,     EScanMotARight,     0, -1, -1, "RIGHT" },
	{ EKeyMotASelect,    EScanMotASelect,    0, -1, -1, "SELECT" },
	{ EKeyMotAButton1,   EScanMotAButton1,   0, -1, -1, "BUTTON1" }, // 5
	{ EKeyMotAButton2,   EScanMotAButton2,   0, -1, -1, "BUTTON2" },
	{ EKeyMotAHome,      EScanMotAHome,      0, -1, -1, "HOME" },
	{ EKeyMotAShortcut,  EScanMotAShortcut,  0, -1, -1, "SHORTCUT" },
	{ EKeyMotAVoice,     EScanMotAVoice,     0, -1, -1, "VOICE" },
	{ EKeyMotACamera,    EScanMotACamera,    0, -1, -1, "CAMERA" },  // 10
	{ EKeyMotAVolUp,     EScanMotAVolUp,     0, -1, -1, "VOL@UP" },
	{ EKeyMotAVolDn,     EScanMotAVolDn,     0, -1, -1, "VOL@DOWN" },
	{ EKeyMotASend,      EScanMotASend,      0, -1, -1, "SEND" },
	{ EKeyMotAEnd,       EScanMotAEnd,       0, -1, -1, "END" },
	{ 0,                 EScanMotAExtra,     0, -1, -1, "EXTRA" },
	{ 0,                 EScanMotAEsc,       0, -1, -1, "ESC" },
	{ 0,                 EScanMotAStart,     0, -1, -1, "START" },
	{ 0,                 EScanMotASelect2,   0, -1, -1, "SELECT" },
	{ 0,                 0,                  0,  0,  0, 0 }
};


// list of areas
TPicoAreaConfigEntry areaConfig[] = {
	{ TRect(  0,   0,   0,   0) },
	// small corner bottons
	{ TRect(  0,   0,  15,  15) },
	{ TRect(192,   0, 207,  15) },
	{ TRect(  0, 304,  15, 319) },
	{ TRect(192, 304, 207, 319) },
	// normal buttons
	{ TRect(  0,   0,  68,  63) },
	{ TRect( 69,   0, 138,  63) },
	{ TRect(139,   0, 207,  63) },
	{ TRect(  0,  64,  68, 127) },
	{ TRect( 69,  64, 138, 127) },
	{ TRect(139,  64, 207, 127) },
	{ TRect(  0, 128,  68, 191) },
	{ TRect( 69, 128, 138, 191) },
	{ TRect(139, 128, 207, 191) },
	{ TRect(  0, 192,  68, 255) },
	{ TRect( 69, 192, 138, 255) },
	{ TRect(139, 192, 207, 255) },
	{ TRect(  0, 256,  68, 319) },
	{ TRect( 69, 256, 138, 319) },
	{ TRect(139, 256, 207, 319) },
	{ TRect(  0,   0,   0,   0) }
};

// PicoPad[] format: SACB RLDU
const char *actionNames[] = {
	"UP", "DOWN", "LEFT", "RIGHT", "B", "C", "A", "START",
	0, 0, 0, 0, 0, 0, 0, 0, // Z, Y, X, MODE (enabled only when needed), ?, ?, ?, ?
	0, 0, 0, 0, 0, 0, "NEXT@SAVE@SLOT", "PREV@SAVE@SLOT", // ?, ?, ?, ?, mot_vol_up, mot_vol_down, next_slot, prev_slot
	0, 0, "PAUSE@EMU", "SAVE@STATE", "LOAD@STATE", "FRAMESKIP@8", "AUTO@FRAMESKIP", "DONE" // ?, switch_renderer
};


// globals are allowed, so why not to (ab)use them?
TInt machineUid = 0;
int gamestate = PGS_Paused, gamestate_prev = PGS_Paused;
TPicoConfig currentConfig;
TPicoKeyConfigEntry *keyConfig = 0;			// currently used keys
static char noticeMsg[64];					// notice msg to draw
static timeval noticeMsgTime = { 0, 0 };	// when started showing
static RLibrary gameAudioLib;				// audio object library
static _gameAudioNew gameAudioNew;			// audio object maker
static IGameAudio *gameAudio = 0;			// the audio object itself
static TProcessId launcherProcessId;
static int reset_timing, state_slot = 0;
extern const char *RomFileName;
extern int pico_was_reset;	
#ifdef TEST_FRAMEBUFF
static TUint8 *iFrameBuffer = 0;
#endif

// some forward declarations
static void MainInit();
static void MainExit();
static void CheckForLauncher();
static void DumpMemInfo();

// just for a nicer grouping of WS related stuff
class CGameWindow
{
public:
	static void ConstructResourcesL();
	static void FreeResources();
	static void DoKeys(timeval &time);
	static void DoKeysConfig(TUint &which);
	static void RunEvents(TUint32 which);
	static void SendClientWsEvent(TInt type);

	static RWsSession				iWsSession;
	static RWindowGroup				iWsWindowGroup;
	static RWindow					iWsWindow;
	static CWsScreenDevice*			iWsScreen;
	static CWindowGc*				iWindowGc;
	static TRequestStatus			iWsEventStatus;
	static TThreadId				iLauncherThreadId;
	static RDirectScreenAccess*		iDSA;
	static TRequestStatus			iDSAstatus;
};


void SkipFrame(int do_sound)
{
  PicoSkipFrame=1;
  PicoFrame();
  PicoSkipFrame=0;

  if(do_sound && PsndOut) {
    PsndOut = gameAudio->NextFrameL();
	if(!PsndOut) { // sound output problems?
      strcpy(noticeMsg, "SOUND@OUTPUT@ERROR;@SOUND@DISABLED");
      gettimeofday(&noticeMsgTime, 0);
	}
  }

/*
  int total=0;

  // V-Blanking period:
  if (Pico.video.reg[1]&0x20) SekInterrupt(6); // Set IRQ
  Pico.video.status|=0x88; // V-Int happened / go into vblank
  total+=SekRun(18560);

  // Active Scan:
  if (Pico.video.reg[1]&0x40) Pico.video.status&=~8; // Come out of vblank if display is enabled
  SekInterrupt(0); // Clear IRQ
  total+=SekRun(127969-total);
*/
}


void TargetEpocGameL()
{
	char buff[24]; // fps count c string
	struct timeval tval; // timing
	int thissec = 0, frames_done = 0, frames_shown = 0;
	int target_fps, target_frametime, too_fast, too_fast_time;
	int i, underflow;
	TRawEvent blevent;

	MainInit();
	buff[0] = 0;

	// just to keep the backlight on..
	blevent.Set(TRawEvent::EActive);

	// loop?
	for(;;) {
		if(gamestate == PGS_Running) {
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

			pico_was_reset = too_fast = 0;
			reset_timing = 1;

			while(gamestate == PGS_Running) {
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
				if(thissec != tval.tv_sec) {
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
					if(currentConfig.iFlags & 2) 
						sprintf(buff, "%02i/%02i", frames_shown, frames_done);
#endif
					thissec = tval.tv_sec;
					if((thissec & 7) == 7) UserSvr::AddEvent(blevent);
					// in is quite common for this implementation to leave 1 fame unfinished
					// when second changes. This creates sound clicks, so it's probably better to
					// skip that frame and render sound
					if(PsndOut && frames_done < target_fps && frames_done > target_fps-5) {
						SkipFrame(1); frames_done++;
					}
					// try to prevent sound buffer underflows by making sure we did _exactly_
					// target_fps sound updates and copying last samples over and over again
					if(PsndOut && frames_done < target_fps)
						for(; frames_done < target_fps; frames_done++) {
							PsndOut = gameAudio->DupeFrameL(underflow);
							if(!PsndOut) { // sound output problems?
								strcpy(noticeMsg, "SOUND@OUTPUT@ERROR;@SOUND@DISABLED");
								gettimeofday(&noticeMsgTime, 0);
								break;
							}
							if(underflow) break;
						}
					frames_done = frames_shown = 0;
				}

				if(currentConfig.iFrameskip >= 0) { // frameskip enabled
					for(i = 0; i < currentConfig.iFrameskip; i++) {
						SkipFrame(frames_done < target_fps); frames_done++;
						CGameWindow::DoKeys(tval);
					}
				} else if(tval.tv_usec > (frames_done+1)*target_frametime) { // auto frameskip
					// no time left for this frame - skip
					SkipFrame(1); frames_done++;
					CGameWindow::DoKeys(tval);
					too_fast = 0;
					continue;
				} else if(tval.tv_usec < (too_fast_time=frames_done*target_frametime)) { // we are too fast
					if(++too_fast > 2) { User::After(too_fast_time-tval.tv_usec); too_fast = 0; }// sleep, but only if we are _really_ fast
				}

				// draw
				vidDrawFrame(notice, buff, frames_shown);
				if(PsndOut && frames_done < target_fps) {
					PsndOut = gameAudio->NextFrameL();
					if(!PsndOut) { // sound output problems?
						strcpy(noticeMsg, "SOUND@OUTPUT@ERROR;@SOUND@DISABLED");
						gettimeofday(&noticeMsgTime, 0);
					}
				}
				frames_done++; frames_shown++;
				CGameWindow::DoKeys(tval);
			}

			// save SRAM
			if((currentConfig.iFlags & 1) && SRam.changed) {
				saveLoadGame(0, 1);
				SRam.changed = 0;
			}
			CGameWindow::SendClientWsEvent(EEventGamePaused);
			CGameWindow::FreeResources();
		} else if(gamestate == PGS_Paused) {
			for(i = 0; gamestate == PGS_Paused; i++) {
				User::After(250000);
				if(!(i & 0x7F)) CheckForLauncher(); // every 32 secs
			}
		} else if(gamestate == PGS_KeyConfig) {
			// prepare window and stuff
			CGameWindow::ConstructResourcesL();

			TUint whichAction = 0;
			while(gamestate == PGS_KeyConfig) {
				vidKeyConfigFrame(whichAction, CGameWindow::iWsScreen->CurrentScreenMode());
				CGameWindow::DoKeysConfig(whichAction);
				User::After(200000);
			}

			CGameWindow::SendClientWsEvent(EEventKeyCfgDone);
			CGameWindow::SendClientWsEvent(EEventGamePaused);
			CGameWindow::FreeResources();
		} else if(gamestate == PGS_DebugHeap) {
			#ifdef __DEBUG_PRINT
			TInt cells = User::CountAllocCells();
			TInt mem;
			User::AllocSize(mem);
			DEBUGPRINT(_L("worker: cels=%d, size=%d KB"), cells, mem/1024);
			gamestate = gamestate_prev;
			#endif
		} else if(gamestate == PGS_Quit) {
			break;
		}
	}

	MainExit();
}


// gameAudio default "maker", which simply leaves
IGameAudio *gameAudioNew_failer(TInt aRate, TBool aStereo, TInt aPcmFrames, TInt aBufferedFrames)
{
	User::Leave(1);
	return 0; // shouldn't happen
}


// main initialization
static void MainInit()
{
	RProcess thisProcess, launcherProcess;
	TInt err = KErrGeneral;

	DEBUGPRINT(_L("\r\n\r\nstarting.."));

	//CPolledActiveScheduler::NewL(); // create Polled AS for the sound engine

	// get launcher id
	if(thisProcess.Owner(launcherProcess) == KErrNone) {
		launcherProcessId = launcherProcess.Id();
		launcherProcess.Close(); // messing with launcherProcess too much strangely reboots my phone
	} else {
		DEBUGPRINT(_L("%i: couldn't find owner, terminating.."), thisProcess.Id());
		thisProcess.Terminate(1);
	}

	// also get launcher thread id (for sending events, nasty way)
	TFindThread findThread;
	TFullName dummy1;
	RThread  tmpThread;
	RProcess tmpProcess;

	while(findThread.Next(dummy1) == KErrNone)
	{
		tmpThread.Open(findThread);
		tmpThread.Process(tmpProcess);
		if(tmpProcess.Id() == launcherProcessId) {
			CGameWindow::iLauncherThreadId = tmpThread.Id();
			break;
		}
		tmpThread.Close();
		tmpProcess.Close();
	}

	// start event listening thread, which waits for GUI commands
	if(StartThread() < 0) {
		// communication thread failed to start, we serve no purpose now, so suicide
		DEBUGPRINT(_L("%i: StartThread() failed, terminating.."), thisProcess.Id());
		thisProcess.Terminate(1);
	}

	HAL::Get(HALData::EMachineUid, machineUid); // find out the machine UID

	// get current dir
	TFileName pfName = thisProcess.FileName();
	TParse parse;
	parse.Set(pfName, 0, 0);
	TPtrC currDir = parse.DriveAndPath();
	DEBUGPRINT(_L("current dir: %S"), &currDir);

	static TPtrC audio_dlls[] = { _L("audio_motorola.dll"), _L("audio_mediaserver.dll") };

	// find our audio object maker
	for(TInt i=0; i < 2; i++) {
		DEBUGPRINT(_L("trying audio DLL: %S"), &audio_dlls[i]);
		err = gameAudioLib.Load(audio_dlls[i], currDir);
		if(err == KErrNone) { // great, we loaded a dll!
			gameAudioNew = (_gameAudioNew) gameAudioLib.Lookup(1);
			if(!gameAudioNew) {
				gameAudioLib.Close();
				err = KErrGeneral;
				DEBUGPRINT(_L("  loaded, but Lookup(1) failed."));
			} else
				break; // done
		} else
			DEBUGPRINT(_L("  load failed! (%i)"), err);;
	}

	if(err != KErrNone)
		 gameAudioNew = gameAudioNew_failer;
	else DEBUGPRINT(_L("  audio dll loaded!"));;

	DumpMemInfo();

	// try to start pico
	DEBUGPRINT(_L("PicoInit();"));
	PicoInit();

#ifdef TEST_FRAMEBUFF
	iFrameBuffer = (TUint8 *) malloc(208*320*4);
#endif
}


// does not return
static void MainExit()
{
	RProcess thisProcess;

	DEBUGPRINT(_L("%i: cleaning up.."), thisProcess.Id());

	// save SRAM
	if((currentConfig.iFlags & 1) && SRam.changed) {
		saveLoadGame(0, 1);
		SRam.changed = 0;
	}

	PicoExit();

	if(gameAudio) delete gameAudio;

	if(gameAudioLib.Handle()) gameAudioLib.Close();

	// Polled AS
	//delete CPolledActiveScheduler::Instance();

	DEBUGPRINT(_L("%i: terminating.."), thisProcess.Id());	
	thisProcess.Terminate(0);
}


static void CheckForLauncher()
{
	RProcess launcherProcess;

	// check for launcher, we are useless without it
	if(launcherProcess.Open(launcherProcessId) != KErrNone || launcherProcess.ExitCategory().Length() != 0) {
		#ifdef __DEBUG_PRINT
		RProcess thisProcess;
		DEBUGPRINT(_L("%i: launcher process is gone, terminating.."), thisProcess.Id());
		if(launcherProcess.Handle()) {
				TExitCategoryName ecn = launcherProcess.ExitCategory();
			DEBUGPRINT(_L("%i: launcher exit category: %S"), thisProcess.Id(), &ecn);
			launcherProcess.Close();
		}
		#endif
		MainExit();
	}
	launcherProcess.Close();
}


void DumpMemInfo()
{
	TInt	ramSize, ramSizeFree, romSize;
	
	HAL::Get(HALData::EMemoryRAM, ramSize);
	HAL::Get(HALData::EMemoryRAMFree, ramSizeFree);
	HAL::Get(HALData::EMemoryROM, romSize);

	DEBUGPRINT(_L("ram=%dKB, ram_free=%dKB, rom=%dKB"), ramSize/1024, ramSizeFree/1024, romSize/1024);
}



TInt E32Main()
{
	// first thing's first
	RThread().SetExceptionHandler(&ExceptionHandler, -1);

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

	CTrapCleanup* cleanup=CTrapCleanup::New();

	TRAPD(error, TargetEpocGameL());

	__ASSERT_ALWAYS(!error, User::Panic(_L("Picosmall"), error));
	delete cleanup;

	return 0;
}


void CGameWindow::ConstructResourcesL()
{

	// connect to window server
	// tried to create it globally and not re-connect everytime,
	// but my window started to lose focus strangely
	User::LeaveIfError(iWsSession.Connect());

	//	 * Tell the Window Server not to mess about with our process priority
	//	 * Also, because of the way legacy games are written, they never sleep
	//	 * and thus never voluntarily yield the CPU. We set our process priority
	//	 * to EPriorityForeground and hope that a Telephony application on
	//	 * this device runs at EPriorityForeground as well. If not, tough! ;-)

	CGameWindow::iWsSession.ComputeMode(RWsSession::EPriorityControlDisabled);
	RProcess me;
	me.SetPriority(EPriorityForeground);

	iWsScreen=new(ELeave) CWsScreenDevice(iWsSession);
	User::LeaveIfError(iWsScreen->Construct());
//	User::LeaveIfError(iWsScreen->CreateContext(iWindowGc));

	iWsWindowGroup=RWindowGroup(iWsSession);
	User::LeaveIfError(iWsWindowGroup.Construct((TUint32)&iWsWindowGroup));
	//iWsWindowGroup.SetOrdinalPosition(0);
	//iWsWindowGroup.SetName(KServerWGName);
	iWsWindowGroup.EnableScreenChangeEvents(); // flip events (EEventScreenDeviceChanged)
	iWsWindowGroup.EnableFocusChangeEvents(); // EEventFocusGroupChanged
	iWsWindowGroup.SetOrdinalPosition(0, 1); // TInt aPos, TInt aOrdinalPriority

	iWsWindow=RWindow(iWsSession);
	User::LeaveIfError(iWsWindow.Construct(iWsWindowGroup, (TUint32)&iWsWindow));
	iWsWindow.SetSize(iWsScreen->SizeInPixels());
	iWsWindow.PointerFilter(EPointerFilterDrag, 0);
	iWsWindow.SetPointerGrab(ETrue);
	iWsWindow.SetVisible(ETrue);
	iWsWindow.Activate();

	// request access through RDirectScreenAccess api, but don't care about the result
	RRegion *dsa_region = 0;
	iDSA = new(ELeave) RDirectScreenAccess(iWsSession);
	if(iDSA->Construct() == KErrNone)
		iDSA->Request(dsa_region, iDSAstatus, iWsWindow);
	DEBUGPRINT(_L("DSA: %i"), dsa_region ? dsa_region->Count() : -1);

	// now get the screenbuffer
	TScreenInfoV01			screenInfo;
	TPckg<TScreenInfoV01>	sI(screenInfo);
	UserSvr::ScreenInfo(sI);

	if(!screenInfo.iScreenAddressValid)
		User::Leave(KErrNotSupported);

#ifndef TEST_FRAMEBUFF
	TUint8 *iFrameBuffer = (TUint8*) screenInfo.iScreenAddress;
#endif
	TInt p800 = 0;

	switch(machineUid)
	{
		case 0x101f6b26: // A9xx & A10xx
		case 0x101f6b27: // Chinese A10xx
		case 0x101fd279: // P3x
		   iFrameBuffer += 32;
		   keyConfig = keyConfigMotA;
		   break;
		case 0x101f408b: // P800
		   p800 = 1;
		//case 0x101fb2ae: // P900
		//case 0x10200ac6: // P910
		default:	
		   keyConfig = keyConfigPXXX;
		   break;
	}
	DEBUGPRINT(_L("framebuffer=0x%08x (%dx%d)"), iFrameBuffer,
					screenInfo.iScreenSize.iWidth, screenInfo.iScreenSize.iHeight);

	// vidInit
	User::LeaveIfError(vidInit(iWsScreen->DisplayMode(), iFrameBuffer, p800));

	// register for keyevents
	for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++) {
		// release all keys
		e->flags &= ~1;
		if(e->flags & 0x80) {
			// key disabled
			e->handle1 = e->handle2 = -1;
			continue;
		}
		e->handle1 = iWsWindowGroup.CaptureKeyUpAndDowns(e->scanCode, 0, 0, 2);
		// although we only use UpAndDown events, register simple events too,
		// just to prevent fireing their default actions.
		if(e->keyCode) e->handle2 = iWsWindowGroup.CaptureKey(e->keyCode, 0, 0, 2); // priority of 1 is not enough on my phone
	}

	// try to start the audio engine
	static int PsndRate_old = 0, PicoOpt_old = 0, pal_old = 0;

	if(gamestate == PGS_Running && (currentConfig.iFlags & 4)) {
		TInt err = 0;
		if(PsndRate != PsndRate_old || (PicoOpt&11) != (PicoOpt_old&11) || Pico.m.pal != pal_old) {
			// if rate changed, reset all enabled chips, else reset only those chips, which were recently enabled
			//sound_reset(PsndRate != PsndRate_old ? PicoOpt : (PicoOpt&(PicoOpt^PicoOpt_old)));
			sound_rerate();
		}
		if(!gameAudio || PsndRate != PsndRate_old || ((PicoOpt&8) ^ (PicoOpt_old&8)) || Pico.m.pal != pal_old) { // rate or stereo or pal/ntsc changed
			if(gameAudio) delete gameAudio; gameAudio = 0;
			DEBUGPRINT(_L("starting audio: %i len: %i stereo: %i, pal: %i"), PsndRate, PsndLen, PicoOpt&8, Pico.m.pal);
			TRAP(err, gameAudio = gameAudioNew(PsndRate, (PicoOpt&8) ? 1 : 0, PsndLen, Pico.m.pal ? 10 : 12));
		}
		if( gameAudio)
			TRAP(err, PsndOut = gameAudio->ResumeL());
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

	// start key WS event polling
	iWsSession.EventReady(&iWsEventStatus);
	DEBUGPRINT(_L("CGameWindow::ConstructResourcesL() finished."));
}


void CGameWindow::FreeResources()
{
	if(gameAudio) gameAudio->Pause();

	// free RDirectScreenAccess stuff
	iDSA->Cancel();
	iDSA->Close();
	delete iDSA;
	iDSA = NULL;

	iWsSession.EventReadyCancel();

	for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++) {
		if(e->handle2 >= 0) iWsWindowGroup.CancelCaptureKey(e->handle2);
		if(e->handle1 >= 0) iWsWindowGroup.CancelCaptureKeyUpAndDowns(e->handle1);
	}

	if(iWsWindow.WsHandle())
		iWsWindow.Close();

	if(iWsWindowGroup.WsHandle())
		iWsWindowGroup.Close();

	// these must be deleted before calling iWsSession.Close()
//	delete iWindowGc;
//	iWindowGc = NULL;

	delete iWsScreen;
	iWsScreen = NULL;

	// emu might change renderer by itself, so we may need to sync config
	if(PicoOpt != currentConfig.iPicoOpt) {
		currentConfig.iFlags |= 0x80;
		CGameWindow::SendClientWsEvent(EEventKeyCfgDone);
	}

	if(iWsSession.WsHandle())
		iWsSession.Close();
	
	vidFree();

#ifdef TEST_FRAMEBUFF
	FILE *tmp = fopen("d:\\temp\\screen.raw", "wb");
	fwrite(iFrameBuffer, 1, 208*320*4, tmp);
	fclose(tmp);
#endif
}


void CGameWindow::DoKeys(timeval &time)
{
	TWsEvent iWsEvent;
	TInt iWsEventType;
	unsigned long allActions = 0;
	static unsigned long areaActions = 0, forceUpdate = 0;
	int i, nEvents;

	// detect if user is holding power button
	static timeval powerPushed = { 0, 0 };
	if(powerPushed.tv_sec) {
		if((time.tv_sec*1000000+time.tv_usec) - (powerPushed.tv_sec*1000000+powerPushed.tv_usec) > 1000000) { // > 1 sec
			gamestate = PGS_Paused;
			powerPushed.tv_sec = powerPushed.tv_usec = 0;
		}
	}

	for(nEvents = 0; iWsEventStatus != KRequestPending; nEvents++)
	{
		iWsSession.GetEvent(iWsEvent);
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
						areaActions = currentConfig.iAreaBinds[i];
						break;
					}
			}
		}
		else if(iWsEventType == EEventKeyDown || iWsEventType == EEventKeyUp) {
			TInt iScanCode = iWsEvent.Key()->iScanCode;

			for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++)
				if(iScanCode == e->scanCode) {
					if(iWsEventType == EEventKeyDown) e->flags |=  1;
					else if((e->flags & 2) == 0)      e->flags &= ~1;
					break;
				}

			// power?
			if(iScanCode == EScanPxxxPower || iScanCode == EScanMotAEnd) {
				if(iWsEventType == EEventKeyDown)
					 powerPushed = time;
				else powerPushed.tv_sec = powerPushed.tv_usec = 0;
			}
		}
		else if(iWsEventType == EEventScreenDeviceChanged) {
			// we have the priority, so the launcher will not be able to process this, but it has to
			User::After(500000);
			reset_timing = 1;
		}
		else if(iWsEventType == EEventFocusGroupChanged) {
			TInt launcherGrpId = iWsSession.FindWindowGroupIdentifier(0, iLauncherThreadId);
			TInt focusGrpId = iWsSession.GetFocusWindowGroup();
			DEBUGPRINT(_L("EEventFocusGroupChanged: %i, our: %i, launcher: %i"),
						focusGrpId, iWsWindowGroup.Identifier(), launcherGrpId);
			// if it is not us and not launcher that got focus, pause emu
			if(focusGrpId != iWsWindowGroup.Identifier() && focusGrpId != launcherGrpId)
				gamestate = PGS_Paused;
		}

		iWsEventStatus = KRequestPending;
		iWsSession.EventReady(&iWsEventStatus);
	}

	if(nEvents || forceUpdate) {
		allActions = areaActions;
		forceUpdate = 0;

		// add all pushed button actions
		i = 0;
		for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++, i++) {
			if(e->flags & 1) allActions |= currentConfig.iKeyBinds[i];
			if((e->flags& 3) == 3) forceUpdate = 1;
			if(e->flags & 2) e->flags &= ~1;
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

	// to detect if user is holding power button
	static int powerIters = 0;

	while(iWsEventStatus != KRequestPending)
	{
		TUint currentActCode = 1 << which;

		iWsSession.GetEvent(iWsEvent);

		// pointer events?
		if(iWsEvent.Type() == EEventPointer) {
			TPoint p = iWsEvent.Pointer()->iPosition;
			TRect prev(190, 112, 208, 126);
			TRect next(190, 194, 208, 208);

			if(iWsEvent.Pointer()->iType == TPointerEvent::EButton1Down) {
				     if(prev.Contains(p)) do { which = (which-1) & 0x1F; } while(!actionNames[which]);
				else if(next.Contains(p)) do { which = (which+1) & 0x1F; } while(!actionNames[which]);
				else if(which == 31) gamestate = PGS_Paused; // done
				else {
					const TPicoAreaConfigEntry *e = areaConfig + 1;
					for(i = 0; e->rect != TRect(0,0,0,0); e++, i++)
						if(e->rect.Contains(p)) {
							currentConfig.iAreaBinds[i] ^= currentActCode;
							break;
						}
				}
			}
		}
		else if(iWsEvent.Type() == EEventKeyDown || iWsEvent.Type() == EEventKeyUp)
		{
			//if(iWsEvent.Type() == EEventKey)
			//	DEBUGPRINT(_L("iWsEvent.Key()->iCode=0x%08x"), iWsEvent.Key()->iCode);

			//if(iWsEvent.Type() == EEventKeyDown)
			//	DEBUGPRINT(_L("EEventKeyDown iScanCode=0x%08x"), iWsEvent.Key()->iScanCode);

			//if(iWsEvent.Type() == EEventKeyUp)
			//	DEBUGPRINT(_L("EEventKeyUp   iScanCode=0x%08x"), iWsEvent.Key()->iScanCode);

			// key events?
			if(iWsEvent.Type() == EEventKeyDown) {
				if(iWsScreen->CurrentScreenMode() == 1 && iWsEvent.Key()->iScanCode == EScanPxxxJogUp) {
					do { which = (which-1) & 0x1F; } while(!actionNames[which]);
				} else if(iWsScreen->CurrentScreenMode() == 1 && iWsEvent.Key()->iScanCode == EScanPxxxJogDown) {
					do { which = (which+1) & 0x1F; } while(!actionNames[which]);
				} else if(which == 31) {
					gamestate = PGS_Paused;
					if(iWsScreen->CurrentScreenMode()) // flip closed
						vidDrawFCconfigDone();
				} else {
					i = 0;
					for(TPicoKeyConfigEntry *e = keyConfig; e->name; e++, i++)
						if(iWsEvent.Key()->iScanCode == e->scanCode)
							if(!(e->flags&0x40)) currentConfig.iKeyBinds[i] ^= currentActCode;
				}
			}

			// power?
			if(iWsEvent.Key()->iScanCode == EScanPxxxPower || iWsEvent.Key()->iScanCode == EScanMotAEnd)
			{
					 if(iWsEvent.Type() == EEventKeyDown) powerIters = 1;
				else if(iWsEvent.Type() == EEventKeyUp)   powerIters = 0;
			}
		}
		else if(iWsEvent.Type() == EEventScreenDeviceChanged) {
			// trying to fix the P910 problem when something steals focus (and returns it after a while?)
			User::After(300000);
		}
		else if(iWsEvent.Type() == EEventFocusGroupChanged) {
			TInt launcherGrpId = iWsSession.FindWindowGroupIdentifier(0, iLauncherThreadId);
			TInt focusGrpId = iWsSession.GetFocusWindowGroup();
			DEBUGPRINT(_L("EEventFocusGroupChanged: %i, our: %i, launcher: %i"),
						focusGrpId, iWsWindowGroup.Identifier(), launcherGrpId);
			// if it is not us and not launcher that got focus, exit config mode
			if(focusGrpId != iWsWindowGroup.Identifier() && focusGrpId != launcherGrpId) {
				// don't give up that easily. May be the focus will be given back.
				for (int i = 0; i < 4; i++) {
					User::After(200000);
					focusGrpId = iWsSession.GetFocusWindowGroup();
					if(focusGrpId == iWsWindowGroup.Identifier() || focusGrpId == launcherGrpId) break;
				}
				if(focusGrpId != iWsWindowGroup.Identifier() && focusGrpId != launcherGrpId) 
					// we probably should give up now
					gamestate = PGS_Paused;
			}
		}

		iWsEventStatus = KRequestPending;
		iWsSession.EventReady(&iWsEventStatus);
	}

	if(powerIters) { // iterations when power was down
		powerIters++;
		if(powerIters > 5) {
			gamestate = PGS_Paused;
			powerIters = 0;
		}
	}
}


void CGameWindow::RunEvents(TUint32 which)
{
	if(which & 0x4000) currentConfig.iFrameskip = -1;
	if(which & 0x2000) currentConfig.iFrameskip =  8;
	if(which & 0x1800) { // save or load (but not both)
		if(PsndOut) gameAudio->Pause(); // this may take a while, so we pause sound output

		vidDrawNotice((which & 0x1000) ? "LOADING@GAME" : "SAVING@GAME");
		saveLoadGame(which & 0x1000);

		if(PsndOut) PsndOut = gameAudio->ResumeL();
		reset_timing = 1;
	}
	if(which & 0x0400) gamestate = PGS_Paused;
	if(which & 0x0200) { // switch renderer
		if(currentConfig.iScreenMode == TPicoConfig::PMCenter && !noticeMsgTime.tv_sec &&
			(currentConfig.iScreenRotation == TPicoConfig::PRot90 || currentConfig.iScreenRotation == TPicoConfig::PRot270)) {

			PicoOpt^=0x10;
			vidInit(iWsScreen->DisplayMode(), 0, 0, 1);

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
	if(which & 0x0020) if(gameAudio) gameAudio->ChangeVolume(0); // for Motorolas (broken?)
	if(which & 0x0010) if(gameAudio) gameAudio->ChangeVolume(1);
}


// send event to launcher windowgroup (WS session MUST be alive)
void CGameWindow::SendClientWsEvent(TInt type)
{
	if(!iWsSession.Handle()) {
		DEBUGPRINT(_L("SendClientWsEvent(%i) called on dead iWsSession."), type);
		return;
	}

	TWsEvent event;
	event.SetType(type);
	event.SetTimeNow();

	TInt launcherGrpId = iWsSession.FindWindowGroupIdentifier(0, iLauncherThreadId);
	if(launcherGrpId != KErrNotFound)
		 iWsSession.SendEventToWindowGroup(launcherGrpId, event);
	else DEBUGPRINT(_L("failed to send event %i to launcher."), event.Type());
}


size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
	return gzread(file, p, _n);
}


size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
{
	return gzwrite(file, p, _n);
}


// this function is shared between both threads
int saveLoadGame(int load, int sram)
{
	int res = 0;

	if(!RomFileName) return -1;

	// make save filename
	char saveFname[KMaxFileName];
	strcpy(saveFname, RomFileName);
	saveFname[KMaxFileName-8] = 0;
	if(saveFname[strlen(saveFname)-4] == '.') saveFname[strlen(saveFname)-4] = 0;
	if(sram) strcat(saveFname, ".srm");
	else {
		if(state_slot > 0 && state_slot < 10) sprintf(saveFname, "%s.%i", saveFname, state_slot);
		strcat(saveFname, ".mds");
	}
	
	DEBUGPRINT(_L("saveLoad (%i, %i): %S"), load, sram, DO_CONV(saveFname));

	if(sram) {
		FILE *sramFile;
		int sram_size = SRam.end-SRam.start+1;
		if(SRam.reg_back & 4) sram_size=0x2000;
		if(!SRam.data) return 0; // SRam forcefully disabled for this game
		if(load) {
			sramFile = fopen(saveFname, "rb");
			if(!sramFile) return -1;
			fread(SRam.data, 1, sram_size, sramFile);
			fclose(sramFile);
		} else {
			// sram save needs some special processing
			// see if we have anything to save
			for(; sram_size > 0; sram_size--)
				if(SRam.data[sram_size-1]) break;
			
			if(sram_size) {
				sramFile = fopen(saveFname, "wb");
				res = fwrite(SRam.data, 1, sram_size, sramFile);
				res = (res != sram_size) ? -1 : 0;
				fclose((FILE *) sramFile);
			}
		}
		return res;
	} else {
		void *PmovFile = NULL;
		// try gzip first
		if(currentConfig.iFlags & 0x80) {
			strcat(saveFname, ".gz");
			if( (PmovFile = gzopen(saveFname, load ? "rb" : "wb")) ) {
				areaRead  = gzRead2;
				areaWrite = gzWrite2;
				if(!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
			} else
				saveFname[strlen(saveFname)-3] = 0;
		}
		if(!PmovFile) { // gzip failed or was disabled
			if( (PmovFile = fopen(saveFname, load ? "rb" : "wb")) ) {
				areaRead  = fread;
				areaWrite = fwrite;
			}
		}
		if(PmovFile) {
			PmovState(load ? 6 : 5, PmovFile);
			strcpy(noticeMsg, load ? "GAME@LOADED" : "GAME@SAVED");
			if(areaRead == gzRead2)
				 gzclose(PmovFile);
			else fclose ((FILE *) PmovFile);
			PmovFile = 0;
		} else {
			strcpy(noticeMsg, load ? "LOAD@FAILED" : "SAVE@FAILED");
			res = -1;
		}

		gettimeofday(&noticeMsgTime, 0);
		return res;
	}
}

// static class members
RWsSession				CGameWindow::iWsSession;
RWindowGroup			CGameWindow::iWsWindowGroup;
RWindow					CGameWindow::iWsWindow;
CWsScreenDevice*		CGameWindow::iWsScreen = NULL;
CWindowGc*				CGameWindow::iWindowGc = NULL;
TRequestStatus			CGameWindow::iWsEventStatus = KRequestPending;
TThreadId				CGameWindow::iLauncherThreadId = 0;
RDirectScreenAccess*	CGameWindow::iDSA;
TRequestStatus			CGameWindow::iDSAstatus = KRequestPending;
