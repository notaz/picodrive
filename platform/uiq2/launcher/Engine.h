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
#include <etel.h>

#include "../ClientServer.h"

class RFs;

#ifdef __DEBUG_PRINT
	#define DEBUGPRINT(x...) RDebug::Print(x)
#else
	#define DEBUGPRINT(x...)
#endif


class MGameWatcher
{
public:
	virtual void NotifyEmuDeath() = 0;
	virtual void NotifyForcedExit() = 0;
};


class CGameRunner : public CActive
{
public:
	static CGameRunner* NewL(MGameWatcher& aGameWatcher);
	~CGameRunner();

	void KillAfter(TInt ms);

protected:
	CGameRunner(MGameWatcher& aGameWatcher);
	void ConstructL();

	virtual void RunL();
	virtual void DoCancel();

	MGameWatcher&		iGameWatcher;
	TProcessId			iProcessId;
};


class CExitForcer : public CActive
{
public:
	static CExitForcer* NewL(MGameWatcher& aGameWatcher, TInt ms);
	~CExitForcer();

protected:
	CExitForcer(MGameWatcher& aGameWatcher);
	void ConstructL(TInt ms);

	virtual void RunL();
	virtual void DoCancel();

	MGameWatcher&		iGameWatcher;
	RTimer				iTimer;
};


class CThreadWatcher : public CActive
{
public:
	static CThreadWatcher* NewL(MGameWatcher& aGameWatcher, const TDesC& aName);
	~CThreadWatcher();

protected:
	CThreadWatcher(MGameWatcher& aGameWatcher, const TDesC& aName);
	void ConstructL();

	virtual void RunL();
	virtual void DoCancel();

	MGameWatcher&		iGameWatcher;
	const TDesC&		iName; // thread name
};


// configuration emu process doesn't care about
class TPLauncherConfig {
public:
	TPLauncherConfig(TPicoConfig &cfg);
	void Load();
	void Save();

	TBool			iPad; // was iPauseOnCall
	TFileName		iLastROMFile;
	TPicoConfig		&iEmuConfig;

private:
	TFileName		iIniFileName;
};


#endif
