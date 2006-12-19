// protocol used to talk between exe and it's launcher

#ifndef __CLIENTSERVER_H
#define __CLIENTSERVER_H

#include <w32std.h>

// names
_LIT(KServerName,   "PicodriveNServ");
_LIT(KServerWGName, "Picosmall"); // window group name
_LIT(KClientName,   "PicodriveN");
_LIT(KClientFind,   "PicodriveN*"); // client search mask (for TFindLibrary)


// opcodes used in message passing between client and server
enum TPicoServRqst {
	PicoMsgLoadState,
	PicoMsgSaveState,
	PicoMsgLoadROM,
	PicoMsgResume,
	PicoMsgReset,
	PicoMsgKeys,
	PicoMsgPause,
	PicoMsgQuit,
	PicoMsgConfigChange,	// launcher -> emu
	PicoMsgRetrieveConfig,  // emu -> launcher
	PicoMsgRetrieveDebugStr,// fixed to 512 bytes 4 now
	kDefaultMessageSlots // this is how many messages we need :)
};


// event messages to launcher
enum TPicoLauncherEvents {
	EEventKeyCfgDone = EEventUser + 1,
	EEventGamePaused,
};


// configuration data to be sent between server and client
struct TPicoConfig {
	enum TPicoScreenRotation {
		PRot0,
		PRot90,
		PRot180,
		PRot270
	};
	enum TPicoScreenMode {
		PMCenter,
		PMFit,
		PMFit2
	};
	enum TPicoFrameSkip {
		PFSkipAuto = -1,
		PFSkip0
	};
	TInt32				iScreenRotation;
	TInt32				iScreenMode;
	TUint32				iFlags;   // LSb->MSb: use_sram, show_fps, enable_sound, sound_rate(3bits), gzip_saves{=0x40}, dont_use_mot_vol
    // enable_ym2612&dac, enable_sn76496, enable_z80, stereo_sound;
    // alt_renderer, 6button_gamepad, accurate_timing
	TInt32				iPicoOpt;
	TInt32				iFrameskip;
	TUint32				iKeyBinds[32];
	TUint32				iAreaBinds[19];
	TInt32				PicoRegion;
};


#endif	// __CLIENTSERVER_H
