#include <stdlib.h>
#include <stdio.h>
#include <eikenv.h>
#include <e32keys.h>
#include <eikapp.h>
#include <zlib.h>
#ifdef S60V3
#include <picodrive.rsg>
#else
#include <picodriveS60.rsg>
#endif
#include <eikdoc.h>
#include <aknlistquerydialog.h>
#include <s32file.h>
#include <string.h>
#include <ctype.h>
#include <eikenv.h>
#include <sys\time.h>

#include "PicoDriveexe.h"
#include "pico.h"
#include "unzip.h"
#include "PicoInt.h"
#include "GGenie.h"

TInt KLineGap = 2;
static const char* KAboutText = 
"This emulator uses code from\n"
"these people/projects:\n"
"\n"
"Dave\n"
"Cyclone 68000 core,\n"
"Pico emulation library\n"
"Homepage:http://www.finalburn.com\n"
"E-mail: david(at)finalburn.com\n"
"\n"
"notaz\n"
"UIQ port,Cyclone 68000 hacks,some\n"
"additional coding (see changelog).\n"
"Homepage:http://notaz.atspace.com/\n"
"E-mail: notasas(at)gmail.com\n"
"\n"
"Reesy\n"
"DrZ80, the Z80 emulator\n"
"written in ARM assembly.\n"
"Homepage: http://reesy.gp32x.de/\n"
"E-mail:drsms_reesy(at)yahoo.co.uk\n"
"\n"
"Tatsuyuki Satoh, Jarek Burczynski,\n"
"MultiArcadeMachineEmulator\n"
"development\n"
"software implementation of\n"
"Yamaha FM sound generator\n"
"\n"
"MultiArcadeMachineEmulator(MAME)\n"
"development\n"
"Texas Instruments SN76489/SN76496\n"
"programmable tone/noise generator\n"
"Homepage: http://www.mame.net/\n"
"\n"
"Additional thanks\n"
"-----------------\n"
"* Peter van Sebille for ECompXL\n"
"  and his various open-source\n"
"  Symbian project to learn from.\n"
"* Mark and Jean-loup for zlib\n"
"  library.\n"
"* Reesy for also finding some\n"
"  Cyclone bugs.\n"
"* Charles MacDonald\n"
"  (http://cgfm2.emuviews.com/)\n"
"  for old but still very useful\n"
"  info about genesis hardware.\n"
"* Stúphane Dallongeville\n"
"  for creating Gens\n"
"  (http://www.gens.ws)\n"
"*The development team behind the\n"
" Symbian GCC Improvement Project\n"
" http://www.inf.u-szeged.hu\n"
" /symbian-gcc/) for their updated\n"
" compiler tools.\n"
"* Inder for the icons.\n";

// Picodrive prefrence uid
const TUid KPicoDrivePrefs={0x1234432E};

// Bittable corresponding to the bitvalues for the different control actions
TUint16 KBitValTable[EPicoNoKeys]={1,2,4,8 ,64,16,32,1024,512,256,2048,128,5,9,10,6,0,0,0};

extern"C" unsigned short *framebuff = 0;  // temporary buffer in sega native BGR format
const  int framebuffsize  = ((8+320)*(224+16))*2; // actual framebuffer size (in bytes+to support new rendering mode)

// Colour lookuptable from BGR to RGB
unsigned short gColorMapTab[4096];

// Scaling line table
TUint8 gColumnStepTable[320];
TUint8 gNarrowColumnStepTable[256];
unsigned short gLineTable[240];
TUint32 gLineOffsets[416];
TUint32 gFullOffset;
extern int PsndLen;

#ifndef S60V3
GLDEF_C  TInt E32Dll(TDllReason)
{
	return KErrNone;
}
#ifdef __WINS__
_LIT(KLitResourceFileName,	"z:\\system\\apps\\picodrives60\\PicoDriveS60.rsc");
#else
_LIT(KLitResourceFileName,	"PicoDriveS60.rsc");
#endif

#endif

#ifdef __WINS__
RHeap* gChunk;
#endif
struct Target Targ;

#ifdef S60V3
#include "S60V3Video.inl"
#else
#include "NormalVideo.inl"
#include "InterpolateVideo.inl"
#endif

TInt CPicoDriveUi::AsyncUpdateL(TAny* aAppUi)
{
	static_cast<CPicoDriveUi*>(aAppUi)->UpdateScreen();
	return 0;
}

void CPicoDriveUi::StartAsynchUpdate() 
{
	TCallBack callback(AsyncUpdateL,iEikonEnv->EikAppUi());
	iAsyncUpdate.Cancel();
	iAsyncUpdate.Set(callback);
	iAsyncUpdate.CallBack();
}


CPicoDriveUi::CPicoDriveUi():iIdleCallBack(CActive::EPriorityIdle),iStartUp(CActive::EPriorityIdle),
iAsyncUpdate(CActive::EPriorityStandard)
{
	iCurrentScan=-1;
	FramesPerSecond=60;
	PicoOpt = 7;
	iLastAboutPos = -1;
	iFrameSkip = -1;
	PsndRate = 8000;
	iInterpolate = ETrue;
	iSoundVolume = 6;
	gFullOffset = 0;
}

CPicoDriveUi::~CPicoDriveUi()
{
	delete iKeyNames;
	delete iRegNames;
	if(iView)
	{
		RemoveFromStack(iView);
		delete iView;
	}
	delete iBackBuffer;
	delete iSndStream;
#ifndef S60V3
	iCoeEnv->DeleteResourceFile(iResourceFileId);
#endif

#ifdef __WINS__
	if(gChunk != NULL)
	{
		gChunk->Close();
	}
#endif
	free(framebuff);
	framebuff = 0;
	CloseSTDLIB();
}

TKeyResponse CPicoDriveUi::HandleKeyEventL(const TKeyEvent& aKeyEvent,TEventCode aType)
{
	if(iCheatEnter)
	{
		return iCheatDlg->OfferKeyEventL(aKeyEvent,aType);
	}

	if(aType==EEventKey && aKeyEvent.iScanCode==EStdKeyBackspace)
	{
		if(iPicoMenu ==ESelectSoundMenu)
		{
			if(iSndRateChanged)
			{
				if(!UpdatePSndRate()) // Not compatible.. reset to 8000
				{
					PsndRate = 8000;
					UpdatePSndRate();											
				}
				
				if(iRomLoaded)
				{
					sound_rerate();
				}
			}

			iView->Clear();
			iPicoMenu = EPicoMainMenu;
			PutMainMenu();
			return EKeyWasConsumed;
		}
		else if (iPicoMenu == ESelectControlsMenu)
		{
			iView->Clear();
			iPicoMenu = EPicoMainMenu;
			PutMainMenu();
			return EKeyWasConsumed;
		}
		else if(iPicoMenu == ESelectScrMenu)
		{
			iScrMode = iLastScrMode;
			iView->Clear();
			iPicoMenu = EPicoMainMenu;
			PutMainMenu();
			return EKeyWasConsumed;
		}
		else if(iPicoMenu == ESelectCheatMenu)
		{
			if(iCheatSelection<2)
			{
				iView->Clear();
				iPicoMenu = EPicoMainMenu;
				PutMainMenu();
			}
			else
			{
				TInt index = (iListOffset+iCheatSelection)-2;
				TInt noFound = 0;
				for(TInt i = 0; i < 256;i++)
				{
					if (Liste_GG[i].code[0] != 0)
					{
						noFound++;
						if(noFound-1 == index)
						{
							Liste_GG[i].code[0] = 0;
							Liste_GG[i].active = 0;
							iNoCheats--;
							iListOffset = 0;
							iCheatSelection = 0;
							break;
						}
						
					}
				}

				iView->Clear();
				PutCheatSelect();
			}

			return EKeyWasConsumed;
		}

		if(!iEmuRunning && (iPicoMenu!=EPicoMainMenu ||iCurrentScan!=-1) )
			return EKeyWasNotConsumed;
		
		if(iRomLoaded )
		{
			iEmuRunning=!iEmuRunning;
			
			if(iEmuRunning && iRomLoaded)
			{
				iView->Clear();
				TCallBack callback(StartEmulatorL,this);
				iStartUp.Set(callback);
				iStartUp.CallBack();
			}
		}
		return EKeyWasConsumed;
	}

	if(!iEmuRunning)
	{
		if(iCurrentScan==-1)
		{
			if(aType==EEventKey)
			{
				
				switch(aKeyEvent.iScanCode) // first determine bit value to change
				{
				case EStdKeyUpArrow:
					switch(iPicoMenu)
					{
					case ESelectControlsMenu:
						{
							iCtrlSelection=!iCtrlSelection;
							PutControllerSelect();
						}break;
					case EPicoMainMenu:
						if(iSelection>0)
							iSelection--;
						else iSelection=EExitPico;
						PutMainMenu();
						break;
					case ESelectScrMenu:
						if(iScrMode>0)
							iScrMode--;
						else iScrMode=10;
						PutScreenSelect();
						break;
					case ESelectCheatMenu:
							if(iCheatSelection>0)
								iCheatSelection--;
							else
								iCheatSelection = ELastCheatItem-1;						
							PutCheatSelect();
						break;
					case EAboutPicoMenu:
						{
							iView->Clear();
							PutAbout();
						}break;
					case ESelectSoundMenu:
						{
							if(iSndSelection>0)
								iSndSelection--;
							else
								iSndSelection = ELastSoundItem-1;						
							PutSoundSelect();
						}break;
					}					
				break;
					case EStdKeyDownArrow:
						switch(iPicoMenu)
						{
						case ESelectControlsMenu:
								{
									iCtrlSelection=!iCtrlSelection;
									PutControllerSelect();
								}break;
						case EPicoMainMenu:
							iSelection++;
							if(iSelection==ELastMenuItem)
								iSelection=0;
							PutMainMenu();
							break;
						case ESelectScrMenu:
							iScrMode++;
							if(iScrMode==11)
								iScrMode=0;
							PutScreenSelect();
							break;
						case ESelectCheatMenu:
							if(iCheatSelection<(ELastCheatItem+iNoCheats-1) && iCheatSelection<6)
								iCheatSelection++;
							else if(iCheatSelection == ELastCheatItem+4 && iListOffset<iNoCheats-5)
							{
								iListOffset++;
							}
							else
							{
								iListOffset = 0;
								iCheatSelection = 0;						
							}
							PutCheatSelect();
						break;
							case EAboutPicoMenu:
							{
								iView->Clear();
								PutAbout();
							}break;
						case ESelectSoundMenu:
							{
								iSndSelection++;
								if(iSndSelection==ELastSoundItem)
									iSndSelection=0;
								PutSoundSelect();
							}break;
						}
						break;
						case EStdKeyDevice0:
						case EStdKeyDevice3:
							{
								switch(iPicoMenu)
								{
								case ESelectControlsMenu:
								{
									if(iCtrlSelection == EConfigControls)
									{
										iPicoMenu = EPicoMainMenu;
										iView->Clear();
										iCurrentScan=0;
										PutConfigKeys();
									}
									else
									{
										iEnableSixButtons = !iEnableSixButtons;
										PicoOpt=PicoOpt^ 32;									
										
										PutControllerSelect();
									}
								}break;
								case ESelectCheatMenu:
									{
										switch(iCheatSelection)
										{
										case EAddCheat:
											{
												TBuf8<16> cheatCode;
												iCheatEnter = ETrue;
												iCheatDlg = new (ELeave) CPicoAddCheatDlg(cheatCode);
												iCheatDlg->SetMopParent(iEikonEnv->EikAppUi());		   
												TInt result = iCheatDlg->ExecuteLD(R_PICO_ADD_CHEAT);

												if(result == EEikBidOk)
												{
													for(TInt i = 0; i < 256;i++)
													{
														if (Liste_GG[i].code[0] == 0)
														{														
															if(check_code((const char*) cheatCode.PtrZ(),i))
															{
																decode( Liste_GG[i].code, (patch *) (&(Liste_GG[i].addr)));
																
																if ((Liste_GG[i].restore == 0xFFFFFFFF) && (Liste_GG[i].addr < Pico.romsize) && (iRomLoaded))
																{
																	Liste_GG[i].restore = (unsigned int) (Pico.rom[Liste_GG[i].addr] & 0xFF);
																	Liste_GG[i].restore += (unsigned int) ((Pico.rom[Liste_GG[i].addr + 1] & 0xFF) << 8);
																}
																iNoCheats++;
																Liste_GG[i].active = 1;
															}
															break; // Found position free
														}
													}
												}
												iCheatDlg = NULL;
												iCheatEnter = EFalse;
											}break;
										case EClearCheats:
											{
												iNoCheats = 0;
												for(TInt i = 0; i < 256;i++)
												{
													Liste_GG[i].code[0] = 0;
													Liste_GG[i].active = 0;
												}
											}
											break;
										default:
											{
												TInt index = (iListOffset+iCheatSelection)-2;
												TInt noFound = 0;
												for(TInt i = 0; i < 256;i++)
													{
														if (Liste_GG[i].code[0] != 0)
														{
															noFound++;
															if(noFound-1 == index)
															{
																Liste_GG[i].active=!Liste_GG[i].active;
																break;
															}

														}
												}
											}
											break;
									
										}

										PutCheatSelect();
									}
									break;
								case EAboutPicoMenu:
									{
										iView->Clear();
										PutAbout();
									}break;
								case ESelectScrMenu:
									{

										switch(iScrMode)
										{
										case 5:
											iInterpolate = !iInterpolate;
											iView->Clear();
											PutScreenSelect();
											break;
										case 6:
											iFrameSkip++;
											if(iFrameSkip == 11)
											{
												iFrameSkip = -1;
											}
											iView->Clear();
											PutScreenSelect();
											break;
										case 7:
											PicoOpt = PicoOpt^0x40;
											iView->Clear();
											PutScreenSelect();
										break;
										case 8:
											PicoOpt = PicoOpt^0x80;
											iView->Clear();
											PutScreenSelect();
										break;
										case 9:
											PicoOpt = PicoOpt^0x10;
											iView->Clear();
											PutScreenSelect();
											break;
										case 10:
											switch(PicoRegionOverride)
											{
											case 0:
											default:
												PicoRegionOverride = 1;
												break;
											case 1:
												PicoRegionOverride = 2;
												break;
											case 2:
												PicoRegionOverride = 4;
												break;
											case 4:
												PicoRegionOverride = 8;
												break;
											case 8:
												PicoRegionOverride = 0;
												break;
											}

											iView->Clear();
											PutScreenSelect();
											break;
										default:
											{
											TBitmapUtil util(iBackBuffer);
											util.Begin(TPoint(0,0));
											TSize sz=iBackBuffer->SizeInPixels();
											TInt dataSize=sz.iWidth*sz.iHeight*2;
											TPtr8 ptr(reinterpret_cast<unsigned char*>(iBackBuffer->DataAddress()),dataSize,dataSize);
											ptr.Fill(0);
											util.End();
											iPicoMenu=EPicoMainMenu;
											iView->Clear();
											PutMainMenu();
											TargetInit(); 
											SaveSettingsL();
											}break;
										}									
									}break;
								case ESelectSoundMenu:
									{
										switch(iSndSelection)
										{
										case EEnableZ80:
											PicoOpt=PicoOpt^4;
											break;
										case EEnableYM2612:
											PicoOpt=PicoOpt^1;
											break;
										case EEnableSN76496:
											PicoOpt=PicoOpt^2;
											break;
										case ESoundVolume:
											{										
												iSoundVolume++;
												if(iSoundVolume==11)
												{
													iSoundVolume=0;
													iEnableSound=EFalse;
												}
												else
												{
													iEnableSound=ETrue;
													iSndStream->SetVolume((iSndStream->MaxVolume()*iSoundVolume)/10);
												}

												if(!iEnableSound)
												{
													iSndStream->Stop();
												}
											}break;
										case ESoundRate:
											{
												iSndRateChanged = ETrue;

												switch(PsndRate)
												{
												case 8000:
													PsndRate = 11025;
													break;
												case 11025:
													PsndRate = 16000;
													break;
												case 16000:
													PsndRate = 22050;
													break;
												case 22050:
													PsndRate = 8000;
													break;
												}											
											}break;
										}	
										PutSoundSelect();
									}break;
								case EPicoMainMenu:
									{
										switch(iSelection)
										{
										case EResetHw:
											{
												if(!PicoReset(0))
												{
													iEmuRunning=ETrue;
													TCallBack callback(StartEmulatorL,this);
													iStartUp.Set(callback);
													iStartUp.CallBack();
												}
											}
											break;
										case ELoadState:
											{
												if(iRomLoaded)
												{													
													saveLoadGame(1,0);													
													iEmuRunning=ETrue;
													iView->Clear();
													TCallBack callback(StartEmulatorL,this);
													iStartUp.Set(callback);
													iStartUp.CallBack();										
												}
											}
											break;
										case ESaveState:
											{												
												if(iRomLoaded)
												{													
													saveLoadGame(0,0);
													iEmuRunning=ETrue;
													iView->Clear();
													TCallBack callback(StartEmulatorL,this);
													iStartUp.Set(callback);
													iStartUp.CallBack();
													
												}
											}
											break;
										case ELoadRom:
											{
												TParsePtr parse(iRomName);
												iRomName=parse.DriveAndPath();
												if(SelectFile(iRomName))
												{
													SaveSettingsL();
													EmulateExit();
													TPtr8 ptr((unsigned char*)RomName,256);
													ptr.Fill(0,256);
													ptr.Copy(iRomName);
													User::CompressAllHeaps();
													if(EmulateInit()==0)
													{
														if(Pico.m.pal)
														{
															FramesPerSecond=50;
														}
														else 
														{
															FramesPerSecond=60;
														}
														
														SetKeyBlockMode(ENoKeyBlock);
														iEmuRunning=ETrue;
														iRomLoaded=ETrue;
														iView->Clear();
														TCallBack callback(StartEmulatorL,this);
														iStartUp.Set(callback);
														iStartUp.CallBack();
													}
													else
													{
														iRomName=KNullDesC();
														iView->Clear();
														PutMainMenu();
														iRomLoaded=EFalse;
													}
												}
												else
												{
													iView->Clear();
													PutMainMenu();
												}
											}
											break;//load rom
										case ESetControls:
											iPicoMenu = ESelectControlsMenu;
											PutControllerSelect();										
											break;
										case ESetScreen:
											iView->Clear();
											iLastScrMode = iScrMode; // In case of cancel
											PutScreenSelect();
											iPicoMenu=ESelectScrMenu;
											break;
										case ESelectSound:
											{			
												iSndRateChanged = EFalse;
												iPicoMenu=ESelectSoundMenu;
												PutSoundSelect();
											}break;
										case ESelectCheat:
											{
											iPicoMenu=ESelectCheatMenu;
											iListOffset = 0;
											iNoCheats = 0;
											for(TInt i = 0; i < 256;i++)
											{
												if (Liste_GG[i].code[0] != 0)
												{
													iNoCheats++;
													if ((Liste_GG[i].restore != 0xFFFFFFFF) && (Liste_GG[i].addr < Pico.romsize) && (iRomLoaded))
													{
														Pico.rom[Liste_GG[i].addr] = (unsigned char)(Liste_GG[i].restore & 0xFF);
														Pico.rom[Liste_GG[i].addr + 1] = (unsigned char)((Liste_GG[i].restore & 0xFF00) >> 8);
													}
													
												}
											}

											PutCheatSelect();
											}
										break;
										case EAboutPico:
											{
												iView->Clear();
												PutAbout();
												iPicoMenu=EAboutPicoMenu;
											}break;
										case EExitPico:
											{
												SaveSettingsL();
												EmulateExit();
												Exit();
											}
											break;
							}
								}
						}break;
					}
					break;
				}
			}
		}
		else
		{
			if(aType == EEventKeyDown)
			{
				if(aKeyEvent.iScanCode != EStdKeyBackspace)
				{
					iScanCodes[iCurrentScan]=aKeyEvent.iScanCode;
				}
				else
				{
					iScanCodes[iCurrentScan] = KErrNotFound;
				}

				iCurrentScan++;
				if(iCurrentScan==iKeyNames->Count())
				{
					SaveSettingsL();
					iCurrentScan=-1;
					iView->DrawText(_L("Done!"),TPoint(0,iFontHeight*11));
					
					User::After(1000000);
					iView->Clear();
					PutMainMenu();
				}
				else
				{
					iView->Clear();
					PutConfigKeys();
				}
			}
		}
	}
	else
	{
		if((aType == EEventKeyUp || aType == EEventKeyDown))
		{						
			TUint16 bitVal=0;
			for(TInt loop=0;loop<16;loop++)
			{
				if(aKeyEvent.iScanCode==iScanCodes[loop])
				{
					bitVal=KBitValTable[loop];
					break;
				}
			}
			
			if(aType == EEventKeyUp)
			{
				iPad1=iPad1&(65535-bitVal); // remove bit
			}
			else 
			{
				iPad1=(iPad1|bitVal); // set bit
			}			
		}

		if(aType == EEventKey)
		{
			if(aKeyEvent.iScanCode == iScanCodes[EPicoResetKey])
			{
				PicoReset(0);
			}

			if(aKeyEvent.iScanCode == iScanCodes[EPicoPanLKey] && gFullOffset>0)
			{
				gFullOffset-=24;
			}
			else if(aKeyEvent.iScanCode == iScanCodes[EPicoPanRKey]&& gFullOffset<144)
			{
				gFullOffset+=24;
			}
		}
	}
	
	return EKeyWasConsumed;
}

TInt CPicoDriveUi::SelectFile(TFileName& aFileName)
{
	TFileName filename=aFileName;
	TInt selectedIndex=-1;
	RArray<TEntry> romList;
	TDriveList driveList;
	TBool refresh=EFalse;
	TInt lastLength=-1;
	do
	{
		CAknListQueryDialog* dlg = new (ELeave) CAknListQueryDialog(&selectedIndex);
		CDesCArrayFlat* list=new (ELeave) CDesCArrayFlat(5);
		refresh=EFalse;
		if(filename.Length()==0)
		{
			
			iEikonEnv->FsSession().DriveList(driveList);
			for(TInt drive=0;drive<driveList.Length();drive++)
			{
				if(driveList[drive]!=0)
				{
					TBuf<16> form;
					form.Format(_L("%c:\\"),drive+65);
					list->AppendL(form);
				}
			}	
		}
		else
		{
			CDir* romDir = NULL;;
			romList.Reset();
			iEikonEnv->FsSession().GetDir(filename,KEntryAttMatchMask,0,romDir);
			
			if(romDir!=NULL)
				list->AppendL(_L(".. <DIR>"));
			
			if(romDir!=NULL && romDir->Count()>0)
			{
				for(TInt loop=0;loop<romDir->Count();loop++)
				{
					const TEntry& entry=(*romDir)[loop];
					TFileName name=entry.iName;
					if(entry.IsDir())
					{
						name.Append(_L(" <DIR>"));
						list->AppendL(name);
						romList.Append(entry);
					}
					else
					{
						TParsePtr parse(name);
						
						if(parse.Ext().CompareF(_L(".bin"))==KErrNone ||
							parse.Ext().CompareF(_L(".smd"))==KErrNone ||
							parse.Ext().CompareF(_L(".zip"))==KErrNone)
						{
							romList.Append(entry);
							list->AppendL(name);
						}
					}
					
				}
				delete romDir;
				romDir=NULL;
			}
			else // no files found.. or path not found.. return to
			{
				if(filename.Length()>3) // more than a c:\ specified
				{
					
					refresh=ETrue;
					filename=KNullDesC();
				}
				else return EFalse;
			}
			
		}
		if(list->Count()>0)
		{
			dlg->PrepareLC(R_PICO_FILE_SELECT_DIALOG);
			dlg->SetItemTextArray(list);
			if(dlg->RunLD())
			{
				if(filename.Length()==0)
				{
					TFileName driveLetter;
					TUint8 driveL;
					TInt countedDrives=0;
					for(TInt drive=0;drive<driveList.Length();drive++)
					{
						if(driveList[drive]!=0 && countedDrives==selectedIndex)
						{
							driveLetter.Format(_L("%c:\\"),drive+65);
							driveL=drive+65;
							break;
						}
						else if(driveList[drive]!=0) countedDrives++;
						
					}
					filename=driveLetter;
					refresh=ETrue;					
				}
				else
				{
					if(selectedIndex>0)
					{
						selectedIndex--;
						const TEntry& entry=romList[selectedIndex];
						TFileName name =entry.iName;
						if(entry.IsDir())
						{
							lastLength=aFileName.Length();
							filename.Append(name);
							filename.Append(_L("\\"));
							
							refresh=ETrue;
							
						}
						else
						{
							filename.Append(name);
							aFileName=filename;
							romList.Close();
							return ETrue;
						}
					}
					else
					{
						refresh=ETrue;
						TInt pos=filename.Left(filename.Length()-1).LocateReverse('\\');
						if(pos!=KErrNotFound)
							filename=filename.Left(pos+1);// keep 
						else filename=KNullDesC();
						// and changefilename..
						
						//return 2;// go up one.
					}
				}
			}
			else
			{
				romList.Close();
				return EFalse;
			}
		}
	}while(refresh);
	romList.Close();
	return EFalse;
}

void CPicoDriveUi::SaveSettingsL()
{
#ifdef S60V3
	CDictionaryStore* prefs = Application()->OpenIniFileLC(iEikonEnv->FsSession());
#else
	CDictionaryFileStore* prefs=CDictionaryFileStore::OpenLC(iEikonEnv->FsSession(),iAppPath,TUid::Uid(0));
#endif
	ExternalizeL(*prefs);
	prefs->CommitL();
	CleanupStack::PopAndDestroy();//close prefs
}


void CPicoDriveUi::ExternalizeL(CDictionaryStore& aStore) 
{	
	RDictionaryWriteStream writeStream;
	writeStream.AssignLC(aStore, KPicoDrivePrefs);
	
	TInt loop=0;
	for(loop=0;loop<EPicoNoKeys;loop++)
	{
		writeStream.WriteInt32L(iScanCodes[loop]);
	}
	
	writeStream.WriteInt32L(iScrMode);
	writeStream.WriteInt32L(PicoOpt);
	writeStream.WriteInt32L(iSoundVolume);
	writeStream.WriteInt32L(iFrameSkip);
	writeStream.WriteInt32L(PsndRate);
	writeStream.WriteInt32L(iInterpolate);
	writeStream.WriteInt32L(iEnableSixButtons);
	writeStream.WriteInt32L(PicoRegionOverride);
	TParsePtr parser(iRomName);
	writeStream.WriteInt32L(parser.DriveAndPath().Length());
	writeStream.WriteL(parser.DriveAndPath(),parser.DriveAndPath().Length());
	writeStream.CommitL();
	
	CleanupStack::PopAndDestroy();//writeStream
}

void CPicoDriveUi::InternalizeL(const CDictionaryStore& aStore)
{
	if (!aStore.IsPresentL(KPicoDrivePrefs))
	{
		iFirstStart = ETrue;
		return;
	}
	RDictionaryReadStream	readStream;
	readStream.OpenLC(aStore, KPicoDrivePrefs);
	TInt loop=0;
	for(loop=0;loop<EPicoNoKeys;loop++)
	{
		iScanCodes[loop] = readStream.ReadInt32L();
	}
	
	iScrMode=readStream.ReadInt32L();
	PicoOpt=readStream.ReadInt32L();
	iEnableSound = PicoOpt&3;
	iSoundVolume=readStream.ReadInt32L();
	iFrameSkip = readStream.ReadInt32L();
	PsndRate = readStream.ReadInt32L();
	iInterpolate = readStream.ReadInt32L();
	iEnableSixButtons = readStream.ReadInt32L();
	PicoRegionOverride = readStream.ReadInt32L();

	TInt len=0;
	TRAPD(err,len=readStream.ReadInt32L())
		if(err==KErrNone)
		{
			readStream.ReadL(iRomName,len);
		}
		
		CleanupStack::PopAndDestroy();//readStream
}

void CPicoDriveUi::ConstructL()
{
#ifdef S60V3
	CAknAppUi::BaseConstructL(CAknAppUi::EAknEnableSkin);
#else
	BaseConstructL(ENoAppResourceFile);	
#endif
	framebuff =  (unsigned short *) malloc(framebuffsize);
	memset(framebuff,0,framebuffsize);

	iFontHeight = iEikonEnv->NormalFont()->HeightInPixels()+KLineGap;
	iScanCodes[0]= EStdKeyUpArrow;			
	iScanCodes[1]= EStdKeyDownArrow;
	iScanCodes[2]=EStdKeyLeftArrow;
	iScanCodes[3]= EStdKeyRightArrow;
	iScanCodes[6]=EStdKeyDevice0;
	iScanCodes[4]= EStdKeyDevice1;
	iScanCodes[5]=EStdKeyDevice3;
	iScanCodes[7]=0;//x
	iScanCodes[8]=0; // y
	iScanCodes[9]=0;//z
	iScanCodes[10]=0;// mode
	iScanCodes[11]= '0'; // start
#ifdef __WINS__
	gChunk = UserHeap::ChunkHeap(&_L("ROMHEAP"),512000,16384000);
#endif

#ifndef S60V3
	TFileName name;
	iEikonEnv->RootWin().SetName(_L("PicoDrive"));
#ifndef __WINS__
	RProcess process;
	process.Rename(_L("PicoDrive"));
	TFileName fname  =process.FileName();
	TParsePtr parser(fname);
	name.Append(parser.DriveAndPath());
#endif
	name.Append(KLitResourceFileName());
	iAppPath=_L("C:");
	iAppPath.Append(TParsePtr(name).Path());
	iAppPath.Append(_L("PicoDriveS60.ini"));
	iResourceFileId = iCoeEnv->AddResourceFileL(name); // eb205: needs to hunt around drives
#endif // S60V3
	iKeyNames =iEikonEnv->ReadDesCArrayResourceL(R_PICODRIVE_KEYS);
	iRegNames = iEikonEnv->ReadDesCArrayResourceL(R_PICODRIVE_REGIONS);

	iEikonEnv->FsSession().MkDirAll(TParsePtr(iAppPath).DriveAndPath());
#ifdef S60V3
	CDictionaryStore* prefs = Application()->OpenIniFileLC(iEikonEnv->FsSession());
	InternalizeL(*prefs);
	CleanupStack::PopAndDestroy();//close prefs
#else
	TRAPD(err,{CDictionaryFileStore* prefs=CDictionaryFileStore::OpenLC(iEikonEnv->FsSession(),iAppPath,TUid::Uid(0));
	InternalizeL(*prefs);
	CleanupStack::PopAndDestroy();//close prefs
	});
#endif
	
	if(iFirstStart)
	{
		iPicoMenu = EAboutPicoMenu;
	}
	
	iView=new (ELeave)CQPicoDriveView;
	iView->ConstructL();
	AddToStackL(iView);
	iDisplayMode =iEikonEnv->ScreenDevice()->DisplayMode();
	if(iDisplayMode != EColor64K && iDisplayMode != EColor4K)
	{
		iDisplayMode=EColor64K;; // Also tried to switch to by the view.
	}
	CalculatePaletteTable();
	iBackBuffer= new (ELeave)CFbsBitmap;
	iBackBuffer->Create(iEikonEnv->ScreenDevice()->SizeInPixels(),iDisplayMode);
	TBitmapUtil util(iBackBuffer);
	util.Begin(TPoint(0,0));
	TSize sz=iBackBuffer->SizeInPixels();
	Targ.view = TRect(TPoint(0,0),sz);
	TInt dataSize=sz.iWidth*sz.iHeight*2;
	Targ.scanline_length = sz.iWidth*2;
	Targ.screen_offset = Targ.scanline_length*(sz.iHeight-1);
	TPtr8 ptr(reinterpret_cast<unsigned char*>(iBackBuffer->DataAddress()),dataSize,dataSize);
	ptr.Fill(0);
	util.End();
	SetKeyBlockMode(ENoKeyBlock);
	iSelection=0;
	
	iSndStream = CMdaAudioOutputStream::NewL(*this);
	iAudioSettings.Query();
	iAudioSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
	iAudioSettings.iChannels = TMdaAudioDataSettings::EChannelsMono;
	iAudioSettings.iFlags = 0;
	iAudioSettings.iVolume = iAudioSettings.iMaxVolume/2;
	iSndStream->Open(&iAudioSettings);
	CActiveScheduler::Start(); // wait for open
}

void CPicoDriveUi::PutAbout(TBool iOnlyRedraw)
{
	TPtrC8 charPtr((unsigned char*)KAboutText,strlen(KAboutText));
	HBufC* credits = HBufC::NewLC(charPtr.Length());
	credits->Des().Copy(charPtr);
	
	if(iLastAboutPos>=credits->Length())
	{
		iLastAboutPos = -1;
		iView->Clear();
		iPicoMenu = EPicoMainMenu;
		PutMainMenu();
	}
	else
	{
		if(iLastAboutPos == -1)
			iLastAboutPos = 0;
		iView->DrawText(_L("PicoDrive S60 Credits"),TPoint(0,0));
		if(iOnlyRedraw && iLastAboutPos == 0)
			iView->DrawTextInRect(*credits,TRect(0,iFontHeight*2,Targ.view.iBr.iX,Targ.view.iBr.iY),iLastAboutPos);
		else
			iLastAboutPos = iView->DrawTextInRect(*credits,TRect(0,iFontHeight*2,Targ.view.iBr.iX,Targ.view.iBr.iY),iLastAboutPos);
	}
	CleanupStack::PopAndDestroy(credits);
	
}
void CPicoDriveUi::PutMainMenu()
{
	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("by Dave et Co"),TPoint(0,iFontHeight*1));
	
	iView->DrawText(_L("Load ROM"),TPoint(0,iFontHeight*3),iSelection==0);
	iView->DrawText(_L("Load state"),TPoint(0,iFontHeight*4),iSelection==1);
	iView->DrawText(_L("Save state"),TPoint(0,iFontHeight*5),iSelection==2);
	iView->DrawText(_L("Configure controls"),TPoint(0,iFontHeight*6),iSelection==3);
	iView->DrawText(_L("Configure screen"),TPoint(0,iFontHeight*7),iSelection==4);
	iView->DrawText(_L("Configure sound"),TPoint(0,iFontHeight*8),iSelection==5);
	iView->DrawText(_L("Game Genie/Cheats"),TPoint(0,iFontHeight*9),iSelection==6);

	iView->DrawText(_L("Reset"),TPoint(0,iFontHeight*10),iSelection==7);
	
	iView->DrawText(_L("Credits"),TPoint(0,iFontHeight*11),iSelection==8);
	iView->DrawText(_L("Exit"),TPoint(0,iFontHeight*12),iSelection==9);
	if(iRomName.Length()>0)
	{
		iView->DrawText(TParsePtr(iRomName).Name(),TPoint(0,iFontHeight*13));
	}
	else
	{
		iView->DrawText(_L("No rom loaded"),TPoint(0,iFontHeight*13));
	}
	
}

void CPicoDriveUi::PutScreenSelect()
{
	TInt regionIndex = 0;

	switch(PicoRegionOverride)
	{
	default:
		regionIndex = 0;
		break;
	case 1:
		regionIndex = 1;
		break;
	case 2:
		regionIndex = 2;
		break;
	case 4:
		regionIndex = 3;
		break;
	case 8:
		regionIndex = 4;
		break;
	}

	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("Screen options"),TPoint(0,iFontHeight));
	iView->DrawText(_L("Portrait"),TPoint(0,iFontHeight*3),iScrMode==0);
	iView->DrawText(_L("Landscape Left"),TPoint(0,iFontHeight*4),iScrMode==1);
	iView->DrawText(_L("Landscape Right"),TPoint(0,iFontHeight*5),iScrMode==2);
	iView->DrawText(_L("Portrait stretched"),TPoint(0,iFontHeight*6),iScrMode==3);
	iView->DrawText(_L("Portrait full"),TPoint(0,iFontHeight*7),iScrMode==4);
	if(iInterpolate)
	{
	iView->DrawText(_L("Interpolate on"),TPoint(0,iFontHeight*8),iScrMode==5);
	}
	else
	{
	iView->DrawText(_L("Interpolate off"),TPoint(0,iFontHeight*8),iScrMode==5);
	}

	if(iFrameSkip == -1)
	{
		iView->DrawText(_L("Frameskip auto"),TPoint(0,iFontHeight*9),iScrMode==6);
	}
	else
	{	
		TBuf<64> skip;
		skip.Format(_L("Frameskip %d"),iFrameSkip);
		iView->DrawText(skip,TPoint(0,iFontHeight*9),iScrMode==6);
	}

	if(PicoOpt & 0x40)
	{
	iView->DrawText(_L("Accurate timing on"),TPoint(0,iFontHeight*10),iScrMode==7);
	}
	else
	{
	iView->DrawText(_L("Accurate timing off"),TPoint(0,iFontHeight*10),iScrMode==7);
	}

	if(PicoOpt & 0x80)
	{
	iView->DrawText(_L("Accurate sprites on"),TPoint(0,iFontHeight*11),iScrMode==8);
	}
	else
	{
	iView->DrawText(_L("Accurate sprites off"),TPoint(0,iFontHeight*11),iScrMode==8);
	}

	if(PicoOpt & 0x10)
	{
	iView->DrawText(_L("Alt. renderer on"),TPoint(0,iFontHeight*12),iScrMode==9);
	}
	else
	{
	iView->DrawText(_L("Alt. renderer off"),TPoint(0,iFontHeight*12),iScrMode==9);
	}

	iView->DrawText(iRegNames->MdcaPoint(regionIndex),TPoint(0,iFontHeight*13),iScrMode==10);
	
}

void CPicoDriveUi::PutSoundSelect()
{
	iView->Clear();
	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("Sound options"),TPoint(0,iFontHeight));
	if (PicoOpt&4)
		iView->DrawText(_L("Z80 enabled"),TPoint(0,iFontHeight*3),iSndSelection==EEnableZ80);
	else
		iView->DrawText(_L("Z80 disabled"),TPoint(0,iFontHeight*3),iSndSelection==EEnableZ80);
	if (PicoOpt&1)
		iView->DrawText(_L("YM2612 enabled"),TPoint(0,iFontHeight*4),iSndSelection==EEnableYM2612);
	else
		iView->DrawText(_L("YM2612 disabled"),TPoint(0,iFontHeight*4),iSndSelection==EEnableYM2612);
	
	if (PicoOpt&2)
		iView->DrawText(_L("SN76496 enabled"),TPoint(0,iFontHeight*5),iSndSelection==EEnableSN76496);
	else
		iView->DrawText(_L("SN76496 disabled"),TPoint(0,iFontHeight*5),iSndSelection==EEnableSN76496);
	
	
	TBuf<32> vol;
	vol.Format(_L("Volume %d"),iSoundVolume*10);
	iView->DrawText(vol,TPoint(0,iFontHeight*6),iSndSelection==ESoundVolume);

	vol.Format(_L("Sample rate %dKhz"),PsndRate/1000);
	iView->DrawText(vol,TPoint(0,iFontHeight*7),iSndSelection==ESoundRate);
}


void CPicoDriveUi::PutCheatSelect()
{
	iView->Clear();
	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("Cheat options"),TPoint(0,iFontHeight));

	iView->DrawText(_L("Add cheat"),TPoint(0,iFontHeight*3),iCheatSelection==EAddCheat);
	iView->DrawText(_L("Clear cheats"),TPoint(0,iFontHeight*4),iCheatSelection==EClearCheats);
	TInt noCheats = 0;
	TBuf<17>cheatCode;

	for(TInt i = iListOffset; (i < 256)&&(noCheats<5); i++)
			{
				if (Liste_GG[i].code[0] != 0)
				{

					TRgb textColour = Liste_GG[i].active?KRgbGreen:KRgbDarkGreen;
					if(iCheatSelection == 2+(i-iListOffset))
					{
						textColour = Liste_GG[i].active?KRgbRed:KRgbDarkRed;
					}

					TPtrC8 ptr((const unsigned char*)Liste_GG[i].code,strlen(Liste_GG[i].code));
					cheatCode.Copy(ptr);										

					iView->DrawText(cheatCode,TPoint(0,iFontHeight*(5+noCheats)),EFalse,textColour);
				
					noCheats++;
				}
			}

	iView->DrawText(_L("Supports GG & Patch"),TPoint(0,iFontHeight*11));
	iView->DrawText(_L("GG: XXXX-XXXX"),TPoint(0,iFontHeight*12));
	iView->DrawText(_L("Patch: XXXXXX:YYYY"),TPoint(0,iFontHeight*13));
}

void CPicoDriveUi::PutControllerSelect()
{
	iView->Clear();
	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("Controller options"),TPoint(0,iFontHeight));
		if (iEnableSixButtons)
		iView->DrawText(_L("6 button pad"),TPoint(0,iFontHeight*3),iCtrlSelection==EControllerType);
	else
		iView->DrawText(_L("3 button pad"),TPoint(0,iFontHeight*3),iCtrlSelection==EControllerType);
	iView->DrawText(_L("Configure keys"),TPoint(0,iFontHeight*4),iCtrlSelection==EConfigControls);



}


void CPicoDriveUi::PutConfigKeys()
{
	iView->DrawText(_L("PicoDrive S60"),TPoint(0,0));
	iView->DrawText(_L("Configure keys"),TPoint(0,iFontHeight));
	iView->DrawText(_L("Please press:"),TPoint(0,iFontHeight*3));
	iView->DrawText(iKeyNames->MdcaPoint(iCurrentScan),TPoint(0,iFontHeight*4));

	iView->DrawText(_L("Press 'C' to skip this!"),TPoint(0,iFontHeight*6));

}

TInt CPicoDriveUi::IdleCallBackStop(TAny* /*aAppUi*/)
{
	CActiveScheduler::Stop();
	return 0;
}

TInt CPicoDriveUi::StartEmulatorL(TAny* aAppUi)
{
	static_cast<CPicoDriveUi*>(aAppUi)->StartEmulatorL();
	return 0;
}

void CPicoDriveUi::HandleForegroundEventL(TBool aForeground)
{
	if(iView != NULL)
	{
		iView->iForeground=aForeground;
	}

	if(!aForeground)
	{
		if(iView != NULL)
		{
			iView->AbortNow(RDirectScreenAccess::ETerminateCancel);
		}
	}
	else
	{
		if(iView != NULL)
		{
		iView->Restart(RDirectScreenAccess::ETerminateCancel);
		UpdateScreen();
		}

		SetKeyBlockMode(ENoKeyBlock);
	}
	
}

void CPicoDriveUi::UpdateScreen()
{
	if(!iEmuRunning)
	{
		iView->Clear();
		if(iCurrentScan>=0)
		{
			PutConfigKeys();
		}
		else if(iPicoMenu==ESelectScrMenu)
		{
			PutScreenSelect();
		}
		else if (iPicoMenu==EAboutPicoMenu)
		{
			PutAbout(ETrue);
		}
		else if (iPicoMenu==ESelectSoundMenu)
		{
			PutSoundSelect();
		}
		else if (iPicoMenu==ESelectControlsMenu)
		{
			PutControllerSelect();
		}
		else if (iPicoMenu==ESelectCheatMenu)
		{
			PutCheatSelect();
		}
		else 
			PutMainMenu();
		
	}
}

void CPicoDriveUi::StartEmulatorL()
{	
	iView->Clear();
	TTime time;
	time.HomeTime();
#ifdef S60V3
	LastSecond=(TInt)(time.Int64()/1000);//GetTickCount(); 
#else
	LastSecond=(TInt)(time.Int64()/1000).GetTInt();//GetTickCount(); 
#endif
	FramesDone=0;
	
	iSndStream->SetVolume((iSndStream->MaxVolume()*iSoundVolume)/10);
	UpdatePSndRate();

	while(iEmuRunning)
	{	
		EmulateFrame();
	}
	iSndStream->Stop();
	iView->Clear();
	PutMainMenu();
	
}

/**
 * Calculates the palette table 0-4096
 */
void CPicoDriveUi::CalculatePaletteTable()
{
	for(TInt cram =0;cram<4096;cram++)
	{
		if(iDisplayMode == EColor4K)
		{
			unsigned short high=0x111;
			high|=(cram&0x00e)<<8; // Red
			high|=(cram&0x0e0); // Green
			high|=(cram&0xe00)>> 8; // Blue
			gColorMapTab[cram] =  high;
		}
		else // 64K color mode
		{
			unsigned short high=0x0841;
			// Convert 0000bbbb ggggrrrr
			// to      rrrr1ggg g10bbbb1
			high|=(cram&0x00f)<<12; // Red
			high|=(cram&0x0f0)<< 3; // Green
			high|=(cram&0xf00)>> 7; // Blue
			gColorMapTab[cram] = high;
		}
	}
}

int CPicoDriveUi::TargetInit()
{
	PicoCram=NULL;
	TUint16 currentLine = 0;
	TReal xFactor = 1;
	TReal xNarrowFactor = 1;
	TReal yFactor = 1;
	TInt loop;

	memset(framebuff,0,framebuffsize);

	if(iScrMode==0)
	{
#ifdef S60V3
		xFactor = ((TReal)Targ.view.iBr.iX/(TReal)320);
		xNarrowFactor = ((TReal)Targ.view.iBr.iX/(TReal)256);
		
		if(xFactor>1)
			xFactor = 1;
		
		if(xNarrowFactor>1)
			xNarrowFactor = 1;

		yFactor = ((TReal)Targ.view.iBr.iY/(TReal)240);

		if(yFactor>1)
			yFactor = 1;		
		
		for(loop = 0;loop<256;loop++)
		{
			TInt line = (loop*xNarrowFactor);
			TInt nextLine = ((loop+1)*xNarrowFactor);
			if(line != nextLine)
			{
				gNarrowColumnStepTable[loop] = 1;
			}
			else
			{
				gNarrowColumnStepTable[loop] = 0;
			}				
		}
		
		for(loop = 0;loop<320;loop++)
		{
			TInt line = (loop*xFactor);
			TInt nextLine = ((loop+1)*xFactor);
			if( line != nextLine) 
			{
				gColumnStepTable[loop] = 1;
			}
			else
			{
				gColumnStepTable[loop] = 0;
			}		
		}

		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			if((loop*yFactor) != ((loop+1)*yFactor))
				currentLine++;
		}
		myPicoScan=EmulateScan16;
		
#else
		if(iInterpolate)
			myPicoScan=EmulateScan16_176Interpolate;
		else
			myPicoScan=EmulateScan16_176;
		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			if(((loop*3)/4) != (((loop+1)*3)/4))
				currentLine++;
		}
#endif
		KBitValTable[0] = 1;
		KBitValTable[1] = 2;
		KBitValTable[2] = 4;
		KBitValTable[3] = 8;
	}
#ifdef S60V3
	else if (iScrMode == 1 || iScrMode == 2)
	{
		xFactor = ((TReal)Targ.view.iBr.iY/(TReal)320);
		xNarrowFactor = ((TReal)Targ.view.iBr.iY/(TReal)256);	

		if(xFactor>2)
			xFactor = 2;
		
		if(xNarrowFactor>2)
			xNarrowFactor = 2;

		yFactor = ((TReal)Targ.view.iBr.iX/(TReal)240);
		if(yFactor>2)
			yFactor = 2;
		
		for(loop = 0;loop<256;loop++)
		{
			TInt col = (loop*xNarrowFactor);
			TInt nextCol= ((loop+1)*xNarrowFactor);
		
			gNarrowColumnStepTable[loop] = nextCol-col;;						
		}
		
		for(loop = 0;loop<320;loop++)
		{
			TInt col = (loop*xFactor);
			TInt nextCol= ((loop+1)*xFactor);
		
			gColumnStepTable[loop] = nextCol-col;;
		}

		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			TInt line = (loop*yFactor);
			TInt nextLine = ((loop+1)*yFactor);
			currentLine+=(nextLine-line);
		}
		if(iScrMode == 2)
		{
			KBitValTable[0] = 4;
			KBitValTable[1] = 8;
			KBitValTable[2] = 2;
			KBitValTable[3] = 1;
			myPicoScan=EmulateScanFullRight16;
		}
		else
		{
			KBitValTable[0] = 8;
			KBitValTable[1] = 4;
			KBitValTable[2] = 1;
			KBitValTable[3] = 2;
			myPicoScan=EmulateScanFull16;
		}
#else
else if (iScrMode == 1)
{
		if(iInterpolate)
			myPicoScan=EmulateScanFull16_176Interpolate;
		else
			myPicoScan=EmulateScanFull16_176;

		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			if(((loop*3)/4) != (((loop+1)*3)/4))
				currentLine++;
		}
		KBitValTable[0] = 8;
		KBitValTable[1] = 4;
		KBitValTable[2] = 1;
		KBitValTable[3] = 2;
#endif	
	}
#ifndef S60V3
	else if(iScrMode==2)
	{
		if(iInterpolate)
			myPicoScan=EmulateScanFullRight16_176Interpolate;
		else
			myPicoScan=EmulateScanFullRight16_176;
		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			if(((loop*3)/4) != (((loop+1)*3)/4))
				currentLine++;
		}
		KBitValTable[0] = 4;
		KBitValTable[1] = 8;
		KBitValTable[2] = 2;
		KBitValTable[3] = 1;
	}
#endif
	else 
	{
#ifdef S60V3
		xFactor = ((TReal)Targ.view.iBr.iX/(TReal)320);
		xNarrowFactor = ((TReal)Targ.view.iBr.iX/(TReal)256);
		
		if(xFactor>2)
			xFactor = 2;
		
		if(xNarrowFactor>2)
			xNarrowFactor = 2;

		yFactor = ((TReal)Targ.view.iBr.iY/(TReal)240);

		if(yFactor>2)
			yFactor = 2;		
		
		for(loop = 0;loop<256;loop++)
		{
			TInt col = (loop*xNarrowFactor);
			TInt nextCol= ((loop+1)*xNarrowFactor);
		
			gNarrowColumnStepTable[loop] = nextCol-col;;						
		}
		
		for(loop = 0;loop<320;loop++)
		{
			TInt col = (loop*xFactor);
			TInt nextCol= ((loop+1)*xFactor);
		
			gColumnStepTable[loop] = nextCol-col;;
		}

		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			TInt line = (loop*yFactor);
			TInt nextLine = ((loop+1)*yFactor);
			currentLine+=(nextLine-line);
		}
		myPicoScan=EmulateStretchScan16;
#else
		if(iScrMode == 4)
		{
			myPicoScan=EmulateStretchScan16_320;
			iInterpolate = EFalse; // not needed
		}
		else
		{
		if(iInterpolate)
			myPicoScan=EmulateStretchScan16_176Interpolate;
		else
			myPicoScan=EmulateStretchScan16_176;
		}

		for(TInt loop = 0;loop<240;loop++)
		{
			gLineTable[loop] = currentLine;
			if(((loop*15)/16) != (((loop+1)*15)/16))
				currentLine++;
		}
#endif
		KBitValTable[0] = 1;
		KBitValTable[1] = 2;
		KBitValTable[2] = 4;
		KBitValTable[3] = 8;
	}
	
	if(iView)
		iView->SetRect(TRect(TPoint(0,0),iEikonEnv->ScreenDevice()->SizeInPixels()));
#ifdef S60V3
	switch(iScrMode)
	{
	case 0:
	case 3:
	case 4:
		iPutRect=TRect(TPoint(0,0),TSize(xFactor*320,yFactor*240));
		iPutPoint=TPoint(Targ.view.Size().iWidth/2-iPutRect.Size().iWidth/2,Targ.view.Size().iHeight/2-iPutRect.Size().iHeight/2);
		
		break;
	case 1:
	case 2:
		iPutRect=TRect(TPoint(0,0),TSize(yFactor*240,xFactor*320));
		iPutPoint=TPoint(Targ.view.Size().iWidth/2-iPutRect.Size().iWidth/2,Targ.view.Size().iHeight/2-iPutRect.Size().iHeight/2);
		break;

	}
#else
	switch(iScrMode)
	{
	case 0:
		iPutPoint=TPoint(0,20);
		iPutRect=TRect(TPoint(0,0),TSize(176,168));	
		break;
	case 1:
	case 2:
		iPutPoint=TPoint(4,1);
		iPutRect=TRect(TPoint(4,1),TSize(168,205));

		break;
	case 3:
	case 4:
		iPutPoint=TPoint(0,0);
		iPutRect=TRect(TPoint(0,0),TSize(176,208));
		break;
	}
#endif
	CalulateLineStarts();
	return 0;
}
#ifdef S60V3
#define KCenterOffset 8
#else
#define KCenterOffset 0
#endif
void CPicoDriveUi::CalulateLineStarts()
{
	switch(iScrMode)
	{
	case 0:
	case 3:
	case 4:
		{
			for(TInt loop = 0;loop<Targ.view.Size().iHeight;loop++)
			{
				gLineOffsets[loop] = loop*Targ.scanline_length;
			}
		}break;
	case 1:
		{		
			for(TInt loop = 0;loop<Targ.view.Size().iHeight;loop++)
			{
				gLineOffsets[loop] = loop*2+Targ.screen_offset+KCenterOffset;
			}
		}break;
	case 2:
		{
			for(TInt loop = 0;loop<Targ.view.Size().iHeight;loop++)
			{
				gLineOffsets[loop] = Targ.scanline_length-loop*2-KCenterOffset;
			}
		}break;	
	}
}

bool IsZip(char *filename)
{
	unsigned char buf[2];
	FILE *fp;
	
	if((fp=fopen(filename,"rb"))!=NULL)
	{
		fread(buf, 2, 1, fp);
		fclose(fp);
		return(memcmp(buf,"PK",2)==0);
	}
	if(fp)fclose(fp);
	return false;
}

int CPicoDriveUi::EmulateInit()
{
	FILE *f=NULL;
	EmulateExit(); // Make sure exited
	TargetInit(); // Find out where to put the screen
	PicoInit();
	// Load cartridge
	if(IsZip(RomName))
	{
		if(CartLoadZip(RomName,&RomData,&RomSize))
		{		
			return 1;
		}
	}
	else
	{
		// Load cartridge
		RFile file;
		TInt error = file.Open(iEikonEnv->FsSession(),iRomName,0);
		file.Close();
		f=fopen(RomName,"rb"); 
		if (f==NULL) 
			return 1;
		TInt result =PicoCartLoad(f,&RomData,&RomSize);
		fclose(f);
		if(result)
			return 1; // failed to load cart
	}
	
	TParsePtr parser(iRomName);
	TPtr8 ptr(Pico.rom_name,511);
	ptr.Copy(parser.DriveAndPath());
	ptr.Append(parser.Name());
	ptr.ZeroTerminate();

	PicoCartInsert(RomData,RomSize);
	Load_Patch_File();
	saveLoadGame(1, 1); // load sram if any saved

	if(!iEnableSixButtons)
		PicoOpt=PicoOpt& 223;
	else
		PicoOpt=PicoOpt|32;	
	return 0;
}

void CPicoDriveUi::EmulateExit()
{
	// Save sram if any
	if(RomData != NULL)
	{
		if(SRam.changed)
		{
		saveLoadGame(0,1);
		SRam.changed = 0;
		}

		Save_Patch_File();
		// Remove cartridge
		PicoCartInsert(NULL,0);
		PicoUnloadCart(RomData);
		RomData=NULL; RomSize=0;
	}
	PicoExit();
}

int CPicoDriveUi::InputFrame()
{
	if(PicoOpt & 3)
	{
		PsndOut=(short*)(iMonoSound.Ptr()+2*iCurrentSeg*PsndLen);
	}
	else
	{
		PsndOut=NULL;
	}

	Patch_Codes();

	PicoFrame();

	if(PicoOpt & 3)
	{
		iCurrentSeg++;
		if(iCurrentSeg==6)
		{
			iMonoSound.SetLength(PsndLen*2*6);
			iSndStream->WriteL(iMonoSound);
			iCurrentSeg=0;
		}
	}
	
	TCallBack callback(IdleCallBackStop,this);
	iIdleCallBack.Cancel();
	iIdleCallBack.Set(callback);
	iIdleCallBack.CallBack();
	CActiveScheduler::Start();
	
	
	PicoPad[0]=iPad1;
	return 0;
}



int CPicoDriveUi::EmulateFrame()
{
	int i=0,need=2;
	if (!iRomLoaded) 
		return 1;
	// Speed throttle:
	if(iFrameSkip ==-1) // auto skipping
	{
		int time=0,frame=0;
		TTime newtime;
		newtime.HomeTime();
#ifdef S60V3
		TInt64 tic=(newtime.Int64()/1000);
		time=(tic-LastSecond); // This will be about 0-1000 ms
#else
		TInt64 tic=(newtime.Int64()/1000).GetTInt();
		time=(tic-LastSecond).GetTInt(); // This will be about 0-1000 ms
#endif
		frame=time*FramesPerSecond/1000;
		need=frame-FramesDone;
		FramesDone=frame;
		
		if (FramesPerSecond>0)
		{
			// Carry over any >60 frame count to one second
			while (FramesDone>=FramesPerSecond) { FramesDone-=FramesPerSecond; LastSecond+=1000; }
		}
		
		if (need<=0) { 
			TTime nextTime;
			do
			{
			nextTime.HomeTime();
			}while((nextTime.Int64()-newtime.Int64())<15000);
		}
		if (need>10) need=10; // Limit frame skipping
	}
	else
	{
		need = iFrameSkip+1;
	}
	PicoSkipFrame=1;
	for (i=0;i<need-1;i++) InputFrame(); // Frame skip if needed
	PicoSkipFrame=0;

	// Now final frame is drawn:
	TBitmapUtil util(iBackBuffer);
	util.Begin(TPoint(0,0));
	Targ.screen=(unsigned char*)iBackBuffer->DataAddress();
	
	if (Targ.screen == NULL) 
	{
		util.End();
		return 1;
	}
	
	PicoScan=myPicoScan; // Setup scanline callback
	InputFrame();


	if(PicoOpt & 0x10) // need to render separatly
	{
		unsigned short* framebuffptr = framebuff+2632;

		if(!(Pico.video.reg[12]&1))
		{
			framebuffptr=framebuffptr-32;
		}

		TInt skipNext =0;
		for(TInt loop = 0;loop<224;loop++)
		{
			if(skipNext == 0)
			{
				skipNext = PicoScan(loop,framebuffptr);
			}
			else
				skipNext--;
			framebuffptr+=328;
		}
	}
	PicoScan=NULL;
	util.End();

	Targ.screen = NULL;
	
	iView->PutBitmap(iBackBuffer,iPutPoint,iPutRect);;
	
	return 0;
}


void CPicoDriveUi::MaoscOpenComplete(TInt aError)
{
	if(aError == KErrNone)
	{
		iSndStream->SetPriority(EPriorityMuchMore, EMdaPriorityPreferenceNone);
		iSndStream->SetVolume((iSndStream->MaxVolume()*iSoundVolume)/10);
		if(!UpdatePSndRate())
		{
			PsndRate = 8000;
			UpdatePSndRate();
		}
	}
	CActiveScheduler::Stop();
}

void CPicoDriveUi::MaoscBufferCopied(TInt /*aError*/, const TDesC8& /*aBuffer*/)
{
}

void CPicoDriveUi::MaoscPlayComplete(TInt aError)
{
	if(aError != KErrNone)
	{
		iSndStream->SetVolume((iSndStream->MaxVolume()*iSoundVolume)/10);
		UpdatePSndRate();
	}
}

TBool CPicoDriveUi::UpdatePSndRate()
{
	TInt sampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
	if(PsndRate == 11025)
		sampleRate = TMdaAudioDataSettings::ESampleRate11025Hz;
	else if (PsndRate == 16000)
		sampleRate = TMdaAudioDataSettings::ESampleRate16000Hz;
	else if (PsndRate == 22050)
		sampleRate = TMdaAudioDataSettings::ESampleRate22050Hz;

	TRAPD(err,iSndStream->SetAudioPropertiesL(sampleRate,TMdaAudioDataSettings::EChannelsMono));
	return (err == KErrNone);
}

size_t gzRead2(void *p, size_t _size, size_t _n, void *file)
{
	return gzread(file, p, _n);
}


size_t gzWrite2(void *p, size_t _size, size_t _n, void *file)
{
	return gzwrite(file, p, _n);
}



// this function is shared between both threads
int CPicoDriveUi::saveLoadGame(int load, int sram)
{
	int res = 0;
	
	if(!(iRomName.Length()>0)) return -1;
	
	// make save filename
	strcpy(saveFname,RomName);
	saveFname[KMaxFileName-5] = 0;
	if(saveFname[strlen(saveFname)-4] == '.') saveFname[strlen(saveFname)-4] = 0;
	strcat(saveFname, sram ? ".srm" : ".mds");
	
	if(sram) {
		int sram_size = SRam.end-SRam.start+1;
		if(SRam.reg_back & 4) sram_size=0x2000;
		if(!SRam.data) return 0; // SRam forcefully disabled for this game
		if(load) {
			PmovFile = fopen(saveFname, "rb");
			if(!PmovFile) return -1;
			fread(SRam.data, 1, sram_size, (FILE *) PmovFile);
			fclose((FILE *) PmovFile);
		} else {
			// sram save needs some special processing
			// see if we have anything to save
			for(; sram_size > 0; sram_size--)
				if(SRam.data[sram_size-1]) break;
				
				if(sram_size) {
					PmovFile = fopen(saveFname, "wb");
					res = fwrite(SRam.data, 1, sram_size, (FILE *) PmovFile);
					res = (res != sram_size) ? -1 : 0;
					fclose((FILE *) PmovFile);
				}
		}
		PmovFile = 0;
		return res;
	} else {
		// try gzip first
		//if(currentConfig.iFlags & 0x80) {
		strcat(saveFname, ".gz");
		if( (PmovFile = gzopen(saveFname, load ? "rb" : "wb")) ) {
			areaRead  = gzRead2;
			areaWrite = gzWrite2;
			if(!load) gzsetparams(PmovFile, 9, Z_DEFAULT_STRATEGY);
		} else
			saveFname[strlen(saveFname)-3] = 0;
		//	}
		
		if(!PmovFile) { // gzip failed or was disabled
			if( (PmovFile = fopen(saveFname, load ? "rb" : "wb")) ) {
				areaRead  = (arearw *) fread;
				areaWrite = (arearw *) fwrite;
			}
		}
		if(PmovFile) {
			PmovAction = load ? 6 : 5; // load/save
			PmovState();
			if(areaRead == gzRead2)
				gzclose(PmovFile);
			else fclose ((FILE *) PmovFile);
			PmovFile = 0;
		} else {
			res = -1;
		}
		
		return res;
	}
	
}

CQPicoDriveView::~CQPicoDriveView()
{
	iDsa->Cancel();
	delete iDsa;
}

void CQPicoDriveView::Restart(RDirectScreenAccess::TTerminationReasons /*aReason*/)
{
	if(iForeground)
	{
		iDsa->Cancel();
		iDsa->StartL();
		iDsa->Gc()->SetClippingRegion(iDsa->DrawingRegion());
		iDrawingOn=ETrue;
	}
}

void CQPicoDriveView::AbortNow(RDirectScreenAccess::TTerminationReasons /*aReason*/)
{
	iDsa->Cancel();
	iDrawingOn=EFalse;
}



void CQPicoDriveView::Draw(const TRect& aRect) const
{
	CWindowGc& gc=SystemGc();
	
	gc.SetBrushColor(KRgbBlack);
	gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc.SetPenStyle(CGraphicsContext::ENullPen);
	gc.DrawRect(aRect);
	static_cast<CPicoDriveUi*>(iEikonEnv->EikAppUi())->StartAsynchUpdate();
}

void CQPicoDriveView::ConstructL()
{
	CreateWindowL();
	ActivateL();
	SetFocus(ETrue);
	SetRect(TRect(TPoint(0,0),iEikonEnv->ScreenDevice()->SizeInPixels())/*iEikonEnv->EikAppUi()->ClientRect()*/);
	iDsa=CDirectScreenAccess::NewL(iEikonEnv->WsSession(),*iEikonEnv->ScreenDevice(),Window(),*this);
	iDsa->StartL();
	iDsa->Gc()->SetClippingRegion(iDsa->DrawingRegion());
	iDrawingOn=ETrue;
	if(Window().DisplayMode() != EColor4K && Window().DisplayMode() != EColor64K)
	{
		Window().SetRequiredDisplayMode(EColor64K); // Try to set 64K color mode
	}
}
void CQPicoDriveView::Clear()
{
	CBitmapContext* gc;
	if(iDrawingOn)
	{
		gc=iDsa->Gc();
	}
	else
	{
		ActivateGc();
		gc=&SystemGc();
	}
	
	gc->SetBrushColor(KRgbBlack);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc->SetPenStyle(CGraphicsContext::ENullPen);
	gc->DrawRect(Rect());	
	if(iDrawingOn)
	{
		iDsa->ScreenDevice()->Update();
		iEikonEnv->WsSession().Flush();
	}
	else
	{
		DeactivateGc();
	}
}

void CQPicoDriveView::DrawText(const TDesC& aText,TPoint aPoint,TBool aHighLight,TRgb aTextColour)
{
	CBitmapContext* gc;
	if(iDrawingOn)
	{
		gc=iDsa->Gc();
	}
	else
	{
		ActivateGc();
		gc=&SystemGc();
	}
	
	gc->SetBrushColor(KRgbBlack);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	if(!aHighLight)
	{
		gc->SetPenColor(aTextColour);
	}
	else
	{
		gc->SetPenColor(KRgbRed);
	}
	gc->SetPenStyle(CGraphicsContext::ESolidPen);
	aPoint.iY+=iEikonEnv->NormalFont()->HeightInPixels()-2;
	aPoint.iX=Size().iWidth/2-iEikonEnv->NormalFont()->TextWidthInPixels(aText)/2;
	gc->UseFont(iEikonEnv->NormalFont());
	gc->DrawText(aText,aPoint);
	gc->DiscardFont();
	
	if(iDrawingOn)
	{
		iDsa->ScreenDevice()->Update();		
	}
	else
	{
		DeactivateGc();
	}
}

TInt CQPicoDriveView::DrawTextInRect(const TDesC& aText,TRect aRect,TInt aStartPos)
{
	CBitmapContext* gc;
	TInt pos = aStartPos;
	TInt len = aText.Length();
	if(iDrawingOn)
	{
		gc=iDsa->Gc();
	}
	else
	{
		ActivateGc();
		gc=&SystemGc();
	}
	
	gc->SetBrushColor(KRgbBlack);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	
	gc->SetPenColor(KRgbWhite);
	
	gc->SetPenStyle(CGraphicsContext::ESolidPen);
	gc->UseFont(iEikonEnv->DenseFont());
	while(pos<len && aRect.iTl.iY<aRect.iBr.iY-(iEikonEnv->DenseFont()->HeightInPixels()+3))
	{
		TInt newline = aText.Right(len-pos).Locate('\n');
		if(newline == KErrNotFound)
			newline=(len-1)-pos;
		gc->DrawText(aText.Mid(pos,newline),aRect,iEikonEnv->DenseFont()->HeightInPixels());
		pos=pos+newline+1; // skip new line
		aRect.iTl+=TSize(0,iEikonEnv->DenseFont()->HeightInPixels()+3);
	}
	gc->DiscardFont();
	
	if(iDrawingOn)
	{
		iDsa->ScreenDevice()->Update();
	}
	else
	{
		DeactivateGc();
	}
	return pos;
}


void CQPicoDriveView::PutBitmap(CFbsBitmap* aBitmap,TPoint aPoint,TRect aRect)
{
	if(iDrawingOn)
	{
		//#ifdef __WINS__
		iDsa->Gc()->BitBlt(aPoint,aBitmap,aRect);
		iDsa->ScreenDevice()->Update();
		//#endif
		//iEikonEnv->WsSession().Flush();
	}
	else
	{
		ActivateGc();
		CWindowGc& gc=SystemGc();
		gc.BitBlt(aPoint,aBitmap,aRect);
		DeactivateGc();
	}
}



void Execute()
{
	__UHEAP_MARK;
	CTrapCleanup* cleanup = CTrapCleanup::New();
	
	// Create a eikenv
	CEikonEnv* eikenv = new CEikonEnv;
	if (!eikenv) 
	{
		return /*KErrNoMemory*/;
	}
	TRAPD(eikErr, eikenv->ConstructL());
	if (eikErr != KErrNone) 
	{
		delete eikenv;
		return /*eikErr*/;
	}
	CPicoDriveUi* appUi = new (ELeave) CPicoDriveUi;
	if (!appUi) 
	{
		delete eikenv;
		return /*KErrNoMemory*/;
	}
	
	TRAPD(constructErr,appUi->ConstructL());
	
	eikenv->SetAppUi(appUi);	// passing ownership of appUi to coe
	
	TInt leaveValue = KErrNone;	
	if (leaveValue != KErrNone)
	{
		delete eikenv;
	}
	else
	{
		// now accept request from clients (start the scheduler)
		eikenv->ExecuteD();
		//delete eikenv;							// ExecuteD kills eikenv
		
	}
	
	delete cleanup;
	
	__UHEAP_MARKEND;
}

//ARM build
#ifdef S60V3
CPicoDriveApp::CPicoDriveApp()
{
}

CPicoDriveApp::~CPicoDriveApp()
{
}

CApaDocument* CPicoDriveApp::CreateDocumentL()
{
	return new (ELeave) CPicoDriveDoc(*this);
}


TUid CPicoDriveApp::AppDllUid()const
{
	return TUid::Uid(0xA00007BE);
}

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
CDictionaryStore* CPicoDriveApp::OpenIniFileLC(RFs& aFs) const
{
	return CEikApplication::OpenIniFileLC(aFs);
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

#ifdef S60V3
LOCAL_C
#endif
CApaApplication* NewApplication() {
	// Return pointer to newly created Application
	return new CPicoDriveApp;
}
#include <eikstart.h>
#endif
GLDEF_C TInt E32Main()
{
#ifdef S60V3	
 	return EikStart::RunApplication(NewApplication);
#else
	Execute();
#endif
	return KErrNone;
}
#if defined(__WINS__)

#ifndef S60V3
EXPORT_C TInt WinsMain()
{
	E32Main();
	return KErrNone;
}
#endif

extern "C" void my_free(void* anAddress)
{
	if(gChunk != NULL)
	{
		gChunk->Free(anAddress);
	}
}

extern "C" void* my_malloc(int aSize)
{
	if(gChunk != NULL)
	{
		return gChunk->Alloc(aSize);
	}
 return NULL;
}
#endif

