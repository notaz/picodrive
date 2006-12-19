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
	IMPORT_C virtual ~CBase();
	inline TAny* operator new(TUint aSize,TAny *aBase) {Mem::FillZ(aBase,aSize);return(aBase);}
	IMPORT_C TAny* operator new(TUint aSize);
	inline TAny* operator new(TUint aSize, TLeave) {return newL(aSize);}
	IMPORT_C TAny* operator new(TUint aSize,TUint anExtraSize);
protected:
	IMPORT_C CBase();
private:
	CBase(const CBase&);
	CBase& operator=(const CBase&);
	IMPORT_C static TAny* newL(TUint aSize);
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
	inline TBool IsActive() const {return(iActive);}
	inline TBool IsAdded() const  {return(iLink.iNext!=NULL);}
	inline TInt Priority() const  {return iLink.iPriority;}
protected:
	IMPORT_C CActive(TInt aPriority);
	IMPORT_C void SetActive();
// Pure virtual
	virtual void DoCancel() =0;
	virtual void RunL() =0;
	IMPORT_C virtual TInt RunError(TInt aError);
public:
	TRequestStatus iStatus;
private:
	TBool iActive;
	TPriQueLink iLink;
	friend class CActiveScheduler;
//	friend class CServer;
	friend class CPrivatePolledActiveScheduler; // added
	};

//
class CActiveScheduler : public CBase
	{
public:
	IMPORT_C CActiveScheduler();
	IMPORT_C ~CActiveScheduler();
	IMPORT_C static void Install(CActiveScheduler* aScheduler);
	IMPORT_C static CActiveScheduler* Current();
	IMPORT_C static void Add(CActive* anActive);
	IMPORT_C static void Start();
	IMPORT_C static void Stop();
	IMPORT_C static TBool RunIfReady(TInt& aError, TInt aMinimumPriority);
	IMPORT_C static CActiveScheduler* Replace(CActiveScheduler* aNewActiveScheduler);
	IMPORT_C virtual void WaitForAnyRequest();
	IMPORT_C virtual void Error(TInt anError) const;
private:
	void DoStart();
	IMPORT_C virtual void OnStarting();
	IMPORT_C virtual void OnStopping();
	IMPORT_C virtual void Reserved_1();
	IMPORT_C virtual void Reserved_2();
	friend class CPrivatePolledActiveScheduler; // added
protected:
	inline TInt Level() const;
private:
	TInt iLevel;
	TPriQue<CActive> iActiveQ;
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
			if (pR->IsActive() && pR->iStatus!=KRequestPending)
			{
				pR->iActive=EFalse;
				TRAPD(r,pR->RunL());
				break;
			}
		}
		else
			break;
	}
}


CPolledActiveScheduler::~CPolledActiveScheduler()
{
	delete iPrivatePolledActiveScheduler;
}

//static CPolledActiveScheduler* sPolledActiveScheduler = NULL;
CPolledActiveScheduler* CPolledActiveScheduler::NewL()
{
	//sPolledActiveScheduler = 
	CPolledActiveScheduler*	self = new(ELeave)CPolledActiveScheduler;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop();
	return self;
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

/*
CPolledActiveScheduler* CPolledActiveScheduler::Instance()
{
//	return (CPolledActiveScheduler*) CActiveScheduler::Current();
	return sPolledActiveScheduler;
}
*/
