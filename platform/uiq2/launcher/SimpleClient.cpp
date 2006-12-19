
// needed for client interface
#include "../version.h"
#include "../ClientServer.h"
#include "SimpleClient.h"


// Connect to the  server - default number of message slots = 4
TInt RServSession::Connect()
{
	TInt r=CreateSession(KServerName,Version(),kDefaultMessageSlots);
	return(r); 
}

	
// Return the client side version number.
TVersion RServSession::Version(void) const
{
	return(TVersion(KPicoMajorVersionNumber,KPicoMinorVersionNumber,0));
}


TInt RServSession::SendReceive(TInt aFunction, TAny* aPtr) const
{
	return RSessionBase::SendReceive(aFunction, aPtr);
}


TInt RServSession::Send(TInt aFunction, TAny* aPtr) const
{
	return RSessionBase::Send(aFunction, aPtr);
}

