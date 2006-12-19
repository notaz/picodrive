

Compiling Symbian ports
-----------------------
First note: there is no WINS support, because I use assembly code in some
places and I don't think emulator is useful for this project because of it's
too different behaviour compared to real device. If you need wins support,
you will have to add it yourself. In that case, you will have to use a68k
instead of Cyclone and mz80 instead of DrZ80. Emulator does have support
for these, also there is equivalent c code for the asm stuff, but you will
have to dig the sources and figure everything out yourself.

Exe:
Before building the exe you will need to compile Cyclone, use the included
vc6 project or see Cyclone.txt for more info.

I don't use standard Symbian build methods for exe, because I use ARM
assembly files (*.s) and different compiler options for different files,
and this is difficult to achieve with .mmp files. Also I use Cylone
patcher, check \cyclone\epoc\ for more info. So I use custom makefile
instead, but to use it, UIQROOT2 environmental variable must be set to
the root of your UIQ SDK. So to build, you need to type something like
this in console window:
  > SET UIQROOT2=C:\UIQ_21\
  > make -f picosmall.armi noecompxl=1
(To use ECompXL, you must rename ECompXL's petran.exe to petran_.exe or
edit the makefile).

Launcher:
There should be far less problems building that. Simply typing
  > makmake PicodriveN.mmp armi
  > make -f PicodriveN.armi
in console window with launcher directory set should build it without
problems.


Compiling GP2X port
-------------------
If you use devkitGP2X with gcc 4.0.2 and your paths are set correctly, running
'make' should be enough. If you are under Linux, you may need do some
adjustments, like changing case of filenames or setting up paths. I am sure
you will figure out yourself :)


License mess
------------
The launcher for Symbian OS is based on Peter van Sebille's projects,
which are released under GPL (license_gpl.txt).

YM2612 and sn76496 sound cores were taken from the MAME project, which is
under it's own license (license_mame.txt).

Dave's Cyclone 68000 core, Pico library are under simple
"Free for non-commercial use, For commercial use, separate licencing
terms must be obtained" license.

As far as I checked, both "Free for non-commercial use" and MAME licenses
might be incompatible with GPL, because GPL DOES allow commercial distribution.
But I don't think the original copyright holders (Peter, Dave, Reesy or the
MAME devs) would get very upset about this "violation", as this is both free
and open-source project. However, the whole project most likely falls under
GPL now (I don't know for sure as I'm just another coder, not a lawyer).
Alternatively, the launcher and exe can be viewed as separate programs
(technically this is true, they both use separate binaries, only protocol
is shared), so I hope nobody sees a problem here.


Contact
-------
My e-mail: notasas(atsymbol)gmail.com
