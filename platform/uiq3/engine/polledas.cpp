/*******************************************************************
 *
 *	File:		PolledAS.cpp
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *	(c) Copyright 2002, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

/*
 * Oh Lord, forgive me for I have sinned.
 * In their infinite wisdom, Symbian Engineers have decided that
 * the Active Scheduler's queue of Active Objects is private
 * and no getters are provided... sigh.
 * This mere mortal will have to excercise the power of C pre-processor 
 * once more to circumvent the will of the gods.
 */


#include <e32std.h>

// from e32base.h
class CBase
	{
public:
	/**
	Default constructor
	*/
	inline CBase()	{}
	IMPORT_C virtual ~CBase();
	inline TAny* operator new(TUint aSize, TAny* aBase) __NO_THROW { Mem::FillZ(aBase, aSize); return aBase; }
	inline TAny* operator new(TUint aSize) __NO_THROW { return User::AllocZ(aSize); }
	inline TAny* operator new(TUint aSize, TLeave) { return User::AllocZL(aSize); }
	inline TAny* operator new(TUint aSize, TUint aExtraSize) { return User::AllocZ(aSize + aExtraSize); }
	inline TAny* operator new(TUint aSize, TLeave, TUint aExtraSize) { return User::AllocZL(aSize + aExtraSize); }
	IMPORT_C static void Delete(CBase* aPtr);
protected:
	IMPORT_C virtual TInt Extension_(TUint aExtensionId, TAny*& a0, TAny* a1);
private:
	CBase(const CBase&);
	CBase& operator=(const CBase&);
private:
	};


class TRequestStatusFaked
	{
public:
	inline TRequestStatusFaked() : iFlags(0) {};
	inline TRequestStatusFaked(TInt aVal) : iStatus(aVal), iFlags(aVal==KRequestPending ? TRequestStatusFaked::ERequestPending : 0) {}
/*	inline TInt operator=(TInt aVal);
	inline TBool operator==(TInt aVal) const;
*/
	inline TBool operator!=(TInt aVal) const {return(iStatus!=aVal);}
/*
	inline TBool operator>=(TInt aVal) const;
	inline TBool operator<=(TInt aVal) const;
	inline TBool operator>(TInt aVal) const;
	inline TBool operator<(TInt aVal) const;
	inline TInt Int() const;
private:
*/
	enum
		{
		EActive				= 1,  //bit0
		ERequestPending		= 2,  //bit1
		};
	TInt iStatus;
	TUint iFlags;
	friend class CActive;
	friend class CActiveScheduler;
	friend class CServer2;
	};


class CActive : public CBase
	{
public:
enum TPriority
	{
	EPriorityIdle=-100,
	EPriorityLow=-20,
	EPriorityStandard=0,
	EPriorityUserInput=10,
	EPriorityHigh=20,
	};
public:
	IMPORT_C ~CActive();
	IMPORT_C void Cancel();
	IMPORT_C void Deque();
	IMPORT_C void SetPriority(TInt aPriority);
	inline TBool IsActive() const {return(iStatus.iFlags&TRequestStatus::EActive);}
	inline TBool IsAdded() const {return(iLink.iNext!=NULL);}
	inline TInt Priority() const {return iLink.iPriority;}
protected:
	IMPORT_C CActive(TInt aPriority);
	IMPORT_C void SetActive();
	virtual void DoCancel() =0;
	virtual void RunL() =0;
	IMPORT_C virtual TInt RunError(TInt aError);
protected:
	IMPORT_C virtual TInt Extension_(TUint aExtensionId, TAny*& a0, TAny* a1);
public:
	TRequestStatusFaked iStatus; // hope this will work
private:
//	TBool iActive;
	TPriQueLink iLink;
	TAny* iSpare;
	friend class CActiveScheduler;
	friend class CServer;
	friend class CServer2;
	friend class CPrivatePolledActiveScheduler; // added
	};



class CActiveScheduler : public CBase
	{
	friend class CActiveSchedulerWait;
public:
	struct TLoop;
	typedef TLoop* TLoopOwner;
public:
	IMPORT_C CActiveScheduler();
	IMPORT_C ~CActiveScheduler();
	IMPORT_C static void Install(CActiveScheduler* aScheduler);
	IMPORT_C static CActiveScheduler* Current();
	IMPORT_C static void Add(CActive* aActive);
	IMPORT_C static void Start();
	IMPORT_C static void Stop();
	IMPORT_C static TBool RunIfReady(TInt& aError, TInt aMinimumPriority);
	IMPORT_C static CActiveScheduler* Replace(CActiveScheduler* aNewActiveScheduler);
	IMPORT_C virtual void WaitForAnyRequest();
	IMPORT_C virtual void Error(TInt aError) const;
	IMPORT_C void Halt(TInt aExitCode) const;
	IMPORT_C TInt StackDepth() const;
private:
	static void Start(TLoopOwner* aOwner);
	IMPORT_C virtual void OnStarting();
	IMPORT_C virtual void OnStopping();
	IMPORT_C virtual void Reserved_1();
	IMPORT_C virtual void Reserved_2();
	void Run(TLoopOwner* const volatile& aLoop);
	void DoRunL(TLoopOwner* const volatile& aLoop, CActive* volatile & aCurrentObj);
	friend class CPrivatePolledActiveScheduler; // added
protected:
	IMPORT_C virtual TInt Extension_(TUint aExtensionId, TAny*& a0, TAny* a1);
protected:
	inline TInt Level() const {return StackDepth();}	// deprecated
private:
	TLoop* iStack;
	TPriQue<CActive> iActiveQ;
	TAny* iSpare;
	};



class TCleanupItem;
class CleanupStack
	{
public:
	IMPORT_C static void PushL(TAny* aPtr);
	IMPORT_C static void PushL(CBase* aPtr);
	IMPORT_C static void PushL(TCleanupItem anItem);
	IMPORT_C static void Pop();
	IMPORT_C static void Pop(TInt aCount);
	IMPORT_C static void PopAndDestroy();
	IMPORT_C static void PopAndDestroy(TInt aCount);
	IMPORT_C static void Check(TAny* aExpectedItem);
	inline static void Pop(TAny* aExpectedItem);
	inline static void Pop(TInt aCount, TAny* aLastExpectedItem);
	inline static void PopAndDestroy(TAny* aExpectedItem);
	inline static void PopAndDestroy(TInt aCount, TAny* aLastExpectedItem);
	};


/*
 * This will declare CPrivatePolledActiveScheduler as a friend
 * of all classes that define a friend. CPrivatePolledActiveScheduler needs to
 * be a friend of CActive
 */
//#define friend friend class CPrivatePolledActiveScheduler; friend


/*
 * This will change the:
 *		 void DoStart();
 * method in CActiveScheduler to:
 *		 void DoStart(); friend class CPrivatePolledActiveScheduler;
 * We need this to access the private datamembers in CActiveScheduler.
 */
//#define DoStart() DoStart(); friend class CPrivatePolledActiveScheduler;
//#include <e32base.h>
#include "PolledAS.h"


class CPrivatePolledActiveScheduler : public CActiveScheduler
{
public:
	void Schedule();
};


void CPrivatePolledActiveScheduler::Schedule()
{
	TDblQueIter<CActive> q(iActiveQ);
	q.SetToFirst();
	FOREVER
	{
		CActive *pR=q++;
		if (pR)
		{
			//TRequestStatus::EActive				= 1,  //bit0
			//TRequestStatus::ERequestPending		= 2,  //bit1
			if (pR->IsActive() && pR->iStatus!=KRequestPending)
			{
//				pR->iActive=EFalse; // won't this cause trouble?
				pR->iStatus.iFlags&=~TRequestStatusFaked::EActive;
				//debugPrintFile(_L("as: %08x"), pR);
				TRAPD(r,pR->RunL());
				//pR->iStatus=TRequestStatus::ERequestPending;
				break;
			}
		}
		else
			break;
	}
}


static CPolledActiveScheduler* sPolledActiveScheduler = NULL;

CPolledActiveScheduler::~CPolledActiveScheduler()
{
	sPolledActiveScheduler = NULL;
	delete iPrivatePolledActiveScheduler;
}

CPolledActiveScheduler* CPolledActiveScheduler::NewL()
{
	// if (sPolledActiveScheduler == NULL)
	{
		sPolledActiveScheduler = new(ELeave)CPolledActiveScheduler;
		CleanupStack::PushL(sPolledActiveScheduler);
		sPolledActiveScheduler->ConstructL();
		CleanupStack::Pop();
	}
	return sPolledActiveScheduler;
}

void CPolledActiveScheduler::ConstructL()
{
	iPrivatePolledActiveScheduler = new(ELeave) CPrivatePolledActiveScheduler;
	iPrivatePolledActiveScheduler->Install(iPrivatePolledActiveScheduler);
}


void CPolledActiveScheduler::Schedule()
{
	iPrivatePolledActiveScheduler->Schedule();
}

CPolledActiveScheduler* CPolledActiveScheduler::Instance()
{
//	return (CPolledActiveScheduler*) CActiveScheduler::Current();
	return sPolledActiveScheduler;
}
