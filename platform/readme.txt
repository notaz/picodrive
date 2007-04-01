For help / comments / questions visit GP32X boards at:
http://www.gp32x.com/board/


About
-----

This is yet another Megadrive / Genesis / Sega CD / Mega CD emulator, which
was written having ARM-based handheld devices in mind (such as PDAs,
smartphones and handheld consoles like GP2X of course). The critical parts
(renderer, 68K and Z80 cpu interpreters) and some other random code is
written in ARM asm, other code is C. The base code originates from Dave's
(fdave, finalburn) PicoDrive 0.30 for Pocket PC. The Sega/Mega CD code is
roughly based on Stephane Dallongeville's Gens.


How to make it run
------------------

Copy PicoDrive.gpe, code940.bin and mmuhack.o to any place in your filesystem
(all 3 files must be in the same directory) and run PicoDrive.gpe.
Then load a ROM and enjoy! ROMs can be in .smd or .bin format and can be zipped.

If you have any problems (game does not boot, sound is glitchy, broken graphics),
make sure you enable "Accurate timing", "Emulate Z80" and then disable
"Fast renderer". This way you will get the best compatibility this emulator can
provide.


How to run Sega/Mega CD games
-----------------------------

To play any game, you need BIOS files. These files must be copied to the same
directory as PicoDrive.gpe. Files can be named as follows:
US: us_scd1_9210.bin us_scd2_9306.bin SegaCDBIOS9303.bin
EU: eu_mcd1_9210.bin eu_mcd2_9303.bin eu_mcd2_9306.bin
JP: jp_mcd1_9112.bin jp_mcd1_9111.bin
these files can also be zipped.

The game must be dumped to ISO format, but BIN can be used too. If you want
CD music, you must use ISO+mp3 files. Audio from BIN files won't be read at
all due to SD access issues. Also BIN files are usually larger, so it's better
to use ISO. ISO+mp3 files can be named similarly as for other emus.
Here are some examples:

SonicCD.iso             data track
SonicCD_02.mp3          audio track 1 (CD track 2)
SonicCD_03.mp3
...

Sonic the Hedgehog CD (US) - Track 01.iso
Sonic the Hedgehog CD (US) - Track 02.mp3
Sonic the Hedgehog CD (US) - Track 03.mp3
...

ISO files can also be zipped (but not mp3 files, as they are already
compressed). Note that this can cause very long loading times, which may
take up to several minutes. File naming is similar as with uncompressed ISOs.
Example:

SonicCD.zip             data track
SonicCD_02.mp3          audio track 1 (CD track 2)
SonicCD_03.mp3
...


Configuration
-------------

See config.txt file.


Other important stuff
---------------------

* When you use both GP2X CPUs, keep in mind that you can't overclock as high as
  when using ARM920 only. For example my GP2X when run singlecore can reach
  280MHz, but with both cores it's about 250MHz. When overclocked too much,
  it may start hanging and producing random noise.
* PicoDrive is not a mp3 player, so all mp3s MUST be encoded at 44.1kHz stereo.
  Otherwise mp3s will play too fast or too slow.
* Due to internal implementation mp3s must not be larger that ~15MB
  (15548416 bytes). Larger mp3s will not be fully loaded.
* Use lower bitrate for better performance (96 or 128kbps CBRs recommended).
* RAM timings option is good for dualcore operation (it is disabled by
  default because it doesn't work on every GP2X, so enable it in advanced
  options).


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


What is emulated?
-----------------

Genesis/MegaDrive:
main 68k @ 7.6MHz: yes, Cyclone core
z80 @ 3.6MHz: yes, DrZ80 core
VDP: yes, except some quirks not used by games
YM2612 FM: yes, optimized MAME core
SN76489 PSG: yes, MAME core

Sega/Mega CD:
another 68k @ 12.5MHz: yes, Cyclone too
gfx scaling/rotation chip (custom ASIC): not yet (faked only)
PCM sound source: yes
CD-ROM controller: yes (mostly)
bram (internal backup RAM): yes


Problems / limitations
----------------------

* 32x and SVP are not emulated.
* Various VDP quirks (window bug, scroll size 2, etc.) are not emulated,
  as very few games use this (if any at all).
* Some games don't work or have glitches because of inaccurate timing and sync
  between the emulated chips.


Credits
-------

This emulator uses code from these people/projects:

Dave
Cyclone 68000 core, Pico emulation library
Homepage: http://www.finalburn.com/

notaz
GP2X port, Cyclone 68000 hacks, lots of additional coding (see changelog).

Reesy & FluBBa
DrZ80, the Z80 emulator written in ARM assembly.
Homepage: http://reesy.gp32x.de/

Tatsuyuki Satoh, Jarek Burczynski, MultiArcadeMachineEmulator development
software implementation of Yamaha FM sound generator

MultiArcadeMachineEmulator (MAME) development
Texas Instruments SN76489 / SN76496 programmable tone /noise generator
Homepage: http://www.mame.net/

Stephane Dallongeville
Gens, MD/Mega CD/32X emulator. Most Sega CD code is based on this emu.

Helix community
Helix mp3 decoder


Additional thanks
-----------------

* Charles MacDonald (http://cgfm2.emuviews.com/) for old but still very useful
  info about genesis hardware.
* Steve Snake for all that he has done for Genesis emulation scene.
* Bart Trzynadlowski for his SSFII and 68000 docs.
* Haze for his research (http://haze.mameworld.info).
* Mark and Jean-loup for zlib library.
* Anyone else I forgot. You know who you are.

GP2X:
* rlyeh and all the other people behind the minimal library.
* Squidge for his famous squidgehack(tm).
* Dzz for his ARM940 sample code.
* GnoStiC & Puck2099 for USB joystick support.
* Hermes PS2R, god_at_hell for the CpuCtrl library.
* craigix for supplying the GP2X hardware and making this port possible.

Symbian:
* Peter van Sebille for his various open-source Symbian projects to learn from.
* Steve Fischer for his open-source Motorola projects.
* The development team behind "Symbian GCC Improvement Project"
  (http://www.inf.u-szeged.hu/symbian-gcc/) for their updated pre-SymbianOS9
  compile tools.
* AnotherGuest for all his Symbian stuff and support.
* Inder for the icons.


Changelog
---------
1.32
  + Added some new scaling options.
  + Added ability to reaload CD images while game is running (needed for games
    with multiple CDs, like Night Trap).
  * Fixed DMA timing emulation (caused lock-ups for some genesis games).
  * Idle loop detection was picking up wrong code and causing glitches, fixed.
  * The ym2612 code on 940 now can handle multiple updates per frame
    (fixes Thunger Force III "seiren" level drums for example).
  * Memory handlers were ignoring some writes to PSG chip, fixed (missing sounds in
    Popful Mail, Silpheed).
  * Improved z80 timing, should fix some sound problems.
  * Fixed a bug with sram register (fixes Phantasy Star 4).
  * Added code for PRG ram write protection register (Dungeon Explorer).

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
    because it is missing some features (it uses tile-based renderering
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

