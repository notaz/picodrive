
About
-----

This is a quick windows port of PicoDrive, a Megadrive / Genesis emulator for
handheld devices. It was originally coded having ARM CPU based devices in mind
(most work was done on GP2X version), but there is also a PSP port.

The sole purpose of this port is to demonstrate my SVP and Sega Pico emulation
code. This makes it one of the most minimal emulators out there. If you need
more features, you will have to wait until support is integrated in Kega,
Gens and the likes, as this emu was not meant to compete with them.

For more info, visit http://notaz.gp2x.de/svp.php


Releases
--------

1.40  - first release.
1.40a - Tasco Deluxe's dithering fix.
1.40b - Perspective fix thanks to Pierpaolo Prazzoli's info.
1.45  - Added preliminary Sega Pico emulation.


Controls
--------

These are currently hardcoded, keyboard only:

PC      Gen/MD      Sega Pico
-------+-----------+---------
Enter:  Start
Z:      A
X:      B           red button
C:      C           pen push
TAB:            (reset)
Esc:           (load ROM)
Arrows:          D-pad


Credits
-------

A lot of work on making SVP emulation happen was done by Tasco Deluxe, my
stuff is a continuation of his. Pierpaolo Prazzoli's information and his
SSP1610 disassembler in MAME code helped a lot too.

The original PicoDrive was written by fDave from finalburn.com

This PicoDrive version uses bits and pieces of from other projects:

68k: FAME/C core, by Chui and Stéphane Dallongeville (as C68K).
z80: CZ80 by Stéphane Dallongeville and modified by NJ.
YM2612 and SN76496 cores: MAME devs.

Special thanks to Rokas and Lordus for various ideas.

Greets to all the sceners and emu authors out there!

