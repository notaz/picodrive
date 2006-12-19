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
//#include <eikdll.h>

#include "../version.h"


CGameRunner::~CGameRunner()
{
	Cancel();

	RProcess process;
	if(process.Open(iProcessId) == KErrNone) {
		process.Terminate(1);
		process.Close();
	}
}

CGameRunner::CGameRunner(MGameWatcher& aGameWatcher)
: CActive(CActive::EPriorityStandard), iGameWatcher(aGameWatcher)
{
}


CGameRunner* CGameRunner::NewL(MGameWatcher& aGameWatcher)
{
	CGameRunner* self = new(ELeave) CGameRunner(aGameWatcher);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();			// self
	return self;
}

void CGameRunner::ConstructL()
{
	RProcess newProcess, thisProcess;

	// make path to picosmall
	TBuf<KMaxFileName> exePath;
	TBuf<KMaxFileName*3> tmpbuff; // hopefully large enough
	thisProcess.CommandLine(tmpbuff);
	TInt pos = tmpbuff.Find(_L(" "));
	if(pos == KErrNotFound) pos = tmpbuff.Length();
	for(pos--; pos > 2; pos--)
		if(tmpbuff[pos] == '\\') break;
	if(pos > 2) {
		exePath.Copy(tmpbuff.Ptr(), pos+1);
		exePath.Append(_L("PICOSMALL.EXE"));
	}

	DEBUGPRINT(_L("[app] starting EXE: %S"), &exePath);
	if(newProcess.Create(exePath, _L(""))) {
		CEikonEnv::Static()->InfoWinL(_L("Error"), _L("Failed to start emulation process."));
		thisProcess.Terminate(1);
	}

	iProcessId = newProcess.Id();
	DEBUGPRINT(_L("[app] newProcess.Id(): %d"), iProcessId);

	CActiveScheduler::Add(this);
	newProcess.SetOwner(thisProcess); // Warning: phone strangely reboots when attempting to get owner after thisProcess exits
	newProcess.Logon(iStatus);

	SetActive();

	newProcess.Resume(); // start execution
	newProcess.Close();
}

void CGameRunner::RunL()
{
	iGameWatcher.NotifyEmuDeath();
}

void CGameRunner::DoCancel()
{
	RProcess process;
	if(process.Open(iProcessId) == KErrNone) {
		process.LogonCancel(iStatus);
		process.Close();
	}
}


// CExitForcer
CExitForcer::~CExitForcer()
{
	Cancel();
}

CExitForcer::CExitForcer(MGameWatcher& aGameWatcher) : CActive(CActive::EPriorityStandard), iGameWatcher(aGameWatcher)
{
}


CExitForcer* CExitForcer::NewL(MGameWatcher& aGameWatcher, TInt ms)
{
	CExitForcer* self = new(ELeave) CExitForcer(aGameWatcher);
	CleanupStack::PushL(self);
	self->ConstructL(ms);
	CleanupStack::Pop();			// self
	return self;
}

void CExitForcer::ConstructL(TInt ms)
{
	CActiveScheduler::Add(this);
	iTimer.CreateLocal();
	iTimer.After(iStatus, ms*1000);
	SetActive();
}

void CExitForcer::RunL()
{
	iGameWatcher.NotifyForcedExit();
}

void CExitForcer::DoCancel()
{
	if(iTimer.Handle()) {
		iTimer.Cancel();
		iTimer.Close();
	}
}


// CThreadWatcher
CThreadWatcher::~CThreadWatcher()
{
	Cancel();
}

CThreadWatcher::CThreadWatcher(MGameWatcher& aGameWatcher, const TDesC& aName)
: CActive(CActive::EPriorityStandard), iGameWatcher(aGameWatcher), iName(aName)
{
}


CThreadWatcher* CThreadWatcher::NewL(MGameWatcher& aGameWatcher, const TDesC& aName)
{
	CThreadWatcher* self = new(ELeave) CThreadWatcher(aGameWatcher, aName);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();			// self
	return self;
}

void CThreadWatcher::ConstructL()
{
	CActiveScheduler::Add(this);
	RThread thread;
	if(thread.Open(iName) == KErrNone) {
		thread.Logon(iStatus);
		thread.Close();
		SetActive();
	}
}

void CThreadWatcher::RunL()
{
	iGameWatcher.NotifyEmuDeath();
}

void CThreadWatcher::DoCancel()
{
	RThread thread;
	if(thread.Open(iName) == KErrNone) {
		thread.LogonCancel(iStatus);
		thread.Close();
	}
}


// config
TPLauncherConfig::TPLauncherConfig(TPicoConfig &cfg)
: iEmuConfig(cfg)
{
	iLastROMFile.Copy(_L("C:\\"));

	// ini
	TBuf<KMaxFileName*3> tmpbuff; // hopefully large enough
	RProcess me;
	me.CommandLine(tmpbuff);
	TInt pos = tmpbuff.Find(_L(" "));
	if(pos == KErrNotFound) pos = tmpbuff.Length();
	if(pos > 3) {
		iIniFileName.Copy(tmpbuff.Ptr(), pos-3);
		iIniFileName.Append(_L("ini"));
	}
	//DEBUGPRINT(_L("[app] made ini: %S"), &iIniFileName);
}


void TPLauncherConfig::Load()
{
	RFile file;

	if(!file.Open(CEikonEnv::Static()->FsSession(), iIniFileName, 0))
	{
		TInt version;
		TPckg<TInt>			pkg_version(version);
		TPckg<TBool>		pkg_Pad(iPad);
		TBuf8<KMaxFileName> pad0; // reserved for future use (6 words)
		TPtr8				picoCfg((TUint8*) &iEmuConfig, sizeof(iEmuConfig));

		file.Read(pkg_version);
		file.Read(pkg_Pad);
		file.Read(pad0, 24);
		file.Read(pad0, KMaxFileName);
		file.Read(picoCfg);

		TBuf8<KMaxFileName> file8(pad0.Ptr()); // take as zero terminated string
		iLastROMFile.Copy(file8);
		//DEBUGPRINT(_L("[app] iLastROMFile (%i): %S"), iLastROMFile.Length(), &iLastROMFile);

		file.Close();
	}
}

void TPLauncherConfig::Save()
{
	RFile file;

	if(!file.Replace(CEikonEnv::Static()->FsSession(), iIniFileName, EFileWrite)) {
		TInt version = (KPicoMajorVersionNumber<<24)+(KPicoMinorVersionNumber<<16);
		TPckgC<TInt>		pkg_version(version);
		TPckgC<TBool>		pkg_Pad(iPad);
		TBuf8<KMaxFileName> pad0; pad0.FillZ(KMaxFileName);
		TBuf8<KMaxFileName> file8; file8.Copy(iLastROMFile);
		TPtrC8				picoCfg((TUint8*) &iEmuConfig, sizeof(iEmuConfig));

		file.Write(pkg_version);
		file.Write(pkg_Pad);			// 0x0004
		file.Write(pad0, 24);			// 0x0008, reserved for future use (6 words)
		file.Write(file8);				// 0x0020
		file.Write(pad0, KMaxFileName-file8.Length());
		file.Write(picoCfg);			// 0x0120

		file.Close();
	}
}
