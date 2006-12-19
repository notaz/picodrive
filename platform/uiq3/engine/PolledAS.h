/*******************************************************************
 *
 *	File:		PolledAS.h
 *
 *	Author:		Peter van Sebille (peter@yipton.net)
 *
 *	(c) Copyright 2001, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#ifndef __POLLED_AS_H
#define __POLLED_AS_H

class CPrivatePolledActiveScheduler;

class CPolledActiveScheduler : public CBase
{
public:
	~CPolledActiveScheduler();
	static CPolledActiveScheduler* NewL();
	static CPolledActiveScheduler* Instance();
	void Schedule();
protected:
	CPolledActiveScheduler(){};
	void ConstructL();
	CPrivatePolledActiveScheduler*	iPrivatePolledActiveScheduler;
};


#endif			/* __POLLED_AS_H */

