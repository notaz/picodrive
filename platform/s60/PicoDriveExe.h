#ifndef PicoDriveH
#define PicoDriveH
#include <e32base.h>
#include <eikappui.h>
#include <coecntrl.h>
#include <aknapp.h>
#include <akndoc.h>
#include <MdaAudioOutputStream.h>
#include <Mda\Common\Audio.h>

#include <aknappUI.h>
struct Target
{
  unsigned char *screen;
  TPoint point; // Screen to client point
  TRect view,update;
  TInt scanline_length;
  TInt screen_offset;
  TBool stretch_line;

};

enum TPicoMainMenu
{
	ELoadRom,
	ELoadState,
	ESaveState,
	ESetControls,
	ESetScreen,
	ESelectSound,
	ESelectCheat,
	EResetHw,
	EAboutPico,
	EExitPico,
	ELastMenuItem
};

enum TPicoSoundMenu
{
	EEnableZ80,
	EEnableYM2612,
	EEnableSN76496,
	ESoundVolume,
	ESoundRate,
	ELastSoundItem
};

enum TPicoCheatMenu
{
	EAddCheat,
	EClearCheats,
	ELastCheatItem
};

enum TPicoControllerMenu
{
	EControllerType,
	EConfigControls
};
enum TPicoMenus
{
	EPicoMainMenu,
	ESelectScrMenu,
	EAboutPicoMenu,
	ESelectSoundMenu,
	ESelectControlsMenu,
	ESelectCheatMenu
};

enum TPicoKeys
{
	EPicoUpKey,
	EPicoDownKey,
	EPicoLeftKey,
	EPicoRightKey,
	EPicoAKey,
	EPicoBKey,
	EPicoCKey,
	EPicoXKey,
	EPicoYKey,
	EPicoZKey,
	EPicoModeKey,
	EPicoStartKey,
	EPicoULKey,
	EPicoURKey,
	EPicoDRKey,
	EPicoDLKey,
	EPicoResetKey,
	EPicoPanLKey,
	EPicoPanRKey,
	EPicoNoKeys
};

class CPicoAddCheatDlg:public CEikDialog
{
public:
	CPicoAddCheatDlg(TDes8& aCheatCode):iCheatCode(aCheatCode){};
	~CPicoAddCheatDlg(){	iEikonEnv->EikAppUi()->RemoveFromStack(this);}
	TBool OkToExitL(TInt /*aButtonId*/)
	{
		static_cast<CEikEdwin*>(ControlOrNull(0x2000))->GetText(iUniCheatCode);
		iCheatCode.Copy(iUniCheatCode);
		return ETrue;
	}

	void PreLayoutDynInitL()
	{
	iEikonEnv->EikAppUi()->AddToStackL(this);
	}
private:
	TDes8& iCheatCode;
	TBuf<16> iUniCheatCode;
};

#ifdef S60V3
class CPicoDriveDoc:public CAknDocument
{
public:
	CPicoDriveDoc(CEikApplication& aApp);
	~CPicoDriveDoc();
	CEikAppUi* CreateAppUiL();
};

class CPicoDriveApp:public CAknApplication
{
public:
	CPicoDriveApp();
	~CPicoDriveApp();
	TUid AppDllUid()const;
	CApaDocument*		CreateDocumentL();

	/**
	* From @c CApaApplication. Opens the .ini file associated with the
	* application. By default, ini files are not supported by SERIES60 
    * applications. If you want to use an ini file, either override this
    * function to base call @c CEikApplication::OpenIniFileLC, or call it
    * directly.
    * @param aFs File server session to use. Not used.
    * @return Pointer to the dictionary store object representing the
    * application's .ini file.
    */
	CDictionaryStore* OpenIniFileLC(RFs& aFs) const;
};
#endif

class CQPicoDriveView:public CCoeControl,public MDirectScreenAccess
	{
public:
	CQPicoDriveView() {};
		~CQPicoDriveView();
		void Draw(const TRect& aRect) const;
		void ConstructL();
		void PutBitmap(CFbsBitmap* aBitmap,TPoint aPoint,TRect aRect);
		void Restart(RDirectScreenAccess::TTerminationReasons aReason);
		void AbortNow(RDirectScreenAccess::TTerminationReasons aReason);
		void DrawText(const TDesC& aText,TPoint aPoint,TBool aHighLight=EFalse,TRgb aTextColour = KRgbWhite);
		TInt DrawTextInRect(const TDesC& aText,TRect aRect,TInt aStartPos);
		void Clear();
		CDirectScreenAccess* iDsa;
		TBool iDrawingOn;
	    TBool iForeground;
	};


class CPicoDriveUi:public CAknAppUi,public MMdaAudioOutputStreamCallback
{
public:
	CPicoDriveUi();
	~CPicoDriveUi();
	void ConstructL();
	void StartAsynchUpdate();
protected:
	static TInt AsyncUpdateL(TAny* aAppUi);
	void StartEmulatorL();
	virtual TKeyResponse HandleKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType);
	void HandleForegroundEventL(TBool aForeground);
	// Menu drawers
	void PutMainMenu();
	void PutScreenSelect();
	void PutControllerSelect();
	void PutConfigKeys();
	void PutSoundSelect();
	void PutCheatSelect();
	void PutAbout(TBool iOnlyRedraw = EFalse);

	// Asynch screen update callback
	void UpdateScreen();

	TInt SelectFile(TFileName& aFileName);
	// Emulation functions
	int EmulateInit();
	void EmulateExit();
	int EmulateFrame();
	int InputFrame();
	int TargetInit();
	void CalulateLineStarts();

	static TInt IdleCallBackStop(TAny* aAppUi);
	static TInt StartEmulatorL(TAny* aAppUi);
	// Settings storage
	void SaveSettingsL();
	void InternalizeL(const CDictionaryStore& aStore);
	void ExternalizeL( CDictionaryStore& aStore);

	// Save state handling
	int saveLoadGame(int load, int sram);

	/**
	 * Calculates the palette table 0-4096
	 */
	void CalculatePaletteTable();
	CAsyncCallBack iIdleCallBack;

	// Variables
	TUint16 iPad1;
	TUint16 iPad2;	
	TFileName iRomName;
	char RomName[260];
	TInt iScanCodes[EPicoNoKeys];
	TInt iCurrentScan;
	CDesCArrayFlat* iKeyNames;
	CDesCArrayFlat* iRegNames;
	TBool iEmuRunning;
	CQPicoDriveView* iView;
	TInt iResourceFileId;
	CAsyncCallBack iStartUp;
	CFbsBitmap* iBackBuffer;
	TFileName iAppPath;
	TPoint iPutPoint;
	TRect  iPutRect; 
	TInt iSelection;
	TInt iSndSelection;
	TInt iCtrlSelection;
	TInt iCheatSelection;
	TInt iNoCheats;
	TBool iCheatEnter;
	CEikDialog* iCheatDlg;
	TInt iListOffset;
	TInt iScrMode;
	TInt iLastScrMode;
	TBool iRomLoaded;
	TBool iInterpolate;
	TBool iStretch;
	TBool iEnableSixButtons;
	TPicoMenus iPicoMenu;
	CAsyncCallBack iAsyncUpdate;

	// sound support
	CMdaAudioOutputStream* iSndStream;
	TMdaAudioDataSettings iAudioSettings;
	TBuf8<442*2*6> iMonoSound;
	TInt iCurrentSeg;
	TBool iEnableSound;
	TBool iSndRateChanged;
	TInt iSoundVolume; // 0-10 in 10% percent
	void MaoscOpenComplete(TInt aError);
	void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer);
	void MaoscPlayComplete(TInt aError);
	// Update the sound output rate
	TBool UpdatePSndRate();

	// Rom pointers
	unsigned char *RomData;
	unsigned int RomSize;
	
	TInt64 LastSecond;
	int FramesDone;
	int FramesPerSecond;
	TDisplayMode iDisplayMode;
	TInt iLastAboutPos;
	TBool iFirstStart;
	TInt iFrameSkip;
	TInt iFontHeight;	
	int (*myPicoScan)(unsigned int scan,unsigned short *pal);
	TBuf<1024> iTempString;
	// make save filename
	char saveFname[KMaxFileName];

};

#endif
