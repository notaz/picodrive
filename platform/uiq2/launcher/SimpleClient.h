#ifndef __SERVSESSION_H__
#define __SERVSESSION_H__

#include <e32base.h>


//**********************************
// RServSession
//**********************************

class RServSession : public RSessionBase
{
public:
	RServSession() {}
	TInt Connect();
	TVersion Version() const;
	TInt SendReceive(TInt aFunction,TAny* aPtr) const;
	TInt Send(TInt aFunction,TAny* aPtr) const;
};


#endif

