//=============================================================================
// Contains all the hooks and interfaces between the emulator interface
// and the main emulator core.
//=============================================================================

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <3ds.h>

#include <dirent.h>

#include "3dstypes.h"
#include "3dsemu.h"
#include "3dsexit.h"
#include "3dsgpu.h"
#include "3dssound.h"
#include "3dsui.h"
#include "3dsinput.h"
#include "3dsfiles.h"
#include "3dsinterface.h"
#include "3dsmain.h"
#include "3dsasync.h"
#include "3dsimpl.h"
#include "3dsopt.h"
#include "3dsconfig.h"
#include "3dsvideo.h"

extern "C" {
#include "3dshack.h"
}

extern "C" void debugWait();
extern "C" void clearBottomScreen();

#include "extern.h"
#include "3dssoundqueue.h"

//---------------------------------------------------------
// All other codes that you need here.
//---------------------------------------------------------
#include "3dsdbg.h"
#include "3dsimpl.h"
#include "3dsimpl_gpu.h"
#include "3dsimpl_tilecache.h"
#include "shaderfast2_shbin.h"
#include "shaderslow_shbin.h"
#include "shaderslow2_shbin.h"

#include "../pico/patch.h"
#include "../pico/pico.h"
#include "../pico/pico_int.h"
#include "../platform/common/emu.h"
#include "platform.h"
#include "cheats.h"

extern "C" int YM2612Write_(unsigned int a, unsigned int v);
extern int ctr_svchack_successful;

//----------------------------------------------------------------------
// Settings
//----------------------------------------------------------------------
SSettings3DS settings3DS;

#define SETTINGS_ALLSPRITES             0
#define SETTINGS_IDLELOOPPATCH          1
#define SETTINGS_BIOS                   2
#define SETTINGS_CPUCORE                3
#define SETTINGS_LOWPASSFILTER          4
#define SETTINGS_CONTROLLERTYPE         5
#define SETTINGS_REGION                 6


//----------------------------------------------------------------------
// Input Bitmasks
//----------------------------------------------------------------------
#define SMD_BUTTON_A                    0x0040
#define SMD_BUTTON_B                    0x0010
#define SMD_BUTTON_C                    0x0020
#define SMD_BUTTON_X                    0x0400
#define SMD_BUTTON_Y                    0x0200
#define SMD_BUTTON_Z                    0x0100
#define SMD_BUTTON_START                0x0080
#define SMD_BUTTON_MODE                 0x0800
#define SMD_BUTTON_UP                   0x0001
#define SMD_BUTTON_DOWN                 0x0002
#define SMD_BUTTON_LEFT                 0x0004
#define SMD_BUTTON_RIGHT                0x0008

//----------------------------------------------------------------------
// Menu options
//----------------------------------------------------------------------

SMenuItem optionsForFont[] = {
    MENU_MAKE_DIALOG_ACTION (0, "Tempesta",               ""),
    MENU_MAKE_DIALOG_ACTION (1, "Ronda",                  ""),
    MENU_MAKE_DIALOG_ACTION (2, "Arial",                  ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForStretch[] = {
    MENU_MAKE_DIALOG_ACTION (0, "No Stretch",               "No stretch"),
    MENU_MAKE_DIALOG_ACTION (7, "4:3 (NTSC) Width",         "Stretch Width Only to 320px"),
    MENU_MAKE_DIALOG_ACTION (1, "4:3 (NTSC) Fit",           "Stretch to 320x240"),
    MENU_MAKE_DIALOG_ACTION (5, "5:4 (PAL) Fit",            "Stretch to 300x240"),
    MENU_MAKE_DIALOG_ACTION (2, "Fullscreen",               "Stretch to 400x240"),
    MENU_MAKE_DIALOG_ACTION (3, "Cropped 4:3 (NTSC) Fit",   "Crop & Stretch to 320x240"),
    MENU_MAKE_DIALOG_ACTION (6, "Cropped 5:4 (PAL) Fit",    "Crop & Stretch to 320x240"),
    MENU_MAKE_DIALOG_ACTION (4, "Cropped Fullscreen",       "Crop & Stretch to 400x240"),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForFrameskip[] = {
    MENU_MAKE_DIALOG_ACTION (0, "Disabled",                 ""),
    MENU_MAKE_DIALOG_ACTION (1, "Enabled (max 1 frame)",    ""),
    MENU_MAKE_DIALOG_ACTION (2, "Enabled (max 2 frames)",    ""),
    MENU_MAKE_DIALOG_ACTION (3, "Enabled (max 3 frames)",    ""),
    MENU_MAKE_DIALOG_ACTION (4, "Enabled (max 4 frames)",    ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForFrameRate[] = {
    MENU_MAKE_DIALOG_ACTION (0, "Default based on ROM/Region",  ""),
    MENU_MAKE_DIALOG_ACTION (1, "50 FPS",                       ""),
    MENU_MAKE_DIALOG_ACTION (2, "60 FPS",                       ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForRegion[] = {
    MENU_MAKE_DIALOG_ACTION (0, "Default based on ROM",     ""),
    MENU_MAKE_DIALOG_ACTION (1, "USA",                      ""),
    MENU_MAKE_DIALOG_ACTION (2, "Europe",                   ""),
    MENU_MAKE_DIALOG_ACTION (3, "Japan",                    ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForAutoSaveSRAMDelay[] = {
    MENU_MAKE_DIALOG_ACTION (1, "1 second",     ""),
    MENU_MAKE_DIALOG_ACTION (2, "10 seconds",   ""),
    MENU_MAKE_DIALOG_ACTION (3, "60 seconds",   ""),
    MENU_MAKE_DIALOG_ACTION (4, "Disabled",     "Touch bottom screen to save"),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForTurboFire[] = {
    MENU_MAKE_DIALOG_ACTION (0, "None",         ""),
    MENU_MAKE_DIALOG_ACTION (10, "Slowest",      ""),
    MENU_MAKE_DIALOG_ACTION (8, "Slower",       ""),
    MENU_MAKE_DIALOG_ACTION (6, "Slow",         ""),
    MENU_MAKE_DIALOG_ACTION (4, "Fast",         ""),
    MENU_MAKE_DIALOG_ACTION (2, "Faster",         ""),
    MENU_MAKE_DIALOG_ACTION (1, "Very Fast",    ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForButtons[] = {
    MENU_MAKE_DIALOG_ACTION (0,                 "None",            ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_A,      "A Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_B,      "B Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_C,      "C Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_X,      "X Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_Y,      "Y Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_Z,      "Z Button",        ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_MODE,   "Mode Button",     ""),
    MENU_MAKE_DIALOG_ACTION (SMD_BUTTON_START,  "Start Button",    ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForControllerType[] = {
    MENU_MAKE_DIALOG_ACTION (0,                 "3-Button Controller",        ""),
    MENU_MAKE_DIALOG_ACTION (1,                 "6-Button Controller",        ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsFor3DSButtons[] = {
    MENU_MAKE_DIALOG_ACTION (0,                 "None",             ""),
    MENU_MAKE_DIALOG_ACTION (KEY_A,             "3DS A Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_B,             "3DS B Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_X,             "3DS X Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_Y,             "3DS Y Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_L,             "3DS L Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_R,             "3DS R Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_ZL,            "New 3DS ZL Button",     ""),
    MENU_MAKE_DIALOG_ACTION (KEY_ZR,            "New 3DS ZR Button",     ""),
    MENU_MAKE_LASTITEM  ()
};

SMenuItem optionsForSpriteFlicker[] =
{
    MENU_MAKE_DIALOG_ACTION (0, "Hardware Accurate",   "Flickers like real hardware"),
    MENU_MAKE_DIALOG_ACTION (1, "Better Visuals",      "Looks better, less accurate"),
    MENU_MAKE_LASTITEM  ()  
};

SMenuItem optionsForIdleLoopPatch[] =
{
    MENU_MAKE_DIALOG_ACTION (1, "Enabled",              "Faster but some games may freeze"),
    MENU_MAKE_DIALOG_ACTION (0, "Disabled",             "Slower but better compatibility"),
    MENU_MAKE_LASTITEM  ()  
};

SMenuItem optionsForCPUCore[] =
{
    MENU_MAKE_DIALOG_ACTION (1, "Fast",                 "Faster, heavily optimized CPU core."),
    MENU_MAKE_DIALOG_ACTION (2, "Compatible",           "More compatible, but slower CPU core."),
    MENU_MAKE_LASTITEM  ()  
};


SMenuItem optionsForPaletteFix[] =
{
    MENU_MAKE_DIALOG_ACTION (0, "Enabled",              "Best, but slower"),
    MENU_MAKE_DIALOG_ACTION (1, "Disabled",             "Fastest, less accurate"),
    MENU_MAKE_LASTITEM  ()  
};

SMenuItem optionMenu[] = {
    MENU_MAKE_HEADER1   ("GLOBAL SETTINGS"),
    MENU_MAKE_PICKER    (11000, "  Screen Stretch", "How would you like the final screen to appear?", optionsForStretch, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (18000, "  Font", "The font used for the user interface.", optionsForFont, DIALOGCOLOR_CYAN),
    MENU_MAKE_CHECKBOX  (15001, "  Hide text in bottom screen", 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_CHECKBOX  (12002, "  Automatically save state on exit and load state on start", 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER1   ("GAME-SPECIFIC SETTINGS"),
    MENU_MAKE_PICKER    (10000, "  Frameskip", "Try changing this if the game runs slow. Skipping frames help it run faster but less smooth.", optionsForFrameskip, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (12003, "  Region", "Each game a default supported region. Override if required.", optionsForRegion, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (12000, "  Framerate", "Some games run at 50 (PAL), 60 (NTSC) FPS by default. Override if required.", optionsForFrameRate, DIALOGCOLOR_CYAN),
    //MENU_MAKE_PICKER    (19000, "  Flickering Sprites", "Sprites on real hardware flicker. You can disable for better visuals.", optionsForSpriteFlicker, DIALOGCOLOR_CYAN),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER1   ("AUDIO"),
    MENU_MAKE_CHECKBOX  (20000, "  Low-pass filter", 0),
    MENU_MAKE_CHECKBOX  (13502, "  Apply volume to all games", 0),
    MENU_MAKE_GAUGE     (14000, "  Volume Amplification", 0, 8, 4),
    MENU_MAKE_LASTITEM  ()
};


SMenuItem controlsMenu[] = {
    MENU_MAKE_HEADER1   ("CONTROLLER TYPE"),
    MENU_MAKE_PICKER    (13100, "  Sega Controller Type", "", optionsForControllerType, DIALOGCOLOR_CYAN),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER1   ("BUTTON CONFIGURATION"),
    MENU_MAKE_CHECKBOX  (13500, "  Apply button mappings to all games", 0),
    MENU_MAKE_CHECKBOX  (13501, "  Apply rapid fire settings to all games", 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS A Button"),
    MENU_MAKE_PICKER    (13010, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13020, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13000, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS B Button"),
    MENU_MAKE_PICKER    (13011, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13021, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13001, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS X Button"),
    MENU_MAKE_PICKER    (13012, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13022, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13002, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS Y Button"),
    MENU_MAKE_PICKER    (13013, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13023, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13003, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS L Button"),
    MENU_MAKE_PICKER    (13014, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13024, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13004, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS R Button"),
    MENU_MAKE_PICKER    (13015, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13025, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13005, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("New 3DS ZL Button"),
    MENU_MAKE_PICKER    (13016, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13026, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13006, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("New 3DS ZR Button"),
    MENU_MAKE_PICKER    (13017, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13027, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_GAUGE     (13007, "  Rapid-Fire Speed", 0, 10, 0),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS SELECT Button"),
    MENU_MAKE_PICKER    (13018, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13028, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER2   ("3DS START Button"),
    MENU_MAKE_PICKER    (13019, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (13029, "  Maps to", "", optionsForButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_DISABLED  (""),
    MENU_MAKE_HEADER1   ("EMULATOR FUNCTIONS"),
    MENU_MAKE_CHECKBOX  (13503, "Apply keys to all games", 0),
    MENU_MAKE_PICKER    (23001, "Open Emulator Menu", "", optionsFor3DSButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_PICKER    (23002, "Fast Forward", "", optionsFor3DSButtons, DIALOGCOLOR_CYAN),
    MENU_MAKE_DISABLED  ("  (Works better on N3DS. May freeze/corrupt games.)"),
    MENU_MAKE_LASTITEM  ()
};


//-------------------------------------------------------
SMenuItem optionsForDisk[] =
{
    MENU_MAKE_DIALOG_ACTION (0, "Eject Disk",               ""),
    MENU_MAKE_DIALOG_ACTION (1, "Change to Disk 1 Side A",  ""),
    MENU_MAKE_DIALOG_ACTION (2, "Change to Disk 1 Side B",  ""),
    MENU_MAKE_DIALOG_ACTION (3, "Change to Disk 2 Side A",  ""),
    MENU_MAKE_DIALOG_ACTION (4, "Change to Disk 2 Side B",  ""),
    MENU_MAKE_DIALOG_ACTION (5, "Change to Disk 3 Side A",  ""),
    MENU_MAKE_DIALOG_ACTION (6, "Change to Disk 3 Side B",  ""),
    MENU_MAKE_DIALOG_ACTION (7, "Change to Disk 4 Side A",  ""),
    MENU_MAKE_DIALOG_ACTION (8, "Change to Disk 4 Side B",  ""),
    MENU_MAKE_LASTITEM  ()  
};


//-------------------------------------------------------
// Standard in-game emulator menu.
// You should not modify those menu items that are
// marked 'do not modify'.
//-------------------------------------------------------
SMenuItem emulatorMenu[] = {
    MENU_MAKE_HEADER2   ("Emulator"),               // Do not modify
    MENU_MAKE_ACTION    (1000, "  Resume Game"),    // Do not modify
    MENU_MAKE_HEADER2   (""),

    MENU_MAKE_HEADER2   ("Savestates"),
    MENU_MAKE_ACTION    (2001, "  Save Slot #1"),   // Do not modify
    MENU_MAKE_ACTION    (2002, "  Save Slot #2"),   // Do not modify
    MENU_MAKE_ACTION    (2003, "  Save Slot #3"),   // Do not modify
    MENU_MAKE_ACTION    (2004, "  Save Slot #4"),   // Do not modify
    MENU_MAKE_ACTION    (2005, "  Save Slot #5"),   // Do not modify
    MENU_MAKE_HEADER2   (""),   
    
    MENU_MAKE_ACTION    (3001, "  Load Slot #1"),   // Do not modify
    MENU_MAKE_ACTION    (3002, "  Load Slot #2"),   // Do not modify
    MENU_MAKE_ACTION    (3003, "  Load Slot #3"),   // Do not modify
    MENU_MAKE_ACTION    (3004, "  Load Slot #4"),   // Do not modify
    MENU_MAKE_ACTION    (3005, "  Load Slot #5"),   // Do not modify
    MENU_MAKE_HEADER2   (""),

    MENU_MAKE_HEADER2   ("Others"),                 // Do not modify
    MENU_MAKE_ACTION    (4001, "  Take Screenshot"),// Do not modify
    MENU_MAKE_ACTION    (5001, "  Reset Console"),  // Do not modify
    MENU_MAKE_ACTION    (6001, "  Exit"),           // Do not modify
    MENU_MAKE_LASTITEM  ()
    };





//------------------------------------------------------------------------
// Memory Usage = 0.003 MB   for 4-point rectangle (triangle strip) vertex buffer
#define RECTANGLE_BUFFER_SIZE           0x20000

//------------------------------------------------------------------------
// Memory Usage = 0.003 MB   for 6-point quad vertex buffer (Citra only)
#define CITRA_VERTEX_BUFFER_SIZE        0x200000

// Memory Usage = Not used (Real 3DS only)
#define CITRA_TILE_BUFFER_SIZE          0x200000


//------------------------------------------------------------------------
// Memory Usage = 0.003 MB   for 6-point quad vertex buffer (Real 3DS only)
#define REAL3DS_VERTEX_BUFFER_SIZE      0x10000

// Memory Usage = 0.003 MB   for 2-point rectangle vertex buffer (Real 3DS only)
#define REAL3DS_TILE_BUFFER_SIZE        0x200000



//---------------------------------------------------------
// Settings related to the emulator.
//---------------------------------------------------------
extern SSettings3DS settings3DS;


//---------------------------------------------------------
// Provide a comma-separated list of file extensions
//---------------------------------------------------------
char *impl3dsRomExtensions = "sms,md,smd,gen,rom,bin,iso,32x";


//---------------------------------------------------------
// The title image .PNG filename.
//---------------------------------------------------------
char *impl3dsTitleImage = "./picodrive_3ds_top.png";


//---------------------------------------------------------
// The title that displays at the bottom right of the
// menu.
//---------------------------------------------------------
char *impl3dsTitleText = "PicoDrive for 3DS v0.94";


//---------------------------------------------------------
// The bitmaps for the emulated console's UP, DOWN, LEFT, 
// RIGHT keys.
//---------------------------------------------------------
u32 input3dsDKeys[4] = { SMD_BUTTON_UP, SMD_BUTTON_DOWN, SMD_BUTTON_LEFT, SMD_BUTTON_RIGHT };


//---------------------------------------------------------
// The list of valid joypad bitmaps for the emulated 
// console.
//
// This should NOT include D-keys.
//---------------------------------------------------------
u32 input3dsValidButtonMappings[10] = { SMD_BUTTON_A, SMD_BUTTON_B, SMD_BUTTON_C, SMD_BUTTON_X, SMD_BUTTON_Y, SMD_BUTTON_Z, SMD_BUTTON_MODE, SMD_BUTTON_START, 0, 0 };


//---------------------------------------------------------
// The maps for the 10 3DS keys to the emulated consoles
// joypad bitmaps for the following 3DS keys (in order):
//   A, B, X, Y, L, R, ZL, ZR, SELECT, START
//
// This should NOT include D-keys.
//---------------------------------------------------------
u32 input3dsDefaultButtonMappings[10] = { SMD_BUTTON_C, SMD_BUTTON_B, SMD_BUTTON_X, SMD_BUTTON_A, SMD_BUTTON_Y, SMD_BUTTON_Z, 0, 0, SMD_BUTTON_MODE, SMD_BUTTON_START };




SSoundQueue soundQueue;
SDACQueue dacQueue;
SDACQueue cddaQueue;

int soundSamplesPerGeneration = 0;
int soundSamplesPerSecond = 0;

int audioFrame = 0;
int emulatorFrame = 0;
int picoFrameCounter = 0;
int picoSoundBlockCounter = 0;
int picoDebugC = 0;


static short __attribute__((aligned(4))) sndBuffer[2*44100/50];

//---------------------------------------------------------
// Initializes the emulator core.
//---------------------------------------------------------
bool impl3dsInitializeCore()
{
    int sampleRate = 30000;
    int soundLoopsPerSecond = 60;
    soundSamplesPerGeneration = snd3dsComputeSamplesPerLoop(sampleRate, soundLoopsPerSecond);
	soundSamplesPerSecond = snd3dsComputeSampleRate(sampleRate, soundLoopsPerSecond);

	memset(&defaultConfig, 0, sizeof(defaultConfig));
	defaultConfig.EmuOpt    = 0x9d | EOPT_EN_CD_LEDS;
	defaultConfig.s_PicoOpt = POPT_EN_STEREO|POPT_EN_FM|POPT_EN_PSG|POPT_EN_Z80 |
				  POPT_EN_MCD_PCM|POPT_EN_MCD_CDDA|POPT_EN_MCD_GFX | POPT_EN_MCD_RAMCART |
				  POPT_ACC_SPRITES |
				  POPT_EN_32X|POPT_EN_PWM | POPT_DIS_IDLE_DET;
	defaultConfig.s_PsndRate = sampleRate;
	defaultConfig.s_PicoRegion = 0; // auto
	defaultConfig.s_PicoAutoRgnOrder = 0x814; // US, JP, EU 
	defaultConfig.s_PicoCDBuffers = 0;
	defaultConfig.confirm_save = EOPT_CONFIRM_SAVE;
	defaultConfig.Frameskip = -1; // auto
	defaultConfig.input_dev0 = PICO_INPUT_PAD_3BTN;
	defaultConfig.input_dev1 = PICO_INPUT_PAD_3BTN;
	defaultConfig.volume = 50;
	defaultConfig.gamma = 100;
	defaultConfig.scaling = 0;
	defaultConfig.turbo_rate = 15;
	defaultConfig.msh2_khz = PICO_MSH2_HZ / 1000;
	defaultConfig.ssh2_khz = PICO_SSH2_HZ / 1000;
	memcpy(&currentConfig, &defaultConfig, sizeof(currentConfig));

    // Actual PicoDrive configuration.
    //
	PicoIn.opt = currentConfig.s_PicoOpt;
	PicoIn.sndRate = currentConfig.s_PsndRate;
    PicoIn.sndOut = sndBuffer;
    PicoIn.sndVolumeMul = 100;
	PicoIn.autoRgnOrder = currentConfig.s_PicoAutoRgnOrder;

    // ** Initialize core
    PicoInit();
    PicoDrawSetOutFormat(PDF_RGB555, 0);
    
	// Initialize our GPU.
	// Load up and initialize any shaders
	//
    if (emulator.isReal3DS)
    {
    	gpu3dsLoadShader(0, (u32 *)shaderslow_shbin, shaderslow_shbin_size, 0);     // copy to screen
    	gpu3dsLoadShader(1, (u32 *)shaderfast2_shbin, shaderfast2_shbin_size, 6);   // draw tiles
    }
    else
    {
    	gpu3dsLoadShader(0, (u32 *)shaderslow_shbin, shaderslow_shbin_size, 0);     // copy to screen
        gpu3dsLoadShader(1, (u32 *)shaderslow2_shbin, shaderslow2_shbin_size, 0);   // draw tiles
    }

	gpu3dsInitializeShaderRegistersForRenderTarget(0, 10);
	gpu3dsInitializeShaderRegistersForTexture(4, 14);
	gpu3dsInitializeShaderRegistersForTextureOffset(6);
	
    if (!video3dsInitializeSoftwareRendering(512, 256, GX_TRANSFER_FMT_RGB565))
        return false;
    
	// allocate all necessary vertex lists
	//
    if (emulator.isReal3DS)
    {
        gpu3dsAllocVertexList(&GPU3DSExt.rectangleVertexes, RECTANGLE_BUFFER_SIZE, sizeof(SVertexColor), 2, SVERTEXCOLOR_ATTRIBFORMAT);
        gpu3dsAllocVertexList(&GPU3DSExt.quadVertexes, REAL3DS_VERTEX_BUFFER_SIZE, sizeof(SVertexTexCoord), 2, SVERTEXTEXCOORD_ATTRIBFORMAT);
        gpu3dsAllocVertexList(&GPU3DSExt.tileVertexes, REAL3DS_TILE_BUFFER_SIZE, sizeof(STileVertex), 2, STILETEXCOORD_ATTRIBFORMAT);
    }
    else
    {
        gpu3dsAllocVertexList(&GPU3DSExt.rectangleVertexes, RECTANGLE_BUFFER_SIZE, sizeof(SVertexColor), 2, SVERTEXCOLOR_ATTRIBFORMAT);
        gpu3dsAllocVertexList(&GPU3DSExt.quadVertexes, CITRA_VERTEX_BUFFER_SIZE, sizeof(SVertexTexCoord), 2, SVERTEXTEXCOORD_ATTRIBFORMAT);
        gpu3dsAllocVertexList(&GPU3DSExt.tileVertexes, CITRA_TILE_BUFFER_SIZE, sizeof(STileVertex), 2, STILETEXCOORD_ATTRIBFORMAT);
    }

    if (GPU3DSExt.quadVertexes.ListBase == NULL ||
        GPU3DSExt.tileVertexes.ListBase == NULL ||
        GPU3DSExt.rectangleVertexes.ListBase == NULL)
    {
        printf ("Unable to allocate vertex list buffers \n");
        return false;
    }

	gpu3dsUseShader(0);

#ifndef EMU_RELEASE
    printf("Homebrew: %d\n", envIsHomebrew());
    printf("IsReal3DS: %d\n", emulator.isReal3DS);
#endif

    if (hack3dsInitializeSvcHack())
    {
        if (hack3dsTestDynamicRecompilation())
        {
            ctr_svchack_successful = 1;
            PicoIn.opt |= POPT_EN_DRC;
        }
    }

    return true;
}


//---------------------------------------------------------
// Finalizes and frees up any resources.
//---------------------------------------------------------
void impl3dsFinalize()
{
    // ** Finalize
    video3dsFinalize();
}


//---------------------------------------------------------
// Mix sound samples into a temporary buffer.
//
// This gives time for the sound generation to execute
// from the 2nd core before copying it to the actual
// output buffer.
//---------------------------------------------------------
void impl3dsGenerateSoundSamples(int numberOfSamples)
{
}


//---------------------------------------------------------
// Mix sound samples into a temporary buffer.
//
// This gives time for the sound generation to execute
// from the 2nd core before copying it to the actual
// output buffer.
// 
// For a console with only MONO output, simply copy
// the samples into the leftSamples buffer.
//---------------------------------------------------------
void impl3dsOutputSoundSamples(int numberOfSamples, short *leftSamples, short *rightSamples)
{
    #define BLOCKS_PER_LOOP 2

    for (int i = 0; i < BLOCKS_PER_LOOP; i++)
    {
        if (!soundQueueIsEmpty(&soundQueue))
        {
            int addr, data, value;
            s64 time;

            // Gets all YM2612 commands for the current block.
            //
            // Based BLOCKS_PER_LOOP (= 2), we are generating two blocks of
            // sound samples per frame. Each block generates samples
            // for the number of scanlines / BLOCKS_PER_LOOP.
            //
            soundQueuePeekNext(&soundQueue, &time, &data, &addr, &value);
            time += 1;

            if (i == (BLOCKS_PER_LOOP - 1))
            {
                s64 lastFlushedCommandTime;

                // Gets all YM2612 commands before the last blocks in the queue.
                // (this prevents the queue from overflowing, just in case
                // we don't have enough CPU resources to generate the samples,
                // esp on an Old 3DS.)
                soundQueuePeekLast(&soundQueue, &lastFlushedCommandTime, &data, &addr, &value);

                if (lastFlushedCommandTime - time > BLOCKS_PER_LOOP * 2)
                    time = lastFlushedCommandTime;
            }
            while (true)
            {
                if (!soundQueueRead(&soundQueue, time, &data, &addr, &value))
                    break;
                if (data == 0xff)
                    break;
                YM2612Write_(addr, value);
            }
        }

        int samplesPerLoop = numberOfSamples / BLOCKS_PER_LOOP;
        int samplesThisLoop = numberOfSamples / BLOCKS_PER_LOOP;
        if (i == (BLOCKS_PER_LOOP - 1))
            samplesThisLoop = numberOfSamples - samplesThisLoop * (BLOCKS_PER_LOOP - 1);

        PsndRender3DS(&leftSamples[samplesPerLoop*i], &rightSamples[samplesPerLoop*i], samplesThisLoop);
    }
}


static const char * const biosfiles_us[] = {
	"sdmc:/3ds/picodrive_3ds/bios/us_scd2_9306.bin", "sdmc:/3ds/picodrive_3ds/bios/SegaCDBIOS9303.bin", "sdmc:/3ds/picodrive_3ds/bios/us_scd1_9210.bin", "sdmc:/3ds/picodrive_3ds/bios/bios_CD_U.bin"
};
static const char * const biosfiles_eu[] = {
	"sdmc:/3ds/picodrive_3ds/bios/eu_mcd2_9306.bin", "sdmc:/3ds/picodrive_3ds/bios/eu_mcd2_9303.bin", "sdmc:/3ds/picodrive_3ds/bios/eu_mcd1_9210.bin", "sdmc:/3ds/picodrive_3ds/bios/bios_CD_E.bin"
};
static const char * const biosfiles_jp[] = {
	"sdmc:/3ds/picodrive_3ds/bios/jp_mcd2_921222.bin", "sdmc:/3ds/picodrive_3ds/bios/jp_mcd1_9112.bin", "sdmc:/3ds/picodrive_3ds/bios/jp_mcd1_9111.bin", "sdmc:/3ds/picodrive_3ds/bios/bios_CD_J.bin"
};

static const char *find_bios(int *region, const char *cd_fname)
{
	int i, count;
	const char * const *files;
	FILE *f = NULL;
	int ret;
	if (*region == 4) { // US
		files = biosfiles_us;
		count = sizeof(biosfiles_us) / sizeof(char *);
	} else if (*region == 8) { // EU
		files = biosfiles_eu;
		count = sizeof(biosfiles_eu) / sizeof(char *);
	} else if (*region == 1 || *region == 2) {
		files = biosfiles_jp;
		count = sizeof(biosfiles_jp) / sizeof(char *);
	} else {
		return 0;
	}

	for (i = 0; i < count; i++)
	{
		f = fopen(files[i], "rb");
		if (f) break;
	}

	if (f) {
		fclose(f);
		return files[i];
	} else {
		return NULL;
	}
}


void setSampleRate(bool preserveState)
{
    int sampleRate = 30000;
    int soundLoopsPerSecond = (Pico.m.pal ? 50 : 60);

	// compute a sample rate closes to 30000 kHz for old 3DS, and 44100 Khz for new 3DS.
	//
    u8 new3DS = false;
    APT_CheckNew3DS(&new3DS);
    if (new3DS)
        sampleRate = 44100;
    
    soundSamplesPerGeneration = snd3dsComputeSamplesPerLoop(sampleRate, soundLoopsPerSecond);
	soundSamplesPerSecond = snd3dsComputeSampleRate(sampleRate, soundLoopsPerSecond);

	PicoIn.sndRate = currentConfig.s_PsndRate = defaultConfig.s_PsndRate = soundSamplesPerSecond;
	snd3dsSetSampleRate(
		true,
		soundSamplesPerSecond, 
		soundLoopsPerSecond, 
		true);
    PsndRerate(preserveState ? 0 : 1);
}

//---------------------------------------------------------
// This is called when a ROM needs to be loaded and the
// emulator engine initialized.
//---------------------------------------------------------
bool impl3dsLoadROM(char *romFilePath)
{
    // ** Load ROM
    PicoPatchUnload();
	enum media_type_e media_type;
	media_type = PicoLoadMedia(romFilePath, "/3ds/picodrive_3ds/carthw.cfg", find_bios, NULL);

    PicoSetInputDevice(0, PICO_INPUT_PAD_6BTN);
    PicoSetInputDevice(1, PICO_INPUT_PAD_6BTN);

    // ** Load SRAM
    emu_save_load_sram(file3dsReplaceFilenameExtension(romFileNameFullPath, ".sram"), 1);

	switch (media_type) {
	case PM_BAD_DETECT:
	case PM_BAD_CD:
	case PM_BAD_CD_NO_BIOS:
	case PM_ERROR:
		return false;
	default:
		break;
	}

    video3dsClearAllSoftwareBuffers();
    
    impl3dsResetConsole();
    setSampleRate(false);

	return true;
}


//---------------------------------------------------------
// This is called to determine what the frame rate of the
// game based on the ROM's region.
//---------------------------------------------------------
int impl3dsGetROMFrameRate()
{
	return Pico.m.pal ? 50 : 60;
}



//---------------------------------------------------------
// This is called when the user chooses to reset the
// console
//---------------------------------------------------------
void impl3dsResetConsole()
{	
    cache3dsInit();

    // ** Reset
    PicoReset();
    picoFrameCounter = 0;
    picoSoundBlockCounter = 1;
    soundQueueReset(&soundQueue);
    dacQueueReset(&dacQueue);
    dacQueueReset(&cddaQueue);
}


//---------------------------------------------------------
// This is called when preparing to start emulating
// a new frame. Use this to do any preparation of data,
// the hardware, swap any vertex list buffers, etc, 
// before the frame is emulated
//---------------------------------------------------------
void impl3dsPrepareForNewFrame()
{
	gpu3dsSwapVertexListForNextFrame(&GPU3DSExt.quadVertexes);
    gpu3dsSwapVertexListForNextFrame(&GPU3DSExt.tileVertexes);
    gpu3dsSwapVertexListForNextFrame(&GPU3DSExt.rectangleVertexes);

    video3dsStartNewSoftwareRenderedFrame();
}




bool isOddFrame = false;
bool skipDrawingPreviousFrame = true;

//---------------------------------------------------------
// Initialize any variables or state of the GPU
// before the emulation loop begins.
//---------------------------------------------------------
void impl3dsEmulationBegin()
{
    audioFrame = 0;
    emulatorFrame = 0;

	skipDrawingPreviousFrame = true;

	gpu3dsUseShader(0);
	gpu3dsDisableAlphaBlending();
	gpu3dsDisableDepthTest();
	gpu3dsDisableAlphaTest();
	gpu3dsDisableStencilTest();
	gpu3dsSetTextureEnvironmentReplaceTexture0();
	gpu3dsSetRenderTargetToTopFrameBuffer();
	gpu3dsFlush();	
	//if (emulator.isReal3DS)
	//	gpu3dsWaitForPreviousFlush();

	PicoLoopPrepare();

}


//---------------------------------------------------------
// Polls and get the emulated console's joy pad.
//---------------------------------------------------------
void impl3dsEmulationPollInput()
{
	u32 keysHeld3ds = input3dsGetCurrentKeysHeld();
    u32 consoleJoyPad = input3dsProcess3dsKeys();

    PicoIn.pad[0] = consoleJoyPad;

}


//---------------------------------------------------------
// The following pipeline is used if the 
// emulation engine does software rendering.
//
// You can potentially 'hide' the wait latencies by
// waiting only after some work on the main thread
// is complete.
//---------------------------------------------------------

int lastWait = 0;
#define WAIT_PPF		1
#define WAIT_P3D		2

int currentFrameIndex = 0;





//---------------------------------------------------------
// Renders the main screen texture to the frame buffer.
//---------------------------------------------------------
void impl3dsRenderDrawTextureToFrameBuffer()
{
	t3dsStartTiming(14, "Draw Texture");	

    gpu3dsUseShader(0);
    gpu3dsSetRenderTargetToTopFrameBuffer();
    
    // 320x224
    float tx1 = 0, ty1 = 8;         
    float tx2 = 320, ty2 = 232;

    if (((Pico.video.reg[12] & 1) == 0) && !(PicoIn.AHW & PAHW_SMS))
    {
        tx1 = 32;
        tx2 = 288;
    }
    if (PicoIn.AHW & PAHW_SMS)
    {
        // 256x192
        tx1 += 32; tx2 -= 32;
        ty1 += 16; ty2 -= 16;
    }

    int bx = (400 - (tx2 - tx1)) / 2;
	switch (settings3DS.ScreenStretch)
	{
		case 0:
            // No stretch
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 400, 240, 0, 0x000000ff);
            //gpu3dsDrawRectangle(328, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);

            gpu3dsAddQuadVertexes(bx, 0, 400 - bx, 240, tx1, 0, tx2, 240, 0);
			break;
		case 7:
            // 4:3 NTSC Stretch Width (320x2??)
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 40, 240, 0, 0x000000ff);
            gpu3dsDrawRectangle(360, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(40, 0, 360, 240, tx1, 0, tx2, 240, 0);
			break;
		case 1:
            // 4:3 NTSC Fit (320x240)
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 40, 240, 0, 0x000000ff);
            gpu3dsDrawRectangle(360, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(40, 0, 360, 240, tx1, ty1, tx2, ty2, 0);
			break;
		case 5:
            // 5:4 PAL Fit (300x240)
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 50, 240, 0, 0x000000ff);
            gpu3dsDrawRectangle(350, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(50, 0, 350, 240, tx1, ty1, tx2, ty2, 0);
			break;
		case 2:
            // Full Screen (400x240)
            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(0, 0, 400, 240, tx1, ty1, tx2, ty2, 0);
			break;
		case 3:
            // Cropped 4:3 NTSC (320x240)
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 40, 240, 0, 0x000000ff);
            gpu3dsDrawRectangle(360, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(40, 0, 360, 240, tx1 + 8, ty1 + 8, tx2 - 8, ty2 - 8, 0);
			break;
		case 6:
            // Cropped 4:3 PAL (320x240)
            gpu3dsSetTextureEnvironmentReplaceColor();
            gpu3dsDrawRectangle(0, 0, 50, 240, 0, 0x000000ff);
            gpu3dsDrawRectangle(350, 0, 400, 240, 0, 0x000000ff);

            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(50, 0, 350, 240, tx1 + 8, ty1 + 8, tx2 - 8, ty2 - 8, 0);
			break;
		case 4:
            // Cropped Fullscreen (400x240)
            gpu3dsSetTextureEnvironmentReplaceTexture0();
            gpu3dsBindTextureMainScreen(video3dsGetPreviousScreenTexture(), GPU_TEXUNIT0);
			gpu3dsAddQuadVertexes(0, 0, 400, 240, tx1 + 8, ty1 + 8, tx2 - 8, ty2 - 8, 0);
			break;
	}
    
    gpu3dsDrawVertexes();
	t3dsEndTiming(14);

	t3dsStartTiming(15, "Flush");
	gpu3dsFlush();
	t3dsEndTiming(15);
}


//---------------------------------------------------------
// Executes one frame and draw to the screen.
//
// Note: TRUE will be passed in the firstFrame if this
// frame is to be run just after the emulator has booted
// up or returned from the menu.
//---------------------------------------------------------
void impl3dsEmulationRunOneFrame(bool firstFrame, bool skipDrawingFrame)
{
	t3dsStartTiming(1, "RunOneFrame");

    // Cheats
    apply_cheats();
    ROMCheatUpdate();
    RAMCheatUpdate();
    
	if (!skipDrawingPreviousFrame)
        video3dsTransferFrameBufferToScreenAndSwap();

	t3dsStartTiming(10, "EmulateFrame");
	{
        // Let's try to keep the digital sample queue filled with some data
        emulator.waitBehavior = WAIT_FULL;
        if (dacQueueGetLength(&dacQueue) <= soundSamplesPerGeneration * 2)
            emulator.waitBehavior = WAIT_NONE;

		impl3dsEmulationPollInput();

        PicoIn.skipFrame = skipDrawingFrame;
        PicoDrawSetOutBuf(video3dsGetCurrentSoftwareBuffer(), 1024);
        PicoFrame();
	}

	t3dsEndTiming(10);
    
	if (!skipDrawingFrame)
        video3dsCopySoftwareBufferToTexture();

	if (!skipDrawingPreviousFrame)
		impl3dsRenderDrawTextureToFrameBuffer();	

	skipDrawingPreviousFrame = skipDrawingFrame;
	t3dsEndTiming(1);
    picoFrameCounter++;
}


//---------------------------------------------------------
// Finalize any variables or state of the GPU
// before the emulation loop ends and control 
// goes into the menu.
//---------------------------------------------------------
void impl3dsEmulationEnd()
{
	// We have to do this to clear the wait event
	//
	/*if (lastWait != 0 && emulator.isReal3DS)
	{
		if (lastWait == WAIT_PPF)
			gspWaitForPPF();
		else 
		if (lastWait == WAIT_P3D)
			gpu3dsWaitForPreviousFlush();
	}*/
}



//---------------------------------------------------------
// This is called when the bottom screen is touched
// during emulation, and the emulation engine is ready
// to display the pause menu.
//
// Use this to save the SRAM to SD card, if applicable.
//---------------------------------------------------------
void impl3dsEmulationPaused()
{
    ui3dsDrawRect(50, 140, 270, 154, 0x000000);
    ui3dsDrawStringWithNoWrapping(50, 140, 270, 154, 0x3f7fff, HALIGN_CENTER, "Saving SRAM to SD card...");

    // ** Save SRAM
    emu_save_load_sram(file3dsReplaceFilenameExtension(romFileNameFullPath, ".sram"), 0);
}


//---------------------------------------------------------
// This is called when the user chooses to save the state.
// This function should save the state into a file whose
// name contains the slot number. This will return
// true if the state is saved successfully.
//
// The slotNumbers passed in start from 1.
//---------------------------------------------------------
bool impl3dsSaveState(int slotNumber)
{
	char ext[_MAX_PATH];
    if (slotNumber == 0)
	    sprintf(ext, ".sta");
    else
	    sprintf(ext, ".st%d", slotNumber - 1);

    // ** Save State
    PicoState(file3dsReplaceFilenameExtension(romFileNameFullPath, ext), 1);

    return true;
}


//---------------------------------------------------------
// This is called when the user chooses to load the state.
// This function should save the state into a file whose
// name contains the slot number. This will return
// true if the state is loaded successfully.
//
// The slotNumbers passed in start from 1.
//---------------------------------------------------------
bool impl3dsLoadState(int slotNumber)
{
	char ext[_MAX_PATH];
    if (slotNumber == 0)
	    sprintf(ext, ".sta");
    else
	    sprintf(ext, ".st%d", slotNumber - 1);

    impl3dsResetConsole();
    
    // ** Load State
    PicoState(file3dsReplaceFilenameExtension(romFileNameFullPath, ext), 0);

    return true;
}


//---------------------------------------------------------
// This function will be called everytime the user
// selects an action on the menu.
//
// Returns true if the menu should close and the game 
// should resume
//---------------------------------------------------------
bool impl3dsOnMenuSelected(int ID)
{
    return false;
}



//---------------------------------------------------------
// This function will be called everytime the user 
// changes the value in the specified menu item.
//
// Returns true if the menu should close and the game 
// should resume
//---------------------------------------------------------
bool impl3dsOnMenuSelectedChanged(int ID, int value)
{
    if (ID == 18000)
    {
        ui3dsSetFont(value);
        return false;
    }
    if (ID == 21000)
    {
        settings3DS.OtherOptions[SETTINGS_BIOS] = value;

        menu3dsHideDialog();
        int result = menu3dsShowDialog("Updated CD-ROM BIOS", "Would you like to reset your console?", DIALOGCOLOR_RED, optionsForNoYes);
        menu3dsHideDialog();

        if (result == 1)
        {
            impl3dsResetConsole();
            return true;
        }
    }
    
    return false;
}



//---------------------------------------------------------
// Initializes the default global settings. 
// This method is called everytime if the global settings
// file does not exist.
//---------------------------------------------------------
void impl3dsInitializeDefaultSettingsGlobal()
{
	settings3DS.GlobalVolume = 4;
    settings3DS.OtherOptions[SETTINGS_LOWPASSFILTER] = 1;
}


//---------------------------------------------------------
// Initializes the default global and game-specifi
// settings. This method is called everytime a game is
// loaded, but the configuration file does not exist.
//---------------------------------------------------------
void impl3dsInitializeDefaultSettingsByGame()
{
	settings3DS.MaxFrameSkips = 1;
	settings3DS.ForceFrameRate = 0;
	settings3DS.Volume = 4;

    settings3DS.OtherOptions[SETTINGS_ALLSPRITES] = 0;	
    settings3DS.OtherOptions[SETTINGS_IDLELOOPPATCH] = 0;	
    settings3DS.OtherOptions[SETTINGS_BIOS] = 0;
    settings3DS.OtherOptions[SETTINGS_CPUCORE] = 1;
    settings3DS.OtherOptions[SETTINGS_LOWPASSFILTER] = 1;
}



//----------------------------------------------------------------------
// Read/write all possible game specific settings into a file 
// created in this method.
//
// This must return true if the settings file exist.
//----------------------------------------------------------------------
bool impl3dsReadWriteSettingsByGame(bool writeMode)
{
    bool success = config3dsOpenFile(file3dsReplaceFilenameExtension(romFileNameFullPath, ".cfg"), writeMode);
    if (!success)
        return false;

    config3dsReadWriteInt32("#v1\n", NULL, 0, 0);
    config3dsReadWriteInt32("# Do not modify this file or risk losing your settings.\n", NULL, 0, 0);

    // set default values first.
    if (!writeMode)
    {
        settings3DS.PaletteFix = 0;
        settings3DS.SRAMSaveInterval = 0;
    }

    // v0.90 options
    config3dsReadWriteInt32("Frameskips=%d\n", &settings3DS.MaxFrameSkips, 0, 4);
    config3dsReadWriteInt32("Framerate=%d\n", &settings3DS.ForceFrameRate, 0, 2);
    config3dsReadWriteInt32("TurboA=%d\n", &settings3DS.Turbo[0], 0, 10);
    config3dsReadWriteInt32("TurboB=%d\n", &settings3DS.Turbo[1], 0, 10);
    config3dsReadWriteInt32("TurboX=%d\n", &settings3DS.Turbo[2], 0, 10);
    config3dsReadWriteInt32("TurboY=%d\n", &settings3DS.Turbo[3], 0, 10);
    config3dsReadWriteInt32("TurboL=%d\n", &settings3DS.Turbo[4], 0, 10);
    config3dsReadWriteInt32("TurboR=%d\n", &settings3DS.Turbo[5], 0, 10);
    config3dsReadWriteInt32("Vol=%d\n", &settings3DS.Volume, 0, 8);
    config3dsReadWriteInt32("AllSprites=%d\n", &settings3DS.OtherOptions[SETTINGS_ALLSPRITES], 0, 1);
    config3dsReadWriteInt32("IdleLoopPatch=%d\n", &settings3DS.OtherOptions[SETTINGS_IDLELOOPPATCH], 0, 1);
    config3dsReadWriteInt32("TurboZL=%d\n", &settings3DS.Turbo[6], 0, 10);
    config3dsReadWriteInt32("TurboZR=%d\n", &settings3DS.Turbo[7], 0, 10);
    static char *buttonName[10] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "SELECT","START"};
    char buttonNameFormat[50];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j) {
            sprintf(buttonNameFormat, "ButtonMap%s_%d=%%d\n", buttonName[i], j);
            config3dsReadWriteInt32(buttonNameFormat, &settings3DS.ButtonMapping[i][j]);
        }
    }
    config3dsReadWriteInt32("ButtonMappingDisableFramelimitHold=%d\n", &settings3DS.ButtonHotkeyDisableFramelimit);
    config3dsReadWriteInt32("ButtonMappingOpenEmulatorMenu=%d\n", &settings3DS.ButtonHotkeyOpenMenu);
    config3dsReadWriteInt32("PalFix=%d\n", &settings3DS.PaletteFix, 0, 1);

    // v0.92 options
    config3dsReadWriteInt32("InputType=%d\n", &settings3DS.OtherOptions[SETTINGS_CONTROLLERTYPE]);

    // v0.94 options
    config3dsReadWriteInt32("Region=%d\n", &settings3DS.OtherOptions[SETTINGS_REGION]);

    config3dsCloseFile();
    return true;
}


//----------------------------------------------------------------------
// Read/write all possible global specific settings into a file 
// created in this method.
//
// This must return true if the settings file exist.
//----------------------------------------------------------------------
bool impl3dsReadWriteSettingsGlobal(bool writeMode)
{
    bool success = config3dsOpenFile("./picodrive_3ds.cfg", writeMode);
    if (!success)
        return false;
    
    int deprecated = 0;

    // v0.90 options
    config3dsReadWriteInt32("#v1\n", NULL, 0, 0);
    config3dsReadWriteInt32("# Do not modify this file or risk losing your settings.\n", NULL, 0, 0);

    config3dsReadWriteInt32("ScreenStretch=%d\n", &settings3DS.ScreenStretch, 0, 7);
    config3dsReadWriteInt32("HideUnnecessaryBottomScrText=%d\n", &settings3DS.HideUnnecessaryBottomScrText, 0, 1);
    config3dsReadWriteInt32("Font=%d\n", &settings3DS.Font, 0, 2);
    config3dsReadWriteInt32("UseGlobalButtonMappings=%d\n", &settings3DS.UseGlobalButtonMappings, 0, 1);
    config3dsReadWriteInt32("UseGlobalTurbo=%d\n", &settings3DS.UseGlobalTurbo, 0, 1);
    config3dsReadWriteInt32("UseGlobalVolume=%d\n", &settings3DS.UseGlobalVolume, 0, 1);
    config3dsReadWriteInt32("TurboA=%d\n", &settings3DS.GlobalTurbo[0], 0, 10);
    config3dsReadWriteInt32("TurboB=%d\n", &settings3DS.GlobalTurbo[1], 0, 10);
    config3dsReadWriteInt32("TurboX=%d\n", &settings3DS.GlobalTurbo[2], 0, 10);
    config3dsReadWriteInt32("TurboY=%d\n", &settings3DS.GlobalTurbo[3], 0, 10);
    config3dsReadWriteInt32("TurboL=%d\n", &settings3DS.GlobalTurbo[4], 0, 10);
    config3dsReadWriteInt32("TurboR=%d\n", &settings3DS.GlobalTurbo[5], 0, 10);
    config3dsReadWriteInt32("Vol=%d\n", &settings3DS.GlobalVolume, 0, 8);
    config3dsReadWriteString("Dir=%s\n", "Dir=%1000[^\n]s\n", file3dsGetCurrentDir());
    config3dsReadWriteString("ROM=%s\n", "ROM=%1000[^\n]s\n", romFileNameLastSelected);
    config3dsReadWriteInt32("AutoSavestate=%d\n", &settings3DS.AutoSavestate, 0, 1);
    config3dsReadWriteInt32("TurboZL=%d\n", &settings3DS.GlobalTurbo[6], 0, 10);
    config3dsReadWriteInt32("TurboZR=%d\n", &settings3DS.GlobalTurbo[7], 0, 10);
    static char *buttonName[10] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "SELECT","START"};
    char buttonNameFormat[50];
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 2; ++j) {
            sprintf(buttonNameFormat, "ButtonMap%s_%d=%%d\n", buttonName[i], j);
            config3dsReadWriteInt32(buttonNameFormat, &settings3DS.GlobalButtonMapping[i][j]);
        }
    }
    config3dsReadWriteInt32("UseGlobalEmuControlKeys=%d\n", &settings3DS.UseGlobalEmuControlKeys, 0, 1);
    config3dsReadWriteInt32("ButtonMappingDisableFramelimitHold_0=%d\n", &settings3DS.GlobalButtonHotkeyDisableFramelimit);
    config3dsReadWriteInt32("ButtonMappingOpenEmulatorMenu_0=%d\n", &settings3DS.GlobalButtonHotkeyOpenMenu);
    config3dsReadWriteInt32("LowPassFilter=%d\n", &settings3DS.OtherOptions[SETTINGS_LOWPASSFILTER]);

    // New options come here.

    
    config3dsCloseFile();

    return true;
}



//----------------------------------------------------------------------
// Apply settings into the emulator.
//
// This method normally copies settings from the settings3DS struct
// and updates the emulator's core's configuration.
//
// This must return true if any settings were modified.
//----------------------------------------------------------------------
bool impl3dsApplyAllSettings(bool updateGameSettings)
{
    bool settingsChanged = true;

    // update screen stretch
    //
    if (settings3DS.ScreenStretch == 0)
    {
        settings3DS.StretchWidth = 256;
        settings3DS.StretchHeight = 240;    // Actual height
        settings3DS.CropPixels = 0;
    }
    else if (settings3DS.ScreenStretch == 1)
    {
        // Added support for 320x240 (4:3) screen ratio
        settings3DS.StretchWidth = 320;
        settings3DS.StretchHeight = 240;
        settings3DS.CropPixels = 0;
    }
    else if (settings3DS.ScreenStretch == 2)
    {
        settings3DS.StretchWidth = 400;
        settings3DS.StretchHeight = 240;
        settings3DS.CropPixels = 0;
    }

	// compute a sample rate closes to 30000 kHz for old 3DS, and 44100 Khz for new 3DS.
	//
    u8 new3DS = false;
    APT_CheckNew3DS(&new3DS);
    PicoIn.lowPassFilter = 0;
    if (settings3DS.OtherOptions[SETTINGS_LOWPASSFILTER])
        PicoIn.lowPassFilter = new3DS ? 4 : 3;

    // Update the screen font
    //
    ui3dsSetFont(settings3DS.Font);

    if (updateGameSettings)
    {
        // This fixes the bug where the 50 FPS is selected menu, but game still running in 60 FPS.
        long oldTicksPerFrame = TICKS_PER_SEC / impl3dsGetROMFrameRate();
        if (Pico.rom)
        {
            if (settings3DS.ForceFrameRate == 0 || 
                settings3DS.OtherOptions[SETTINGS_REGION] == 0)
                PicoDetectRegion();

            if (settings3DS.ForceFrameRate == 1)
            {
                Pico.m.pal = 1;
            }
            else if (settings3DS.ForceFrameRate == 2)
            {
                Pico.m.pal = 0;
            }

            if (settings3DS.OtherOptions[SETTINGS_REGION] == 1)
            {
                Pico.m.hardware &= ~0xc0;
                Pico.m.hardware |= 0x80;        // NTSC US
                if (settings3DS.ForceFrameRate == 0)
                    Pico.m.pal = 0;
            }
            else if (settings3DS.OtherOptions[SETTINGS_REGION] == 2)
            {
                Pico.m.hardware &= ~0xc0;
                Pico.m.hardware |= 0xc0;        // Europe
                if (settings3DS.ForceFrameRate == 0)
                    Pico.m.pal = 1;
            }
            else if (settings3DS.OtherOptions[SETTINGS_REGION] == 3)
            {
                Pico.m.hardware &= ~0xc0;
                Pico.m.hardware |= 0x00;        // NTSC JP
                if (settings3DS.ForceFrameRate == 0)
                    Pico.m.pal = 0;
            }
        }
        //long oldTicksPerFrame = TICKS_PER_SEC / impl3dsGetROMFrameRate(); 
        settings3DS.TicksPerFrame = TICKS_PER_SEC / impl3dsGetROMFrameRate();
        if (settings3DS.TicksPerFrame != oldTicksPerFrame)
            setSampleRate(true);

        // update global volume
        //
        if (settings3DS.Volume < 0)
            settings3DS.Volume = 0;
        if (settings3DS.Volume > 8)
            settings3DS.Volume = 8;
        if (settings3DS.GlobalVolume < 0)
            settings3DS.GlobalVolume = 0;
        if (settings3DS.GlobalVolume > 8)
            settings3DS.GlobalVolume = 8;

        static int volumeMul[] = { 16, 20, 24, 28, 32, 40, 48, 56, 64 };

        if (settings3DS.UseGlobalVolume)
            PicoIn.sndVolumeMul = volumeMul[settings3DS.GlobalVolume];
        else
            PicoIn.sndVolumeMul = volumeMul[settings3DS.Volume];

/*
        PicoIn.opt = PicoIn.opt & ~POPT_ACC_SPRITES;
        if (!settings3DS.OtherOptions[SETTINGS_ALLSPRITES])
            PicoIn.opt = PicoIn.opt | POPT_ACC_SPRITES;
*/
        if (settings3DS.OtherOptions[SETTINGS_CONTROLLERTYPE] == 0)
        {
            PicoSetInputDevice(0, PICO_INPUT_PAD_3BTN);
            PicoSetInputDevice(1, PICO_INPUT_PAD_3BTN);
        }
        else
        {
            PicoSetInputDevice(0, PICO_INPUT_PAD_6BTN);
            PicoSetInputDevice(1, PICO_INPUT_PAD_6BTN);
        }
        
    }

    return settingsChanged;
}


//----------------------------------------------------------------------
// Copy values from menu to settings3DS structure,
// or from settings3DS structure to the menu, depending on the
// copyMenuToSettings parameter.
//
// This must return return if any of the settings were changed.
//----------------------------------------------------------------------
bool impl3dsCopyMenuToOrFromSettings(bool copyMenuToSettings)
{
#define UPDATE_SETTINGS(var, tabIndex, ID)  \
    { \
    if (copyMenuToSettings && (var) != menu3dsGetValueByID(tabIndex, ID)) \
    { \
        var = menu3dsGetValueByID(tabIndex, (ID)); \
        settingsUpdated = true; \
    } \
    if (!copyMenuToSettings) \
    { \
        menu3dsSetValueByID(tabIndex, (ID), (var)); \
    } \
    }

    bool settingsUpdated = false;
    UPDATE_SETTINGS(settings3DS.Font, -1, 18000);
    UPDATE_SETTINGS(settings3DS.ScreenStretch, -1, 11000);
    UPDATE_SETTINGS(settings3DS.HideUnnecessaryBottomScrText, -1, 15001);
    UPDATE_SETTINGS(settings3DS.MaxFrameSkips, -1, 10000);
    UPDATE_SETTINGS(settings3DS.ForceFrameRate, -1, 12000);
    UPDATE_SETTINGS(settings3DS.OtherOptions[SETTINGS_REGION], -1, 12003);
    UPDATE_SETTINGS(settings3DS.AutoSavestate, -1, 12002);

    UPDATE_SETTINGS(settings3DS.UseGlobalButtonMappings, -1, 13500);
    UPDATE_SETTINGS(settings3DS.UseGlobalTurbo, -1, 13501);
    UPDATE_SETTINGS(settings3DS.UseGlobalVolume, -1, 13502);
    UPDATE_SETTINGS(settings3DS.UseGlobalEmuControlKeys, -1, 13503);
    if (settings3DS.UseGlobalButtonMappings || copyMenuToSettings)
    {
        for (int i = 0; i < 2; i++)
            for (int b = 0; b < 10; b++)
                UPDATE_SETTINGS(settings3DS.GlobalButtonMapping[b][i], -1, 13010 + b + (i * 10));
    }
    if (!settings3DS.UseGlobalButtonMappings || copyMenuToSettings)
    {
        for (int i = 0; i < 2; i++)
            for (int b = 0; b < 10; b++)
                UPDATE_SETTINGS(settings3DS.ButtonMapping[b][i], -1, 13010 + b + (i * 10));
    }
    if (settings3DS.UseGlobalTurbo || copyMenuToSettings)
    {
        for (int b = 0; b < 8; b++)
            UPDATE_SETTINGS(settings3DS.GlobalTurbo[b], -1, 13000 + b);
    }
    if (!settings3DS.UseGlobalTurbo || copyMenuToSettings) 
    {
        for (int b = 0; b < 8; b++)
            UPDATE_SETTINGS(settings3DS.Turbo[b], -1, 13000 + b);
    }
    if (settings3DS.UseGlobalVolume || copyMenuToSettings)
    {
        UPDATE_SETTINGS(settings3DS.GlobalVolume, -1, 14000);
    }
    if (!settings3DS.UseGlobalVolume || copyMenuToSettings)
    {
        UPDATE_SETTINGS(settings3DS.Volume, -1, 14000);
    }
    if (settings3DS.UseGlobalEmuControlKeys || copyMenuToSettings)
    {
        UPDATE_SETTINGS(settings3DS.GlobalButtonHotkeyOpenMenu, -1, 23001);
        UPDATE_SETTINGS(settings3DS.GlobalButtonHotkeyDisableFramelimit, -1, 23002);
    }
    if (!settings3DS.UseGlobalEmuControlKeys || copyMenuToSettings)
    {
        UPDATE_SETTINGS(settings3DS.ButtonHotkeyOpenMenu, -1, 23001);
        UPDATE_SETTINGS(settings3DS.ButtonHotkeyDisableFramelimit, -1, 23002);
    }

    //UPDATE_SETTINGS(settings3DS.PaletteFix, -1, 16000);

    //UPDATE_SETTINGS(settings3DS.OtherOptions[SETTINGS_ALLSPRITES], -1, 19000);     // sprite flicker
    UPDATE_SETTINGS(settings3DS.OtherOptions[SETTINGS_LOWPASSFILTER], -1, 20000);  // low pass filter
    UPDATE_SETTINGS(settings3DS.OtherOptions[SETTINGS_CONTROLLERTYPE], -1, 13100); // controller type

    return settingsUpdated;
	
}



//----------------------------------------------------------------------
// Clears all cheats from the core.
//
// This method is called only when cheats are loaded.
// This only happens after a new ROM is loaded.
//----------------------------------------------------------------------
void impl3dsClearAllCheats()
{
    clear_cheats();
}



//----------------------------------------------------------------------
// Adds cheats into the emulator core after being loaded up from 
// the .CHX file.
//
// This method is called only when cheats are loaded.
// This only happens after a new ROM is loaded.
//
// This method must return true if the cheat code format is valid,
// and the cheat is added successfully into the core.
//----------------------------------------------------------------------
extern int cheatCount;
bool impl3dsAddCheat(bool cheatEnabled, char *name, char *code)
{
    int index = cheatCount;

    if (decode_cheat(code, index))
    {
        enable_cheat(index, cheatEnabled);
        return true;
    }
    return false;
}


//----------------------------------------------------------------------
// Enable/disables a cheat in the emulator core.
// 
// This method will be triggered when the user enables/disables
// cheats in the cheat menu.
//----------------------------------------------------------------------
void impl3dsSetCheatEnabledFlag(int cheatIdx, bool enabled)
{
    enable_cheat(cheatIdx, enabled);
}



extern void emu_video_mode_change(int start_line, int line_count, int is_32cols)
{

}

void mp3_update(int *buffer, int length, int stereo)
{

}