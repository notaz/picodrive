This is yet another SEGA 8 bit and 16 bit console emulator for emulating most
of the hardware SEGA has published up to and including the 32X.


PicoDrive was originally written having ARM-based handheld devices
in mind, but later had got various improvements for other architectures
too, like SH2 recompilers for MIPS (mips32r2), ARM64 (armv8), RISC-V (RV64IM)
and PowerPC (G4/2.03).

PicoDrive was the first emulator ever to properly emulate Virtua Racing and
its SVP chip.

Currently the main development happens in the
[irixxxx's fork](https://github.com/irixxxx/picodrive),
[notaz's repo](https://github.com/notaz/picodrive) isn't updated as much.

### gallery

Some images of demos and homebrew software:

**16/32 bit systems**: Mega Drive/Genesis, Sega/Mega CD, 32X, Pico.

![Titan Overdrive 2](https://github.com/irixxxx/picodrive/assets/31696370/02a4295b-ac9d-4114-bcd1-b5dd6e5930d0)
![Raycast Demo](https://github.com/irixxxx/picodrive/assets/31696370/6e9c0bfe-49a9-45aa-bad7-544de065e388)
![OpenLara](https://github.com/irixxxx/picodrive/assets/31696370/8a00002a-5c10-4d1d-a948-739bf978282a)

![Titan Overdrive 2](https://github.com/irixxxx/picodrive/assets/31696370/2e263e81-51c8-4daa-ab16-0b2cd5554f84)
![DMA David](https://github.com/irixxxx/picodrive/assets/31696370/fbbeac15-8665-4d3e-9729-d1f8c35e417a)
![Doom Resurrection](https://github.com/irixxxx/picodrive/assets/31696370/db7b7153-b917-4850-8442-a748c2fbb968)

$~$

**8 bit systems**: SG-1000, SC-3000, Master System/Mark III, Game Gear.

![Cheril Perils Classics](https://github.com/irixxxx/picodrive/assets/31696370/653914a4-9f90-45f8-bd91-56e784df7550)
![Stygian Quest](https://github.com/irixxxx/picodrive/assets/31696370/8196801b-85c8-4d84-97e1-ae57ab3d577f)
![Wing Warriors](https://github.com/irixxxx/picodrive/assets/31696370/3c4a8f40-dad6-4fa4-b188-46b428a4b8c6)

![Nyan Cat](https://github.com/irixxxx/picodrive/assets/31696370/6fe0d38b-549d-4faa-9351-b260a89dc745)
![Anguna the Prison Dungeon](https://github.com/irixxxx/picodrive/assets/31696370/3264b962-7da2-4257-9ff7-1b509bd50cdf)
![Turrican](https://github.com/irixxxx/picodrive/assets/31696370/c4eb2f2c-806e-4f4b-ac94-5c2cda82e962)


### compiling

For platforms for which release builds are supplied the most easy way is to 
use the release script in tools/release.sh. See the release script for details.
To make a platform build use

> tools/release.sh [version] [platforms...]

This will deliver a file for each platform in a the release-[version] directory.
A list of platforms is in the release script.

If you want to build an executable for a unixoid platform not listed in the
platform list, try using

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

To compile the helix sources, set CROSS_COMPILE to your cross compiler prefix
(e.g. arm-linux-gnueabi-) and LIBGCC to your cross compiler's libgcc.a
(e.g. /usr/lib/gcc-cross/arm-linux-gnueabi/4.7/libgcc.a), and compile with

> make -C platform/common/helix CROSS_COMPILE=$CROSS_COMPILE LIBGCC=$LIBGCC

This will result in a shared library named ${CROSS_COMPILE}helix_mp3.so. Copy
this as libhelix.so to where the PicoDrive binary is on the target device.


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
