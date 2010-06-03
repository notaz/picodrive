#
PicoDrive 1.xx

About
-----

#ifdef PSP
This is yet another Megadrive / Genesis emulator for PSP, but with
Sega CD / Mega CD support. Although it has been originally written having
ARM CPU based devices in mind, it has now been ported to PSP too, by
replacing ARM specific parts with portable C code.
The base code originates from Dave's (fdave, finalburn) PicoDrive 0.30 for
Pocket PC. The Sega/Mega CD code is roughly based on Stephane Dallongeville's
Gens.
#else
This is yet another Megadrive / Genesis / Sega CD / Mega CD emulator, which
was written having ARM-based handheld devices in mind (such as PDAs,
smartphones and handheld consoles like GP2X and Gizmondo of course).
The critical parts (renderer, 68K and Z80 cpu interpreters) and some other
random code is written in ARM asm, other code is C. The base code originates
from Dave's (fdave, finalburn) PicoDrive 0.30 for Pocket PC. The Sega/Mega CD
code is roughly based on Stephane Dallongeville's Gens.
#endif

PicoDrive is the first emulator ever to properly emulate Virtua Racing and
it's SVP chip.


How to make it run
------------------

#ifdef GP2X
Extract all files to some directory on your SD and run PicoDrive.gpe from your
GP2X/Wiz menu. The same .gpe supports GP2X F100/F200 and Wiz, there is no need
to use separate versions.
Then load a ROM and enjoy! ROMs can be in .smd or .bin format and can be zipped.
Sega/Mega CD images can be in ISO/CSO+MP3/WAV or CUE+BIN formats (read below
for more details).
#endif
#ifdef GIZ
First make sure you have homebrew-enabled Service Pack installed. Then copy
PicoDrive.exe and KGSDK.dll to any place in your filesystem (both files must
be in the same directory) and run PicoDrive.exe using the launcher of your choice
(some of them might require renaming PicoDrive.exe to Autorun.exe, placing it in
the root of SD, etc). Then load a ROM and enjoy! ROMs can be placed anywhere, can
be in .smd or .bin format and can be zipped (one ROM per zip).
#endif
#ifdef PSP
If you are running a custom firmware, just copy the whole PicoDrive directory to
/PSP/GAME or /PSP/GAMEXXX directory in your memory stick (it shouldn't matter
which one GAME* directory to use).

If you are on 1.5, there is a separate KXploited version for it.
#endif
#ifdef UIQ
Copy SIS and some ROMs to any directory in your memory stick, and install the SIS.
Then load a ROM and enjoy! ROMs can be in .smd or .bin format and can be zipped.
#endif
#ifdef PANDORA
Just copy the .pnd to <sd card>/pandora/menu or <sd card>/pandora/desktop.
#endif
#ifndef UIQ

This emulator has lots of options with various tweaks (for improved speed mostly),
but it should have best compatibility in it's default config. If suddently you
start getting glitches or change something and forget what, use "Restore defaults"
option.
#endif


How to run Sega/Mega CD games
-----------------------------

To play any game, you need BIOS files. These files must be copied to
#ifdef UIQ
D:\other\PicoDrive\ directory.
#else
#ifdef PANDORA
<sd card>/pandora/appdata/PicoDrive/ directory
(if you run PicoDrive once it will create that directory for you).
#else
the same directory as PicoDrive files.
#endif
#endif
Files can be named as follows:

US: us_scd1_9210.bin us_scd2_9306.bin SegaCDBIOS9303.bin
EU: eu_mcd1_9210.bin eu_mcd2_9303.bin eu_mcd2_9306.bin
JP: jp_mcd1_9112.bin jp_mcd1_9111.bin
these files can also be zipped.

The game must be dumped to ISO/CSO+MP3/WAV or CUE+BIN format. When using
CUE/BIN, you must load .cue file from the menu, or else the emu will not find
audio tracks.
CUE/BIN usually takes a lot of space, so it can be converted to cue/cso/mp3 by
#ifdef PANDORA
using bin_to_cso_mp3 tool, which can be downloaded from:
http://notaz.gp2x.de/releases/misc/bin_to_cso_mp3.zip
See readme in the bin_to_cso_mp3/ directory for details.
#else
using bin_to_cso_mp3 tool, which is included with the emulator. See readme in
the bin_to_cso_mp3/ directory for details.
#endif

ISO+mp3 files can be named similarly as for other emus.
Here are some examples:

SonicCD.iso             data track
SonicCD_02.mp3          audio track 1 (CD track 2)
SonicCD_03.mp3
...

Sonic the Hedgehog CD (US) - Track 01.iso
Sonic the Hedgehog CD (US) - Track 02.mp3
Sonic the Hedgehog CD (US) - Track 03.mp3
...

In case there is a .cue file with properly specified files names in it,
file naming doesn't matter. Just be sure to load .cue from the menu.

It is very important to have the MP3s encoded at 44kHz sample rate and they
must be stereo, or else they will play too fast/slow or won't play at all.
Be sure NOT to use anything but classic mp3 format (don't use things like
mp3pro).

ISO files can also be .cso compressed or zipped (but not mp3 files, as they
are already compressed). CSO will cause slightly longer loading times, and
is not very good for FMV games. Zipping ISOs is not recommended, as it will
cause very long (several minute) loading times, and make some games
unplayable. File naming is similar as with uncompressed ISOs.
Example:

SonicCD.cso             data track
SonicCD_02.mp3          audio track 1 (CD track 2)
SonicCD_03.mp3
...


Other important stuff
---------------------

* Sega/Mega CD: If the game hangs after Sega logo, you may need to enable
  "better sync" and/or "Scale/Rot. fx" options, found in "Sega/Mega CD options"
  submenu, and then reset the game. Some other games may also require
  "CDDA audio" and "PCM audio" to be enabled to work (enabled by default).
  Incorrectly named/missing mp3s may also be the cause.
* Sega/Mega CD: If the background music is missing, you might have named your
  MP3s incorrectly. Read "How to run Sega/Mega CD games" section again.
* Sega/Mega CD: If the game music plays too fast/too slow/out of sync, you have
  encoded your MP3s incorrectly. You will have to re-encode and/or resample them.
  PicoDrive is not a mp3 player, so all mp3s MUST be encoded at 44.1kHz stereo.
  Badly encoded mp3s can cause various kind of problems, like noises, incorrect
  playback speeds, not repeating music or even prevent game from starting.
  Some games (like Snatcher) may hang in certain scenes because of this.
  Some mp3 rippers/encoders remove silence and beginning/end of audio tracks,
  what causes audio desyncs and/or mentioned problems.
  If you have cue/bin rip, you can use the bin_to_cso_mp3 tool (included with
  the emulator) to make a proper iso/mp3 rip.
* Sega/Mega CD: If your games hangs at the BIOS screen (with planets shown),
  you may be using a bad BIOS dump. Try another from a different source.
* Some Sega/Mega CD games don't use Z80 for anything, but they leave it active,
  so disabling Z80 manually (in advanced options) improves performance.
* Use lower bitrate for better performance (96 or 128kbps CBRs recommended).
#ifdef GP2X
* GP2X F100/F200: When you use both GP2X CPUs, keep in mind that you can't
  overclock as high as when using ARM920 only. For example my GP2X when run
  singlecore can reach 280MHz, but with both cores it's about 250MHz. When
  overclocked too much, it may start hanging and producing random noise, or
  causing ARM940 crashes ("940 crashed" message displayed).
* GP2X F100/F200: Due to internal implementation mp3s must not be larger that
  12MB (12582912 bytes). Larger mp3s will not be fully loaded.
#endif


Configuration
-------------

#ifdef UIQ
#include "config.txt"
#else
@@0. "Save slot"
This is a slot number to use for savestates, when done by a button press outside
menu. This can also be configured to be changed with a button
(see "key configuration").

@@0. "Frameskip"
How many frames to skip rendering before displaying another.
"Auto" is recommended.

@@0. "Region"
This option lets you force the game to think it is running on machine from the
specified region, or just to set autodetection order. Also affects Sega/Mega CD.

@@0. "Show FPS"
Self-explanatory. Format is XX/YY, where XX is the number of rendered frames and
YY is the number of emulated frames per second.

@@0. "Enable sound"
Does what it says. You must enable at least YM2612 or SN76496 (in advanced options,
see below) for this to make sense (already done by default).

@@0. "Sound Quality"
#ifdef PSP
Sound sample rate, affects sound quality and emulation performance.
22050Hz setting is the recommended one.
#else
Sound sample rate and stereo mode. Mono is not available in Sega/Mega CD mode.
#endif

@@0. "Confirm savestate"
Allows to enable confirmation on savestate saving (to prevent savestate overwrites),
on loading (to prevent destroying current game progress), and on both or none, when
using shortcut buttons (not menu) for saving/loading.

#ifdef GP2X
@@0. "GP2X CPU clocks"
Here you can change clocks of both GP2X's CPUs. Larger values increase performance.
There is no separate option for the second CPU because both CPUs use the same clock
source. Setting this option to 200 will cause PicoDrive NOT to change GP2X's clocks
at all (this is if you use external program to set clock).
#endif
#ifdef PSP
@@0. "CPU/bus clock"
This allows to change CPU and bus clocks for PSP. 333MHz is recommended.
#endif

@@0. "[Display options]"
Enters Display options menu (see below).

@@0. "[Sega/Mega CD options]"
Enters Sega/Mega CD options menu (see below).

@@0. "[Advanced options]"
Enters advanced options menu (see below).

@@0. "Save cfg as default"
If you save your config here it will be loaded on next ROM load, but only if there
is no game specific config saved (which will be loaded in that case).
You can press left/right to switch to a different config profile.

@@0. "Save cfg for current game only"
Whenever you load current ROM again these settings will be loaded

@@0. "Restore defaults"
Restores all options (except controls) to defaults.


Display options
---------------

#ifndef PANDORA
@@1. "Renderer"
#ifdef GP2X
8bit fast:
This enables alternative heavily optimized tile-based renderer, which renders
pixels not line-by-line (this is what accurate renderers do), but in 8x8 tiles,
which is much faster. But because of the way it works it can't render any
mid-frame image changes (raster effects), so it is useful only with some games.

Other two are accurate line-based renderers. The 8bit is faster but does not
run well with some games like Street Racer.
#endif
#ifdef GIZ
This option allows to switch between 16bit and 8bit renderers. The 8bit one is
a bit faster for some games, but not much, because colors still need to be
converted to 16bit, as this is what Gizmondo requires. It also introduces
graphics problems for some games, so it's best to use 16bit one.
#endif
#ifdef PSP
This option allows to switch between fast and accurate renderers. The fast one
is much faster, because it draws the whole frame at a time, instead of doing it
line by line, like the accurate one does. But because of the way it works it
can't render any mid-frame image changes (raster effects), so it is useful only
for some games.
#endif
#endif

#ifdef GP2X
@@1. "Scaling"
"hw" means GP2X hardware scaler, which causes no performance loss, but scaled
image looks a bit blocky. "sw" means software scaling, which uses pixel
averaging and may look a bit nicer, but blurry. Horizontal scaling is only for
games which use so called "32 column mode" (256x224 or 256x240), and scales
image width to 320 pixels. Vertical scales height to 240 for games which use
height 224 (most of them). Note that Wiz doesn't have the hardware scaler.

@@1. "Tearing Fix"
Wiz only: works around the tearing problem by using portrait mode. Causes ~5-10%
performance hit, but eliminates the tearing effect.

@@1. "Gamma correction"
F100/F200 only: Alters image gamma through GP2X hardware. Larger values make
image to look brighter, lower - darker (default is 1.0).

@@1. "A_SN's gamma curve"
F100/F200 only: If this is enabled, different gamma adjustment method will be
used (suggested by A_SN from gp32x boards). Intended to be used for F100, makes
difference for dark and bright colors.

@@1. "Perfect vsync"
This one adjusts the LCD refresh rate to better match game's refresh rate and
starts synchronizing rendering with it. Should make scrolling smoother and
eliminate tearing on F100/F200.
#endif
#ifdef GIZ
@@1. "Scanline mode"
This option was designed to work around slow framebuffer access (the Gizmondo's
main bottleneck) by drawing every other line (even numbered lines only).
This improves performance greatly, but looses detail.

@@1. "Scale low res mode"
The Genesis/Megadrive had several graphics modes, some of which were only 256
pixels wide. This option scales their width to 320 by using simple
pixel averaging scaling. Works only when 16bit renderer is enabled.

@@1. "Double buffering"
Draws the display to offscreen buffer, and flips it with visible one when done.
Unfortunately this causes serious tearing, unless v-sync is used (next option).

@@1. "Wait for V-sync"
Waits for vertical sync before drawing (or flipping buffers, if previous option
is enabled). Emulation is stopped while waiting, so this causes large performance
hit.
#endif
#ifdef PSP
@@1. "Scale factor"
This allows to resize the displayed image by using the PSP's hardware. The number is
used to multiply width and height of the game image to get the size of image to be
displayed. If you just want to make it fullscreen, just use "Set to fullscreen"
setting below.

@@1. "Hor. scale (for low res. games)"
This one works similarly as the previous setting, but can be used to apply additional
scaling horizontally, and is used for games which use lower (256 pixel wide) Gen/MD
resolution.

@@1. "Hor. scale (for hi res. games)"
Same as above, only for higher (320 pixel wide) resolution using games.

@@1. "Bilinear filtering"
If this is enabled, PSP hardware will apply bilinear filtering on the resulting image,
making it smoother, but blurry.

@@1. "Gamma adjustment"
Color gamma can be adjusted with this.

@@1. "Black level"
This can be used to reduce unwanted "ghosting" effect for dark games, by making
black pixels brighter. Use in conjunction with "gamma adjustment" for more effect.

@@1. "Wait for v-sync"
Wait for the screen to finish updating before switching to next frame, to avoid tearing.
There are 3 options:
* never: don't wait for vsync.
* sometimes: wait only if emulator is running fast enough.
* always: always wait (causes emulation slowdown).

@@1. "Set to unscaled centered"
Adjust the resizing options to set game image to it's original size.

@@1. "Set to 4:3 scaled"
Scale the image up, but keep 4:3 aspect, by adding black borders.

@@1. "Set to fullscreen"
Adjust the resizing options to make the game image fullscreen.
#endif
#ifdef PANDORA
To be updated..
#endif


Sega/Mega CD options 
--------------------

@@2. "CD LEDs"
The Sega/Mega CD unit had two blinking LEDs (red and green) on it. This option
will display them on top-left corner of the screen.

@@2. "CDDA audio"
This option enables CD audio playback.

@@2. "PCM audio"
This enables 8 channel PCM sound source. It is required for some games to run,
because they monitor state of this audio chip.

@@2. "ReadAhead buffer"
This option can prefetch more data from the CD image than requested by game
(to avoid accessing card later), what can improve performance in some cases.
#ifndef PSP
"OFF" is the recommended setting.
#endif

@@2. "Save RAM cart"
Here you can enable 64K RAM cart. Format it in BIOS if you do.

@@2. "Scale/Rot. fx"
The Sega/Mega CD had scaling/rotation chip, which allows effects similar to
"Mode 7" effects in SNES. Unfortunately emulating it is slow, and very few games
used it, so it's better to disable this option, unless game really needs it.

@@2. "Better sync"
This option is similar to "Perfect synchro" in Gens. Some games require it to run,
for example most (all?) Wolfteam games, and some other ones. Don't use it for
games which don't need it, it will just slow them down.


Advanced configuration
----------------------

@@3. "Use SRAM/BRAM savestates"
This will automatically read/write SRAM (or BRAM for Sega/Mega CD) savestates for
games which are using them. SRAM is saved whenever you enter the menu or exit the
emulator.

@@3. "Disable sprite limit"
The MegaDrive/Genesis had a limit on how many sprites (usually smaller moving
objects) can be displayed on single line. This option allows to disable that
limit. Note that some games used this to hide unwanted things, so it is not
always good to enable this option.

@@3. "Emulate Z80"
Enables emulation of Z80 chip, which was mostly used to drive the other sound chips.
Some games do complex sync with it, so you must enable it even if you don't use
sound to be able to play them.

@@3. "Emulate YM2612 (FM)"
This enables emulation of six-channel FM sound synthesizer chip, which was used to
produce sound effects and music.

@@3. "Emulate SN76496 (PSG)"
This enables emulation of PSG (programmable sound generation) sound chip for
additional effects.

Note: if you change sound settings AFTER loading a ROM, you may need to reset
game to get sound. This is because most games initialize sound chips on
startup, and this data is lost when sound chips are being enabled/disabled.

@@3. "gzip savestates"
This will always apply gzip compression on your savestates, allowing you to
save some space and load/save time.

@@3. "Don't save last used ROM"
This will disable writing last used ROM to config on exit (what might cause SD
card corruption according to DaveC).

@@3. "Disable idle loop patching"
Idle loop patching is used to improve performance, but may cause compatibility
problems in some rare cases. Try disabling this if your game has problems.

@@3. "Disable frame limiter"
This allows games to run faster then 50/60fps, useful for benchmarking.

#ifdef GP2X
@@3. "Use ARM940 core for sound"
F100/F200: This option causes PicoDrive to use ARM940T core (GP2X's second CPU)
for sound (i.e. to generate YM2612 samples) to improve performance noticeably.
It also decodes MP3s in Sega/Mega CD mode.

@@3. "RAM overclock"
This overclocks the GP2X RAM chips for improved performance, but may cause
instability. Keep it enabled if it doesn't cause problems.

@@3. "MMU hack"
Makes framebuffer bufferable for improved performance. There are no drawbacks
so it should be left enabled.

@@3. "SVP dynarec"
This enables dynamic recompilation for SVP chip emulated for Virtua Racing game,
what improves it's emulation performance greatly.
#endif


Key configuration
-----------------

Select "Configure controls" from the main menu. Then select "Player 1" and you will
see two columns. The left column lists names of Genesis/MD controller buttons, and
the right column your handheld ones, which are assigned.
#ifndef PANDORA
If you bind 2 different buttons to the same action, you will get a combo
(which means that you will have to press both buttons for that action to happen).
#endif

There is also option to enable 6 button pad (will allow you to configure XYZ
buttons), and an option to set turbo rate (in Hz) for turbo buttons.
#ifndef UIQ


Cheat support
-------------

To use GG/patch codes, you must type them into your favorite text editor, one
per line. Comments may follow code after a whitespace. Only GameGenie and
Genecyst patch formats are supported.
Examples:

Genecyst patch (this example is for Sonic):

00334A:0005 Start with five lives
012D24:0001 Keep invincibility until end of stage
009C76:5478 each ring worth 2
009C76:5678 each ring worth 3
...

Game Genie patch (for Sonic 2):

ACLA-ATD4 Hidden palace instead of death egg in level select
...

Both GG and patch codes can be mixed in one file.

When the file is ready, name it just like your ROM file, but with additional
.pat extension, making sure that case matches.

Examples:

ROM: Sonic.zip
PATCH FILE: Sonic.zip.pat

ROM: Sonic 2.bin
PATCH FILE: Sonic 2.bin.pat

Put the file into your ROMs directory. Then load the .pat file as you would
a ROM. Then Cheat Menu Option should appear in main menu.
#endif


What is emulated?
-----------------

Genesis/MegaDrive:
#ifdef PSP
main 68k @ 7.6MHz: yes, FAME/C core
z80 @ 3.6MHz: yes, CZ80 core
#else
main 68k @ 7.6MHz: yes, Cyclone core
z80 @ 3.6MHz: yes, DrZ80 core
#endif
VDP: yes, except some quirks and modes not used by games
YM2612 FM: yes, optimized MAME core
SN76489 PSG: yes, MAME core
SVP chip: yes! This is first emu to ever do this.
Some in-cart mappers are supported too.

Sega/Mega CD:
#ifdef PSP
another 68k @ 12.5MHz: yes, FAME/C too
#else
another 68k @ 12.5MHz: yes, Cyclone too
#endif
gfx scaling/rotation chip (custom ASIC): yes
PCM sound source: yes
CD-ROM controller: yes (mostly)
bram (internal backup RAM): yes


Problems / limitations
----------------------

* 32x is not emulated.
#ifdef PSP
* SVP emulation is terribly slow.
#endif
* Various VDP modes and quirks (window bug, scroll size 2, etc.) are not
  emulated, as very few games use this (if any at all).
* The emulator is not 100% accurate, so some things may not work as expected.
* The FM sound core doesn't support all features and has some accuracy issues.


Credits
-------

This emulator is made of the code from following people/projects:

notaz
GP2X, UIQ, PSP, Gizmondo ports, CPU core hacks,
lots of additional coding (see changelog).
Homepage: http://notaz.gp2x.de/

fDave
one who started it all:
Cyclone 68000 core and PicoDrive itself

#ifdef PSP
Chui
FAME/C 68k interpreter core
(based on C68K by Stephane Dallongeville)

Stephane Dallongeville (written), NJ (optimized)
CZ80 Z80 interpreter core

#else
Reesy & FluBBa
DrZ80, the Z80 interpreter written in ARM assembly.
Homepage: http://reesy.gp32x.de/

#endif
Tatsuyuki Satoh, Jarek Burczynski, MAME development
software implementation of Yamaha FM sound generator

MAME development
Texas Instruments SN76489 / SN76496 programmable tone/noise generator
Homepage: http://www.mame.net/

Stephane Dallongeville
Gens, MD/Mega CD/32X emulator. Most Sega CD code is based on this emu.
#ifdef PSP

people @ ps2dev.org forums / PSPSDK crew
libaudiocodec code (by cooleyes)
other sample code
#else

Helix community
Helix mp3 decoder
#endif


Additional thanks
-----------------

* Charles MacDonald (http://cgfm2.emuviews.com/) for old but still very useful
  info about genesis hardware.
* Steve Snake for all that he has done for Genesis emulation scene.
* Tasco Deluxe for his reverse engineering work on SVP and some mappers.
* Bart Trzynadlowski for his SSFII and 68000 docs.
* Haze for his research (http://haze.mameworld.info).
* Lordus, Exophase and Rokas for various ideas.
* Nemesis for his YM2612 research.
* Many posters at spritesmind.net forums valuable information.
* Mark and Jean-loup for zlib library.
* ketchupgun for the skin.
#ifdef GP2X
* rlyeh and all the other people behind the minimal library.
* Squidge for his famous squidgehack(tm).
* Dzz for his ARM940 sample code.
* A_SN for his gamma code.
* craigix for supplying the GP2X hardware and making this port possible.
* Alex for the icon.
* All the people from gp32x boards for their support.
#endif
#ifdef GIZ
* Kingcdr's for the SDK and Reesy for the DLL and sound code.
* jens.l for supplying the Gizmondo hardware and making this port possible.
#endif
#ifdef UIQ
* Peter van Sebille for his various open-source Symbian projects to learn from.
* Steve Fischer for his open-source Motorola projects.
* The development team behind "Symbian GCC Improvement Project"
  (http://www.inf.u-szeged.hu/symbian-gcc/) for their updated pre-SymbianOS9
  compile tools.
* AnotherGuest for all his Symbian stuff and support.
#endif
* Inder for some graphics.
* Anyone else I forgot. You know who you are.


Changelog
---------
1.80beta2
  * Pandora: updated documentation.

1.80beta1
  + Added pandora port.
  * Internal refactoring for 32x/SMS support.
  * Move mapper database to external file.
  + Added preliminary SMS emulation.
  + Added emulation of 32x peripherals including VDP. More work is needed here.
  + ARM: Added new SH2 recompiler for 32x. Some unification with SVP one.
  - Disabled most of the above bacause I'm not yet happy with the results.

1.56
  * Changed sync in Sega CD emulation again. Should fix games that
    broke after changes in 1.51a.
  * Fixed default keys rebinding when they shouldn't.
  * Fixed sram being loaded from wrong game.
  * Emu should no longer hang shortly after using fast-forward.
  * Fixed save states sometimes no longer showing up in save state menu.
  * ARM: some asm code refactoring for slight speed improvement.

1.55
  + Added Wiz support. Now the same GP2X binary supports F100/F200 and Wiz.
  * Changed shadow/hilight handling a bit, fixes some effects in Pirates! Gold.
  * Complete input code rewrite. This fixes some limitations like not allowing
    to control both players using single input device. It also allows to use
    more devices (like keyboards) on Linux based devices.
  * Options menu has been reordered, "restore defaults" option added.

1.51b
  * Fixed a crash when uncompressed savestate is loaded.
  * Fixed an idle loop detection related hanging problem.
  * PSP: fixed another palette related regression.
  * UIQ3: updated frontend for the latest emu core.

1.51a
  * Fixed a sync problem between main and sub 68k. Should fix the hanging
    problem for some games.
  * ARM: fixed a crash when CD savestate is loaded just after loading ROM.

1.51
  * Improved bin_to_cso_mp3 tool, it should no longer complain about
    missing lame.exe even if it's in working dir.
  * Fixed a regression from 1.50, which caused slowdowns in Final Fight.
  * Fixed some regressions from 1.50 related to sprite limit and palette
    handling (caused graphical glitches in some games).
  + Added ABC turbo actions to key config.
  * Some other minor adjustments.

1.50
  + Added some basic support for Sega Pico, a MegaDrive-based toy.
  + Added proper support for cue/bin images, including cdda playback.
    .cue sheets with iso/cso/mp3/wav files listed in them are now
    supported too (but 44kHz restriction still applies).
  + Added bin_to_cso_mp3 tool, based on Exophase's bin_to_iso_ogg.
    The tool can convert .cue/.bin Sega CD images to .cso/.mp3.
  * Greatly improved Sega CD load times.
  * Changed how scheduling between 68k and z80 is handled. Improves
    performance for some games. Credits to Lordus for the idea.
  * YM2612 state was not 100% saved, this should be better now.
  * Improved renderer performance for shadow/hilight mode.
  * Added a hack for YM2612 frequency overflow issue (bleep noises
    in Shaq Fu, Spider-Man - The Animated Series (intro music), etc.)
    Credits to Nemesis @ spritesmind forum. Works only if sound rate
    is set to 44kHz.
  + Implemented some sprite rendering improvements, as suggested by
    Exophase. Games with lots of sprites now perform better.
  + Added better idle loop detection, based on Lordus' idea again.
  - "accurate timing" option removed, as disabling it no longer
    improves performance.
  - "accurate sprites" was removed too, the new sprite code can
    properly handle sprite priorities in all cases.
  * Timers adjusted again.
  * Improved .smd detection code.
  * ARM: fixed a bug in DrZ80 core, which could cause problems in
    some rare cases.
  * ARM: fixed a problem of occasional clicks on MP3 music start.
  * Minor general optimizations and menu improvements.
  * Fixed a bug in Sega CD savestate loader, where the game would
    sometimes crash after load.
  * Fixed a crash of games using eeprom (introduced in 1.40b).
  * PSP: fixed suspend/resume (hopefully for real).

1.40c
  * Fixed a problem with sound in Marble Madness.
  * GP2X: Fixed minor problem with key config.

1.40b
  * Fixed sprite masking code. Thanks to Lordus for explaining how it works.
  + Added "disable sprite limit" option.
  + PSP: added black level adjustment to display options.
  * Changed reset to act as 'soft' reset.
  + Added detection for Puggsy (it doesn't really have sram).
  * Some small timing adjustments.

1.40a
  * GP2X: Fixed a binding problem with up and down keys.
  * Default game config no longer overrides global user config.

1.40
  + Added support for SVP (Sega Virtua Processor) to emulate Virtua Racing,
    wrote ARM recompiler and some HLE code for VR. Credits to Exophase and
    Rokas for various ideas.
  * Changed config file format, files are now human-readable. Game specific
    configs are now held in single file (but old game config files are still
    read when new one is missing).
  * Fixed a bug where some key combos didn't work as expected.
  * Fixed a regression in renderer (ARM ports only, some graphic glitches in
    rare cases).
  * Adjusted fast renderer to work with more games, including VR.
  * Fixed a problem where SegaCD RAM cart data was getting lost on reset.
  * GP2X: Greatly reduced SegaCD FMV game slowdowns by disabling read-ahead
    in the Linux kernel and C library (thanks to Rokas and Exophase for ideas
    again). Be sure to keep "ReadAhead buffer" OFF to avoid slowdowns.
  + PicoDrive now comes with a game config file for some games which need
    special settings, so they should now work out-of-the-box. More games will
    be added with later updates.
  + GP2X: Files now can be deleted by pressing A+SELECT in the file browser.

1.35b
  * PSP: mp3 code should no longer fail on 1.5 firmware.
  + PSP: added gamma adjustment option.
  + Added .cso ISO format support. Useful for non-FMV games.
  * It is now possile to force a region after the ROM is loaded.
  * Fixed a sram bug in memhandlers (fixes Shining in the Darkness saves).
  * PSP: fixed another bug in memhanlers, which crashed the emu for some games
    (like NBA Jam and NHL 9x).
  + PSP: added suspend/resume handling for Sega CD games.
  + GP2X: added additional low volume levels for my late-night gaming sessions
    (in stereo mode only).
  + GP2X: added "fast forward" action in key config. Not recommended to use for
    Sega CD, may case problems there.
  * Some other small tweaks I forgot about.

1.35a
  * PSP: fixed a bug which prevented to load any ROMs after testing the BIOS.
  * PSP: fixed incorrect CZ80 memory map setup, which caused Z80 crashes and
    graphics corruption in EU Mega CD model1 BIOS menus.
  + PSP: added additional "set to 4:3 scaled" display option for convenience.
  + PSP: Added an option to disable frame limitter (works only with non-auto frameskip).

1.35
  + PSP port added. Lots of new code for it. Integrated modified FAME/C, CZ80 cores.
  + Some minor generic optimizations.
  * Patched some code which was crashing under PSP, but was working in GP2X/Giz
    (although it should have crashed there too).
  * Readme updated.

1.34
  + Gizmondo port added.
  + Some new optimizations in memory handlers, and for shadow/hilight mode.
  + Added some hacks to make more games work without enabling "accurate timing".
  * Adjusted timing for "accurate timing" mode and added preliminary VDP FIFO
    emulation. Fixes Double Dragon 2, tearing in Chaos Engine and some other games.
  * Fixed a few games not having sound at startup.
  * Updated serial EEPROM code to support more games. Thanks to EkeEke for
    providing info about additional EEPROM types and game mappers.
  * The above change fixed hang of NBA Jam.
  * Minor adjustments to control configurator.

1.33
  * Updated Cyclone core to 0.0088.
  + Added A r k's usbjoy fix.
  + Added "perfect vsync" option, which adjusts GP2X LCD refresh rate and syncs
    emulation to it to eliminate tearing and ensure smoothest scrolling possible.
  + Added an option to use A_SN's gamma curve for gamma correction (improves dark
    and bright color display for mk2s).
  * Sometimes stray sounds were played after loading a savestate. Fixed.
  * Fixed a problem where >6MB mp3s were corrupted in memory (sound glitches in
    Snatcher).
  * PD no longer overwrites video player code in memory, video player now can be
    used after exiting PicoDrive.
  * Fixed a bug which was causing Sonic 3 code to deadlock in some rare conditions
    if "accurate timing" was not enabled.
  * Fixed support for large hacked ROMs like "Ultimate Mortal Kombat Trilogy".
    Upto 10MB hacked ROMs are supported now.
  + Config profiles added (press left/right when saving config).
  * Changed key configuration behavior to the one from gpfce (should be more
    intuitive).
  + Added some skinning capabilities to the menu system with default skin by
    ketchupgun. Delete skin directory if you want old behaviour.
  * Some other little tweaks I forgot about.

1.32
  + Added some new scaling options.
  + Added ability to reload CD images while game is running (needed for games
    with multiple CDs, like Night Trap).
  + Added RAM cart emulation.
  * Fixed DMA timing emulation (caused lock-ups for some genesis games).
  * Idle loop detection was picking up wrong code and causing glitches, fixed.
  * The ym2612 code on 940 now can handle multiple updates per frame
    (fixes Thunger Force III "seiren" level drums for example).
  * Memory handlers were ignoring some writes to PSG chip, fixed (missing sounds in
    Popful Mail, Silpheed).
  * Improved z80 timing, should fix some sound problems.
  * Fixed a bug with sram register (fixes Phantasy Star 4).
  * ROM loader was incorrectly identifying some ROMs as invalid. Fixed.
  * Added code for PRG ram write protection register (Dungeon Explorer).
  * The memory mode register change in 1.31 was unsafe and caused some glitches in
    AH-3 Thunderstrike. Fixed.
  * Fixed a file descriptor leak.
  * Updated documentation, added Gmenu2x manual.

1.31
  * Changed the way memory mode register is read (fixes Lunar 2, broken in 1.30).
  * Fixed TAS opcode on sub-68k side (fixes Batman games).
  * File browser now filters out mp3s, saves and some other files, which are not ROMS.

1.30
  + ISO files now can be zipped. Note that this causes VERY long loading times.
  + Added data pre-buffering support, this allows to reduce frequency of short pauses
    in FMV games (caused by SD access), but makes those pauses longer.
  * Fixed PCM DMA transfers (intro FMV in Popful Mail).
  + Properly implemented "decode" data transformation (Jaguar XJ220).
  * Integrated "better sync" code into cyclone code, what made this mode much faster.
  * Fixed a bug related to game specific config saving.
  * Frameskipper was skipping sound processing, what caused some audio desyncs. Fixed.
  * Fixed reset not working for some games.
  + New assembly optimized memory handlers for CD (gives at least a few fps).
    Also re-enabled all optimizations from 0.964 release.
  + New idle-loop detection code for sub-68k. Speeds up at least a few games.

1.201
  + Added basic cheat support (GameGenie and Genecyst patches).

1.20
  * Fixed a long-standing problem in audio mixing code which caused slight distortions
    at lower sample rates.
  * Changed the way 920 and 940 communicates (again), should be more reliable and give
    slight performance increase.
  * Some optimizations in audio mixing code.
  * Some menu changes (background added, smaller font in ROM browser, savestate loader
    now can select slots).
  + 1M mode DMA transfers implemented (used by FMV games like Night Trap and Sewer Shark).
  + Games now can run code from WORD RAM in 1M mode (fixes Adventures of Willy Beamish).
  + "Cell arrange" address mapping is now emulated (Heart of the alien).
  + "Color numeric operation" is now emulated (text in Lunar 2, Silpheed intro graphics).
  + "Better sync" option added (prevents some games from hanging).

1.14
  + Region autodetection now can be customized.
  * When CDDA music tracks changed, old buffer contents were incorrectly played. Fixed.
  * BRAM is now automatically formatted (no need to enter BIOS menu and format any more).
  * Games now can be reset, CDDA music no longer breaks after loading another ISO.
  * Fixed a race condition between 920 and 940 which sometimes caused CDDA music not to play.
  + Savestates implemented for Sega/Mega CD.
  + PCM sound added.
  * Some mixer code rewritten in asm. 22kHz and 11kHz sound rates are now supported in
    Mega CD mode (but mp3s must still be 44kHz stereo).
  + Timer emulation added.
  * CDC DMA tansfers fixed. Snatcher and probably some more games now boot.
  * 2M word RAM -> VDP transfers fixed, no more corruption in Ecco and some other games.

1.10
  + GP2X: Added experimental Sega CD support.
  + GP2X: Added partial gmv movie playback support.

0.964
  * GP2X: Fixed a sound buffer underflow issue on lower sample rate modes, which was
          happening for NTSC games and causing sound clicks.
  * GP2X: Redone key config to better support USB joysticks (now multiple joysticks
          should be useable and configurable).
  + GP2X: Added save confirmation option.
  + GP2X: Added 940 CPU crash detection.
  + ALL:  UIQ3 port added.

0.963
  * GP2X: Gamma-reset-on-entering-menu bug fixed.
  * GP2X: Recompiled PicoDrive with gcc profiling option set as described here:
          http://www.gp32x.com/board/index.php?showtopic=28490

0.962
  * GP2X: Fixed an issue with incorrect sounds in some games when dualcore operation
          was enabled (for example punch sound in SOR).
  * GP2X: Limited max volume to 90, because higher values often cause distortions.
  * GP2X: Fixed a bug with lower res scaling.
  * GP2X: Gamma is now reset on exit.

0.96
  * ALL:  Severely optimized MAME's YM2612 core, part of it is now rewritten in asm.
  + GP2X: The YM2612's code now can be run in GP2X's ARM940T CPU, what causes large
          performance increase.
  * ALL:  Accurate renderers are slightly faster now.
  + GP2X: Using quadruple buffering instead of doublebuffer now, also updated
          framelimitter, this should eliminate some scrolling and tearing problems.
  * GP2X: Fixed some flickering issues of 8bit accurate renderer.
  + GP2X: craigix's RAM timings now can be enabled in the menu (see advanced options).
  + GP2X: Added ability to save config for specific games only.
  + GP2X: Gamma control added (using GP2X's hardware capabilities for this).
  * GP2X: Volume keys are now configurable.
  + GP2X: GnoStiC added USB joystick support, I made it possible to use it for
          player 2 control (currently untested).
  * GP2X: squidgehack is now applied through kernel module (cleaner way).

0.95
  * ALL:  Fixed a bug in sprite renderer which was causing slowdowns for some games.
  + GP2X: Added command line support
  + GP2X: Added optional hardware scaling for lower-res games like Shining Force.
  * ALL:  Sound chips are now sampled 2 times per frame. This fixed some games which
          had missing sounds (Vectorman 2 1st level, Thunder Force 3 water level,
	      etc.).
  + ALL:  Added another accurate 8-bit renderer which is slightly faster and made it
          default.

0.945
  + GP2X: Added frame limiter for frameskipped modes.
  * GP2X: Increased brightness a bit (unused pixel bits now also contain data).
  * GP2X: Suidgehack was not applied correctly (was applied before allocating some
          high memory and had no effect).

0.94
  + Added GP2X port.
  * Improved interrupt timing, Mazin Saga and Burning Force now works.
  * Rewritten renderer code to better suit GP2X, should be faster on other
    ports too.
  + Added support for banking used by 12-in-1 and 4-in-1 ROMs (thanks Haze).
  + Added some protection device faking, used by some unlicensed games like
    Super Bubble Bobble, King of Fighters, Elf Wor, ... (thanks to Haze again)
  + Added primitive Virtua Racing SVP faking, so menus can be seen now.

0.93
  * Fixed a problem with P900/P910 key configuration in FC mode.
  * Improved shadow/hilight mode emulation. Still not perfect, but should be
    enough for most games.
  + Save state slots added.
  + Region selector added.

0.92
  VDP changes:
  * VDP emulation is now more accurate (fixes flickering in Chase HQ II,
    Super Hang-On and some other problems in other games).
  * HV counter emulation is now much more accurate. Fixes the Asterix games,
    line in Road Rash 3, etc.
  * Minor sprite and layer scroll masking bugs fixed.
  + Added partial interlace mode renderer (Sonic 2 vs mode)
  * Fixed a crash in both renderers when certain size window layers were used.
  + Added emulation of shadow/hilight operator sprites. Other shadow/hilight
    effects are still unemulated.
  + Sprite emulation is more accurate, sprite limit is emulated.
  + Added "accurate sprites" option, which always draws sprites in correct
    order and emulates sprite collision bit, but is significantly slower.

  Emulation changes:
  * Improved interrupt handling, added deferred interrupt emulation
    (Lemmings, etc).
  + Added serial EEPROM SRAM support (Wonder Boy in Monster World,
    Megaman - The Wily Wars and many EA sports games like NBA Jam).
  + Implemented ROM banking for Super Street Fighter II - The New Challengers
  * Updated to the latest version of DrZ80 core, integrated memory handlers
    in it for better performance. A noticeable performance increase, but save
	states may not work from the previous version (you can only use them with
	sound disabled in that case).
  + SRAM word read handler was using incorrect byte order, fixed.

  Changes in Cyclone 0.0086:
  + Added missing CHK opcode handler (used by SeaQuest DSV).
  + Added missing TAS opcode handler (Gargoyles,Bubba N Stix,...). As in real genesis,
    memory write-back phase is ignored (but can be enabled in config.h if needed).
  + Added missing NBCD and TRAPV opcode handlers.
  + Added missing addressing mode for CMP/EOR.
  + Added some minor optimizations.
  - Removed 216 handlers for 2927 opcodes which were generated for invalid addressing modes.
  + Fixed flags for ASL, NEG, NEGX, DIVU, ADDX, SUBX, ROXR.
  + Bugs fixed in MOVEP, LINK, ADDQ, DIVS handlers.
  * Undocumented flags for CHK, ABCD, SBCD and NBCD are now emulated the same way as in Musashi.
  + Added Uninitialized Interrupt emulation.
  + Altered timing for about half of opcodes to match Musashi's.

0.80
  * Nearly all VDP code was rewritten in ARM asm. Gives ~10-25% performance
    increase (depends on game).
  * Optimized 32-column renderer not to render tiles offscreen, games which
    use 32-column display (like Shining Force) run ~50% faster.
  + Added new "Alternative renderer", which gives another ~30-45% performance
    increase (in addition to mentioned above), but works only with some games,
    because it is missing some features (it uses tile-based rendering
    instead of default line-based and disables H-ints).
  + Added "fit2" display mode for all FC gamers. It always uses 208x146 for
    P800 and 208x208 for all other phones.
  + Added volume control for Motorolas (experimental).

  VDP changes:
  + Added support for vertical window (used by Vapor Trail, Mercs, GRIND
    Stormer and others).
  + Added sprite masking (hiding), adds some speed.
  + Added preliminary H counter emulation. Comix Zone and Sonic 3D Blast
    special stage are now playable.
  + Added column based vertical scrolling (Gunstar Heroes battleship level,
    Sonic and Knuckles lava boss, etc).

  Emulation changes:
  + Re-added and improved Z80 faking when Z80 is disabled. Many games now can
    be played without enabling Z80 (Lost Vikings, Syndicate, etc), but some
    still need it (International Superstar Soccer Deluxe).
  * Improved ym2612 timers, Outrun music plays at correct speed, voices in
    Earthworm Jim play better, more games play sound.
  * I/O registers now remember their values (needed for Pirates! Gold)
  + Added support for 6 button pad.

  Changes in Cyclone 0.0083wip:
  + Added missing CHK opcode (used by SeaQuest DSV).
  + Added missing TAS opcode (Gargoyles). As in real genesis, write-back phase
    is ignored (but is enabled for other systems).

  Backported stuff from Snes9x:
  * Fixed Pxxx jog up/down which were not working in game.
  + Added an option to gzip save states to save space.
  + The emulator now pauses whenever it is loosing focus, so it will now pause
    when alarm/ponecall/battery low/... windows come up.
  - Removed 'pause on phonecall' feature, as it is no longer needed.
  + Video fix for asian A1000s.

0.70
  * Started using tools from "Symbian GCC Improvement Project", which give
    considerable speed increase (~4fps in "center 90" mode).
  * Rewrote some drawing routines in ARM assembly (gives ~6 more fps in
    "center 90" mode).
  * Minor improvement to 0 and 180 "fit" modes. Now they look slightly better
    and are faster.
  * Minor stability improvements (emulator is less likely to crash).
  + Added some background for OSD text for better readability.
  + Added Pal/NTSC detection. This is needed for proper sound speed.
  + Implemented Reesy's DrZ80 Z80 emu. Made some changes to it with hope to make
    it faster.
  + Implemented ym2612 emu from the MAME project. Runs well but sometimes sounds
    a bit weird. Could be a little faster, so made some changes too.
  + Implemented SN76489 emu from the MAME project.
  + Added two separate sound output methods (mediaserver and cmaudiofb) with
    autodetection (needs testing).
  * Fixed VDP DMA fill emulation (as described in Charles MacDonald's docs),
    fixes Contra and some other games.

0.301
  Launcher:
  * Launcher now starts emulation process from current directory,
    not from hardcoded paths.
  * Improved 'pause on call' feature, should hopefully work with Motorola phones.

0.30
  Initial release.


Disclaimer
----------

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
POSSIBILITY OF SUCH DAMAGE. 

SEGA/Genesis/MegaDrive/SEGA-CD/Mega-CD/32X are trademarks of
Sega Enterprises Ltd.

