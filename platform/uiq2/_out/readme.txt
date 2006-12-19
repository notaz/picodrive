
About
-----

PicodriveN is another port of PicoDrive, Dave's Megadrive / Genesis
emulator for Pocket PC. This version is based on PicoDrive 0.030 and is
made for Symbian UIQ devices. It is alternative version to another port by
AnotherGuest / Someone and is not based on it (so it has a little
different name). It also has full sound support (starting
from version 0.70) .


Features
--------

* Good compatibility (> 90%)
* Improved Cyclone 68000 core.
* Zipped ROMs and savestates.
* SRAM support, including serial SRAM.
* Game screen rotation with many render modes (like 'centered' and 'fit').
* Selectable frameskip.
* Configurable keys and touchpad.
* Flip-closed mode for SE phones.
* Full sound support.


Problems / limitations
----------------------

* 32x, Sega CD, SVP are not emulated.
* Various VDP quirks (window bug, scroll size 2, etc.) are not emulated,
  as very few games use this.
* Some games don't work or have glitches because of inaccurate sync.


Configuration
-------------

1. Keys:

If it looks confusing to you, check this tutorial first:
http://notaz.atspace.com/pico_tut/

There are no default settings.
When you start key configuration mode, black screen with dark-red squares will
appear. Also there will be little 'control' on the right with the function
name in it, and arrows on the corners of it. You can tap on these corners to
select a function. You can also tap on these squares to bind that function to
them. This way you can associate touchpad areas with game-controls or functions.
I also made a small square in every corner of the screen to be used as a virtual
button for some function, like save state. You can bind it as you like. To
bind phone buttons, simply select the function you need, and press a button
you want. To unbind any key or touchpad area, simply push or tap it again.
To configure flip-closed mode, enter configuration mode and close flip.

When finished, select 'done' and press any key. You can also hold 'Power'
button for a while to exit (seems to work on PXXX only).

You need to bind 'pause emu' function to be able exit game when ROM is loaded.
You can also exit game by holding 'power' button (possibly 'end' for motorola
users (?)).

2. Main Settings:

Here you can set the orientation of screen and the drawing mode. The "fit"
option will scale the image so it fully fits in the screen, but some detail
will be lost. "center" displays the game at the center of the screen, but
non-fitting parts are not visible then (better for RPG games with lots of
text, which becomes unreadable in 'fit' mode). "fit2" was meant for Pxxx FC
gaming and will always use 208x146 for P800 and 208x208 for all other phones.

"Fast renderer" enables faster rendering method, but it works only with some
games (some other have serious glitches or even hang).

"Accurate timing" is needed for some games to run (like Red Zone). It should
be kept off for all other games, because it slows emulation down. Some games
also need this option for proper sound, so enable this if game has any
glitches.

"Accurate sprites" fixes sprite priority problems, for example if game
character is in front of or behind some object it should not be, this option
should fix it. This option does not work in "Fast renderer" mode.

"Show FPS" shows game frames per second in format XX/YY, where XX is the
number of frames shown per previous second, and YY is the number of frames
emulated, but not necessarily shown. By calculating YY-XX you get the number
of skipped frames per second.

3. Sound settings:

Sound emulation is very picky on CPU power (in most cases sound alone uses
more CPU power than everything else altogether), but it is still possible to
play some games. When using sound, the recommended display modes are "fit 0"
and "fit 180", because these are the fastest ones. Also try "Alternative
renderer", but it might cause graphical glitches. You must use auto frameskip
when using sound, or else you will get stuttering sound. Also, it is
recommended to exit all other non-vital apps (you can use SMan for this),
disable bluetooth and any other devices your phone may have. I also noticed
that simply connecting the phone battery charger strangely slows everything
down.

"Enable sound" tries to enable sound output on your device, but that alone is
not enough to get sound. You need to enable the sound chips below:
"Z80" is secondary CPU in genesis and is mostly used to control the other 2
sound chips. So if you disable Z80, sound will be lost in most games, with
some exceptions like Sonic1. It is possible to use Z80 for other things,
some games do that and Z80 must be enabled to run them at all.
"YM2612" is a fairly complex Frequency Modulation (FM) sound synthesis chip.
It was the main sound output device in genesis and is horrible CPU hog when
is tried to be emulated in software. Disabling it gives large speed
improvement, but most of the sound is lost.
"SN76496" is programmable sound generator (PSG) chip, used for various sound
effects and music elements.
The lowest setting is audio quality setting, which should be left set to
"8000Hz mono", because other choces slow everything down terribly and
are left for testing and possibly for use in other ports to faster future
devices with faster CPUs.

Note: if you change sound settings AFTER loading a ROM, you may need to reset
game to get sound. This is because most games initialize sound chips on
startup, and this data is lost when sound chips are being enabled/disabled.

4. Misc:

"6 button pad" will enable 6 button gamepad emulation and will add additional
X, Y, Z and MODE actions to key configuration.
Note: if you enable this, games may detect that and use different button
configuration, for example A ("high punch") will change to "low punch" in
Mortal Kombat and you will need to bind X for "high punch".

"gzip save states" enables gzip (similar to ordinary zip, but a little
different) compression on your save states to save space. The compression
ratio is 50-90%, so it's worth to enable this.

"Use SRAM saves" option enables emulation of batery-backed save RAM some game
cartridges had. RPG games used it alot, but there were some others too, like
Sonic 3. If this is enabled, <ROMname>.srm files are generated when you exit
the emulator or load another ROM. Format is compatible with other popular
emulators (like Gens and Fusion).


5. Frameskip:

"Auto"  option tries to run the game in it's original speed by skipping next
        frame if the previous was rendered too slow.
"0"     displays every frame, thus game runs very slow.
"1"     skips every other frame. Use this for a game which is smoother, but a bit
        too slow (actually depends on display mode you use).
"2"     also makes the game smoother, but it will be too fast in most areas.
"4","8" is way too fast and is useful for skiping intros, etc.



Credits
-------

This emulator uses code from these people/projects:

Dave
Cyclone 68000 core, Pico emulation library
Homepage: http://www.finalburn.com/
E-mail: david(atsymbol)finalburn.com

notaz
UIQ port, Cyclone 68000 hacks, some additional coding (see changelog).
Homepage: http://notaz.atspace.com/
E-mail: notasas(atsymbol)gmail.com

Reesy & FluBBa
DrZ80, the Z80 emulator written in ARM assembly.
Homepage: http://reesy.gp32x.de/
E-mail: drsms_reesy(atsymbol)yahoo.co.uk

Tatsuyuki Satoh, Jarek Burczynski, MultiArcadeMachineEmulator development
software implementation of Yamaha FM sound generator

MultiArcadeMachineEmulator (MAME) development
Texas Instruments SN76489 / SN76496 programmable tone /noise generator
Homepage: http://www.mame.net/


Additional thanks
-----------------

* Peter van Sebille for ECompXL and his various open-source Symbian projects
  to learn from.
* Steve Fischer for his open-source Motorola projects.
* Charles MacDonald (http://cgfm2.emuviews.com/) for old but still very useful
  info about genesis hardware.
* Stéphane Dallongeville for creating Gens and making it open-source.
* Steve Snake for all that he has done for Genesis emulation scene.
* Bart Trzynadlowski for his SSFII and 68000 docs.
* Haze for his research (http://haze.mameworld.info).
* The development team behind "Symbian GCC Improvement Project"
  (http://www.inf.u-szeged.hu/symbian-gcc/) for their updated compile tools.
* Mark and Jean-loup for zlib library.
* Reesy for also finding some Cyclone bugs.
* Inder for the icons.


Changelog
---------
0.94
  * Improved interrupt timing, Mazin Saga and Burning Force now works.
  * Rewritten renderer code to better suit gp2x, should be faster on other
    ports too.
  + Added support for banking used by 12-in-1 and 4-in-1 ROMs (thanks Haze).
  + Added some protection device faking, used by some unlicensed games like
    Super Bubble Bobble, King of Fighters, Elf Wor, ...
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
  * Minnor sprite and layer scroll masking bugs fixed.
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
