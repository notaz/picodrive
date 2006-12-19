// SimpleServer.h

#ifndef __SIMPLESERVER_H
#define __SIMPLESERVER_H

#include <e32base.h>


TInt StartThread();


// engine states
enum TPicoGameState {
	PGS_Running,
	PGS_Paused,
	PGS_Quit,
	PGS_KeyConfig,
	PGS_DebugHeap,
};

// needed for creating server thread.
const TUint KPicoMaxHeapSize=0x00800000;

// reasons for server panic
enum TPicoServPanic
{
	EBadRequest,
	EBadDescriptor,
	EMainSchedulerError,
	ESvrCreateServer,
	ESvrStartServer,
	ECreateTrapCleanup,
	ENotImplementedYet,
};


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


//**********************************
//CPicoServServer
//**********************************
//The server class; an active object.
//Contains an instance of RServer; a handle to the kernel server representation which is used 
//to receive messages. 

class CPicoServServer : public CServer
{
public:
	enum {EPriority=950};
public:
	static void New();
	virtual CSharableSession *NewSessionL(const TVersion &aVersion) const;
	static TInt ThreadFunction(TAny* aStarted);
protected:
	CPicoServServer(TInt aPriority);
private:
	TInt				iActive;
};


//**********************************
//CPicoServSession
//**********************************
//This class represents a session in the server.
//CSession::Client() returns the client thread.
//Functions are provided to respond appropriately to client messages.


class CPicoServSession : public CSession
{
public:
	// construct/destruct
	CPicoServSession(RThread &aClient, CPicoServServer * aServer);
	static CPicoServSession* NewL(RThread &aClient, CPicoServServer * aServer);
	//service request
	virtual void ServiceL(const RMessage &aMessage);
	void DispatchMessageL(const RMessage &aMessage);

	// services available
	void loadROM();
	void changeConfig();
	void sendConfig();
	void sendDebug();

protected:
	// panic the client
	void PanicClient(TInt aPanic) const;
	// safewrite between client and server
	void Write(const TAny* aPtr,const TDesC8& aDes,TInt anOffset=0);
private:
	//CPicoServServer *iPicoSvr;

	unsigned char *rom_data;
};



//**********************************
//global functions
//**********************************

// function to panic the server
GLREF_C void PanicServer(TPicoServPanic aPanic);
int saveLoadGame(int load, int sram=0);

#endif // __SIMPLESERVER_H
