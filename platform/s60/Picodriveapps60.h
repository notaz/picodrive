#ifndef picodriveapps60h
#define picodriveapps60h

#include <aknapp.h>
#include <eikapp.h>
#include <e32base.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <coeview.h>
#include <eikappui.h>
class CPicoView:public MCoeView,public CCoeControl
{
public:
	CPicoView() {};
	~CPicoView(){};
	void ConstructL(){CreateWindowL();SetRect(iEikonEnv->EikAppUi()->ClientRect());ActivateL();SetBlank();};
	void ViewDeactivated(){MakeVisible(EFalse);};
	void ViewActivatedL(const TVwsViewId& /*aPrevViewId*/,TUid /*aCustomMessageId*/,const TDesC8& /*aCustomMessage*/)
	{
	MakeVisible(ETrue);
	}
	TVwsViewId ViewId() const
	{
		TVwsViewId viewId(TUid::Uid(0x101F9B49),TUid::Uid(0x101010));
		return viewId;
	}
};

class CPicoDrive:public CEikApplication
{
public:
	CPicoDrive();
	~CPicoDrive();
	CApaDocument* CreateDocumentL();
	TUid AppDllUid() const;
};


#include <AKNdoc.h>

class CPicoDriveDoc:public  CAknDocument
{
public:
	~CPicoDriveDoc();
	CEikAppUi* CreateAppUiL();
	void ConstructL();
	CPicoDriveDoc(CEikApplication& aApplicaiton);
};

#include <aknappui.h>
class CPicoDriveUi;
class CPicoWatcher:public CActive
{
public:
	CPicoWatcher();
	~CPicoWatcher();
	void DoCancel();
	void RunL();
	CPicoDriveUi* iAppUi;
};

class CPicoDriveUi:public CAknAppUi
{
public:
	CPicoDriveUi();
	~CPicoDriveUi();
	void ConstructL();
	void HandleCommandL(TInt aCommand);
	void HandleForegroundEventL(TBool aForeground);
	void BringUpEmulatorL();
private:
	CPicoView* iView;
	TThreadId iThreadId;
	TInt iExeWgId;
	RThread iThreadWatch;
	CPicoWatcher* iWatcher;
};
#endif