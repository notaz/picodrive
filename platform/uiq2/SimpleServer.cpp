// SimpleServer.cpp

#include <e32svr.h>
#include <e32math.h>
#include <e32uid.h>

#include <string.h>

#include "debug.h"

#include "version.h"
#include "ClientServer.h"
#include "SimpleServer.h"
#include "pico\picoInt.h"

extern TInt machineUid;
extern int gamestate, gamestate_prev;
extern TPicoConfig currentConfig;
extern TPicoKeyConfigEntry keyConfigMotA[];
extern const char *actionNames[];
const char *RomFileName = 0;
int pico_was_reset = 0;


// utility
unsigned int bigend(unsigned int l)
{
	return (l>>24)|((l>>8)&0xff00)|((l<<8)&0xff0000)|(l<<24);
}


//**********************************
//CPicoServServer
//**********************************


CPicoServServer::CPicoServServer(TInt aPriority)
	: CServer(aPriority)
{
}


// Create and start a new count server.
void CPicoServServer::New()
{
	CPicoServServer *pS=new CPicoServServer(EPriority);
	__ASSERT_ALWAYS(pS!=NULL,PanicServer(ESvrCreateServer));
	pS->StartL(KServerName);
}


// Create a new server session.
CSharableSession *CPicoServServer::NewSessionL(const TVersion &aVersion) const
{
	// check we're the right version
	TVersion v(KPicoMajorVersionNumber,KPicoMinorVersionNumber,0);
	if (!User::QueryVersionSupported(v,aVersion))
		User::Leave(KErrNotSupported);
	// make new session
	RThread aClient = Message().Client();
	return CPicoServSession::NewL(aClient, (CPicoServServer*)this);
}


//**********************************
//CPicoServSession
//**********************************


// constructor - must pass client to CSession
CPicoServSession::CPicoServSession(RThread &aClient, CPicoServServer *aServer)
: CSession(aClient), rom_data(0)
{
//	iPicoSvr=aServer;
}

CPicoServSession* CPicoServSession::NewL(RThread &aClient, CPicoServServer * aServer)
{
	return new(ELeave) CPicoServSession(aClient,aServer);
}


void CPicoServSession::ServiceL(const RMessage& aMessage)
{
	TRAPD(err,DispatchMessageL(aMessage));
	aMessage.Complete(err);
}



// service a client request; test the opcode and then do appropriate servicing
void CPicoServSession::DispatchMessageL(const RMessage &aMessage)
{
	switch (aMessage.Function()) {
		case PicoMsgLoadState: 
			if(!rom_data) User::Leave(-1); // no ROM
			User::LeaveIfError(saveLoadGame(1));
			gamestate = PGS_Running;
			return;

		case PicoMsgSaveState:
			if(!rom_data) User::Leave(-1);
			User::LeaveIfError(saveLoadGame(0));
			gamestate = PGS_Running;
			return;

		case PicoMsgLoadROM:
			loadROM();
			return;
		
		case PicoMsgResume:
			if(rom_data) gamestate = PGS_Running;
			return;

		case PicoMsgReset: 
			if(rom_data) {
				PicoReset(0);
				pico_was_reset = 1;
				gamestate = PGS_Running;
			}
			return;

		case PicoMsgKeys:
			gamestate = PGS_KeyConfig;
			return;

		case PicoMsgPause:
			gamestate = PGS_Paused;
			return;

		case PicoMsgQuit:
			DEBUGPRINT(_L("got quit msg."));
			gamestate = PGS_Quit;
			return;

		// config change
		case PicoMsgConfigChange: // launcher -> emu
			changeConfig();
			return;

		case PicoMsgRetrieveConfig: // emu -> launcher
			sendConfig();
			return;

		case PicoMsgRetrieveDebugStr: // emu -> launcher
			sendDebug();
			return;

		// requests we don't understand at all are a different thing,
		// so panic the client here, this function also completes the message
		default:
			PanicClient(EBadRequest);
			return;
	}
}


void CPicoServSession::loadROM()
{
	TInt res;

	const TAny* pD=Message().Ptr0();

	// TInt desLen=Message().Client().GetDesLength(pD);

	if(rom_data) {
		// save SRAM for previous ROM
		if(currentConfig.iFlags & 1)
			saveLoadGame(0, 1);
	}

	RomFileName = 0;
	if(rom_data) {
		free(rom_data);
		rom_data = 0;
	}

	// read the contents of the client pointer into a TPtr.
	static TBuf8<KMaxFileName> writeBuf;
	TRAP(res,Message().ReadL(pD,writeBuf));
	if (res!=KErrNone) {
		PanicClient(EBadDescriptor);
		return;
	}

	// detect wrong extensions (.srm and .mds)
	TBuf8<5> ext;
	ext.Copy(writeBuf.Right(4));
	ext.LowerCase();
	if(!strcmp((char *)ext.PtrZ(), ".srm") || !strcmp((char *)ext.PtrZ(), "s.gz") || // .mds.gz
	   !strcmp((char *)ext.PtrZ(), ".mds")) {
		User::Leave(3);
		return;
	}

	FILE *rom = fopen((char *) writeBuf.PtrZ(), "rb");
	if(!rom) {
		DEBUGPRINT(_L("failed to open rom."));
		User::Leave(1);
		return;
	}


	unsigned int rom_size = 0;
	// zipfile support
	if(!strcmp((char *)ext.PtrZ(), ".zip")) {
		fclose(rom);
		res = CartLoadZip((const char *) writeBuf.PtrZ(), &rom_data, &rom_size);
		if(res) {
			User::Leave(res);
			return;
		}
	} else {
		if( (res = PicoCartLoad(rom, &rom_data, &rom_size)) ) {
			DEBUGPRINT(_L("PicoCartLoad() failed."));
			fclose(rom);
			User::Leave(2);
			return;
		}
		fclose(rom);
	}

	// detect wrong files (Pico crashes on very small files), also see if ROM EP is good
	if(rom_size <= 0x200 || strncmp((char *)rom_data, "Pico", 4) == 0 ||
	  ((*(TUint16 *)(rom_data+4)<<16)|(*(TUint16 *)(rom_data+6))) >= (int)rom_size) {
		free(rom_data);
		rom_data = 0;
		User::Leave(3); // not a ROM
	}

	DEBUGPRINT(_L("PicoCartInsert(0x%08X, %d);"), rom_data, rom_size);
	if(PicoCartInsert(rom_data, rom_size)) {
		User::Leave(2);
		return;
	}

	pico_was_reset = 1;

	// global ROM file name for later use
	RomFileName = (const char *) writeBuf.PtrZ();

	// load SRAM for this ROM
	if(currentConfig.iFlags & 1)
		saveLoadGame(1, 1);

	// debug
	#ifdef __DEBUG_PRINT
	TInt cells = User::CountAllocCells();
	TInt mem;
	User::AllocSize(mem);
	DEBUGPRINT(_L("comm:   cels=%d, size=%d KB"), cells, mem/1024);
	gamestate = PGS_DebugHeap;
	gamestate_prev = PGS_Running;
	#else
	gamestate = PGS_Running;
	#endif
}


void CPicoServSession::changeConfig()
{
	DEBUGPRINT(_L("got new config."));

	// receve it
	const TAny* pD=Message().Ptr0();
	TPtr8 descr((TUint8*) &currentConfig, sizeof(currentConfig));
	TRAPD(res,Message().ReadL(pD, descr));
	if (res!=KErrNone) {
		PanicClient(EBadDescriptor);
		return;
	}

	// Motorola: enable experimental volume control
	if((machineUid&0xfffffff0) == 0x101f6b20) { // Motorolas
		if(currentConfig.iFlags & 0x40) {
			currentConfig.iKeyBinds[11]  =  0x00100000; // vol up
			currentConfig.iKeyBinds[12]  =  0x00200000; // vol down
			keyConfigMotA[11].flags |=  0x40; // add "not configurable" flag
			keyConfigMotA[12].flags |=  0x40;
		} else {
			currentConfig.iKeyBinds[11] &= ~0x00100000; // remove vol actions
			currentConfig.iKeyBinds[12] &= ~0x00200000;
			keyConfigMotA[11].flags &= ~0x40; // remove "not configurable" flag
			keyConfigMotA[12].flags &= ~0x40;
		}
	}

	// set region, PicoOpt and rate
	PicoRegionOverride = currentConfig.PicoRegion;
	PicoOpt = currentConfig.iPicoOpt;
	switch((currentConfig.iFlags>>3)&3) {
		case 1:  PsndRate=11025; break;
		case 2:  PsndRate=16000; break;
		case 3:  PsndRate=22050; break;
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
	if(currentConfig.iScreenMode == TPicoConfig::PMCenter &&
		(currentConfig.iScreenRotation == TPicoConfig::PRot90 || currentConfig.iScreenRotation == TPicoConfig::PRot270))
				 actionNames[25] = "RENDERER";
			else actionNames[25] = 0;
}


void CPicoServSession::sendConfig()
{
	// send current config to client
	currentConfig.iPicoOpt = PicoOpt;
	TPtrC8 descr((TUint8*) &currentConfig, sizeof(currentConfig));
	Write(Message().Ptr0(), descr);
}

#ifdef __DEBUG_PRINT
extern "C" char *debugString();
#endif

void CPicoServSession::sendDebug()
{
#ifdef __DEBUG_PRINT
	char *str = debugString();
	// send current config to client
	currentConfig.iPicoOpt = PicoOpt;
	TPtrC8 descr((TUint8*) str, 1024);
	Write(Message().Ptr0(), descr);
#endif
}

// panic the client
void CPicoServSession::PanicClient(TInt aPanic) const
{
	Panic(_L("PicoN client"), aPanic);
	// client screwed up - there is nothing for us to do now
	RProcess me;
	me.Terminate(1);
}


// write to the client thread; if unsuccessful, panic the client
void CPicoServSession::Write(const TAny* aPtr,const TDesC8& aDes,TInt anOffset)
{
	TRAPD(ret,WriteL(aPtr,aDes,anOffset);)
	if (ret!=KErrNone)
		PanicClient(EBadDescriptor);
}



//**********************************
//Global functions
//**********************************


// The server thread.
TInt CPicoServServer::ThreadFunction(TAny* anArg)
{
	// install our exception hanler first
	RThread().SetExceptionHandler(&ExceptionHandler, -1);

	// convert argument into semaphore reference
//	RSemaphore& semaphore=*(RSemaphore *)anArg;

	// start scheduler and server
	CActiveScheduler *pA=new CActiveScheduler;
	__ASSERT_ALWAYS(pA!=NULL,PanicServer(EMainSchedulerError));
	CActiveScheduler::Install(pA);
	//CTrapCleanup::New(); // docs say this is created automatically, but I somehow got E32USER-CBase 69 panic
	CPicoServServer::New();
	// signal that we've started
//	semaphore.Signal();
	// start fielding requests from clients
	CActiveScheduler::Start();
	// finished
	return(KErrNone);
}


// Panic the server
//GLDEF_C 
void PanicServer(TPicoServPanic aPanic)
{
	User::Panic(_L("PicoN server"),aPanic);
}


// Create the server thread
// This function is exported from the DLL and called from the client 
//EXPORT_C
TInt StartThread()
{
	TInt res=KErrNone;
	// create server - if one of this name does not already exist
	TFindServer findPicoServer(KServerName);
	TFullName name;
	if(findPicoServer.Next(name) == KErrNone) return -1; // we already exist

	RThread thread;
//	RSemaphore semaphore;
//	semaphore.CreateLocal(0); // create a semaphore so we know when thread finished
	res=thread.Create(KServerName,   // create new server thread
		CPicoServServer::ThreadFunction, // thread's main function
		KDefaultStackSize,
		KMinHeapSize,
		KPicoMaxHeapSize,
//		&semaphore // passed as TAny* argument to thread function
		0
		);

	if(res==KErrNone) { // thread created ok - now start it going
		thread.SetPriority(EPriorityNormal);
		thread.Resume(); // start it going
//		semaphore.Wait(); // wait until it's initialized
		thread.Close(); // we're no longer interested in the other thread
	}

//	semaphore.Close();

    return res;
}

