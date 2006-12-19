#include "PicoDriveAppS60.h"
#include <picodrives60.rsg>
#include <apgcli.h>
#include <eikdll.h>
EXPORT_C CApaApplication* NewApplication()
{
        return (new CPicoDrive);
}

CPicoDrive::CPicoDrive()
{
}


CPicoDrive::~CPicoDrive()
{
}


CApaDocument* CPicoDrive::CreateDocumentL()
	{
	return new (ELeave) CPicoDriveDoc(*this);
	}
TUid CPicoDrive::AppDllUid() const
	{
	return TUid::Uid(0x101F9B49);
	}


CPicoDriveDoc::CPicoDriveDoc(CEikApplication& aApp):CAknDocument(aApp)
{
}

CPicoDriveDoc::~CPicoDriveDoc()
	{
	}

CEikAppUi* CPicoDriveDoc::CreateAppUiL()
	{
	return new (ELeave) CPicoDriveUi;
	}


void CPicoDriveUi::HandleForegroundEventL(TBool aForeground)
{
	if(aForeground)
	{
		BringUpEmulatorL();	
	}
}

CPicoDriveUi::CPicoDriveUi()
{
}

CPicoDriveUi::~CPicoDriveUi()
	{
	
	 RemoveFromViewStack(*iView,iView);
	DeregisterViewAndRemoveStack(*iView);
	delete iView;
	if(iWatcher)
	{
		iThreadWatch.LogonCancel(iWatcher->iStatus);
		iWatcher->Cancel();
	}
	delete iWatcher;

	iThreadWatch.Close();
	}


void CPicoDriveUi::ConstructL()
	{
	BaseConstructL();
	iView = new(ELeave)CPicoView;
	iView->SetMopParent(this);
	iView->ConstructL();
	RegisterViewAndAddStackL(*iView);
	AddToViewStackL(*iView,iView);
	SetDefaultViewL(*iView);
	TBuf<128> startFile;
	startFile = iEikonEnv->EikAppUi()->Application()->AppFullName();
	TParse parser;
	parser.Set(startFile,NULL,NULL);
	
	startFile = parser.DriveAndPath();	
#ifndef __WINS__
	startFile.Append( _L("PicoDrive.EXE"));
#else
	startFile.Append( _L("PicoDrive.DLL"));
#endif
	CApaCommandLine* cmdLine=CApaCommandLine::NewLC(startFile);
	RApaLsSession lsSession;
	lsSession.Connect();
	CleanupClosePushL(lsSession);
	lsSession.StartApp(*cmdLine,iThreadId);
	CleanupStack::PopAndDestroy();//close lsSession
	CleanupStack::PopAndDestroy(cmdLine);
	User::After(500000);// Let the application start
	TApaTaskList taskList(iEikonEnv->WsSession());
	TApaTask myTask=taskList.FindApp(TUid::Uid(0x101F9B49));
	myTask.SendToBackground();
	TApaTask exeTask=taskList.FindByPos(0);
	iExeWgId=exeTask.WgId();
	if(iThreadWatch.Open(iThreadId)==KErrNone)
	{
		iWatcher = new (ELeave)CPicoWatcher;
		iWatcher->iAppUi=this;
		iThreadWatch.Logon(iWatcher->iStatus);	
	}
}


CPicoWatcher::CPicoWatcher():CActive(EPriorityStandard)
{
	CActiveScheduler::Add(this);
	iStatus=KRequestPending;
	SetActive();
}

CPicoWatcher::~CPicoWatcher()
{
}
void CPicoWatcher::DoCancel()
{
}

void CPicoWatcher::RunL()
{
	iAppUi->HandleCommandL(EEikCmdExit);
}

void CPicoDriveUi::BringUpEmulatorL()
{
	RThread thread;
	if(thread.Open(iThreadId)==KErrNone)
	{
		thread.Close();
		TApaTask apaTask(iEikonEnv->WsSession());
		apaTask.SetWgId(iExeWgId);
		apaTask.BringToForeground();
	}
	else
	{
		iExeWgId=-1;
	}
}



void CPicoDriveUi::HandleCommandL(TInt aCommand)
{
	
	switch(aCommand)
	{
	case EEikCmdExit:
		{
			RThread thread;
			if(thread.Open(iThreadId)==KErrNone)
			{
				thread.Terminate(0);
				thread.Close();
			}
			Exit();
		} 
		break;
	
	}
}

GLDEF_C  TInt E32Dll(TDllReason)
{
	return KErrNone;
}



