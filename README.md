This is yet another Megadrive / Genesis / Sega CD / Mega CD / 32X / SMS
emulator, which was originally written having ARM-based handheld devices
in mind, but later had got various improvements for other architectures
too, like SH2 recompilers for MIPS (mips32r2), ARM64 (armv8), RISC-V (RV64IM)
and PowerPC (G4/2.03).

PicoDrive was the first emulator ever to properly emulate Virtua Racing and
its SVP chip.

Currently the main development happens in the
[irixxxx's fork](https://github.com/irixxxx/picodrive),
[notaz's repo](https://github.com/notaz/picodrive) isn't updated as much.

### compiling

For platforms for which release builds are supplied the most easy way is to 
use the release script in tools/release.sh.  The script requires toolchains
to be installed locally, depending on the platform to build. See the release
script for details. To make a platform release build use

> tools/release.sh [version] [platforms...]

This will deliver a zip files for each platform in a the release-[version]
directory.

A list of platforms for which this is possible can be obtained with

> configure --help

If you want to build an executable for a unixoid platform not listed in the
platform list, just use

> configure --platform=generic

If DRC is available for the platform, it should be enabled automatically.
For gp2x, wiz, and caanoo you may need to compile libpng first.

After configure, compile with

> make


### helix MP3 decoder for ARM

For 32 bit ARM platforms, the optimized helix MP3 decoder can be used to play
MP3 audio files with CD games. The helix source files are however not supplied
due to licensing issues. If you have legally obtained the sources, put them in
the platform/common/helix directory. 

To compile the helix sources, set CROSS to your cross compiler prefix
(e.g. arm-linux-gnueabi-) and LIBGCC to your cross compiler's libgcc.a
(e.g. /usr/lib/gcc-cross/arm-linux-gnueabi/4.7/libgcc.a), and compile with

> make -C platform/common/helix CROSS=$CROSS LIBGCC=$LIBGCC

This will result in a shared library named ${CROSS}helix_mp3.so. Copy this
as libhelix.so to where the PicoDrive binary is on the target device.


Also, the support for helix must be enabled in PicoDrive by compiling with

> make PLATFORM_MP3=1

This switch is automatically enabled for Gamepark Holdings devices (gp2x,
caanoo and wiz). Without installing libhelix.so those devices will not play
MP3 audio.


### installing

The release scripts produces zip files which need to be installed on the
target device manually. Usually that means unpacking the file to some
directory on the device storage or on an SD card. See device specific
descriptions on the net.

Send bug reports, fixes etc to <derkub@gmail.com>
