#! /bin/bash
#
# picodrive release build script
#
# creates builds for the supported platforms in the release directory
#
# usage: release.sh <version>
#
# expects toolchains to be installed in $HOME/opt:
#	gph:		arm-open2x-linux
# 			needs additional libpng, ATM in src/gp2x/armroot/lib
#	dingux:		opendingux-toolchain (the old 2012 version)
#	retrofw:	mipsel-linux-uclibc (the retrofw toolchain)
#	gcw0:		gcw0-toolchain
#	rg350:		rg350-toolchain (+ mips-toolchain for newer gcc)
#	psp:		pspdev

mkdir -p release-$1
trap "exit" ERR

# GPH devices: gp2x, wiz, caanoo, with ubuntu arm gcc 4.7
# note that -msoft-float has a changed float ABI with gcc >= 4.8...
TC=$HOME/opt/open2x/gcc-4.1.1-glibc-2.3.6 CROSS_COMPILE=arm-linux-gnueabi- CFLAGS="-I$TC/arm-open2x-linux/include -I$HOME/src/gp2x/armroot/include -U_FORTIFY_SOURCE" LDFLAGS="-B$TC/lib/gcc/arm-open2x-linux/4.1.1 -B$TC/arm-open2x-linux/lib -L$TC/arm-open2x-linux/lib -L$HOME/src/gp2x/armroot/lib" ./configure --platform=gp2x
make clean all
make -C platform/gp2x rel VER=$1
mv PicoDrive_$1.zip release-$1/PicoDrive-gph_$1.zip

# dingux: dingoo a320, ritmix rzx-50, JZ4755 or older (mips32r1 w/o fpu)
# NB works for legacy dingux and possibly opendingux before gcw0
TC=$HOME/opt/opendingux-toolchain PATH=$TC/usr/bin:$PATH CROSS_COMPILE=mipsel-linux- CFLAGS="-I $TC/usr/include -I $TC/usr/include/SDL -Wno-unused-result -mabicalls" LDFLAGS="--sysroot $TC" ./configure --platform=dingux
TC=$HOME/opt/opendingux-toolchain PATH=$TC/usr/bin:$PATH make clean all
mv PicoDrive-dge.zip release-$1/PicoDrive-dge_$1.zip

# retrofw: rs-97 and similar, JZ4760 (mips32r1 with fpu)
TC=$HOME/opt/mipsel-linux-uclibc PATH=$TC/bin:$PATH CROSS_COMPILE=mipsel-linux- CFLAGS="-I $TC/include -I $TC/include/SDL -Wno-unused-result" LDFLAGS="--sysroot $TC/mipsel-buildroot-linux-uclibc/sysroot" ./configure --platform=retrofw
TC=$HOME/opt/mipsel-linux-uclibc PATH=$TC/bin:$PATH make clean all
mv PicoDrive.opk release-$1/PicoDrive-retrofw_$1.opk

# gcw0: JZ4770 (mips32r2 with fpu), swapped X/Y buttons, SDK toolchain
#TC=$HOME/opt/gcw0-toolchain PATH=$TC/usr/bin:$PATH CROSS_COMPILE=mipsel-linux- TS=$TC/usr/mipsel-gcw0-linux-uclibc/sysroot CFLAGS="-I$TS/usr/include -I$TS/usr/include/SDL" LDFLAGS="--sysroot $TS" ./configure --platform=gcw0
#TC=$HOME/opt/gcw0-toolchain PATH=$TC/usr/bin:$PATH make clean all

# gcw0: JZ4770 (mips32r2 with fpu), swapped X/Y buttons, newer generic gcc
TC=$HOME/opt/gcw0-toolchain PATH=$HOME/opt/mips-toolchain/bin:$PATH CROSS_COMPILE=mipsel-linux- TS=$TC/usr/mipsel-gcw0-linux-uclibc/sysroot CFLAGS="-I$TS/usr/include -I$TS/usr/include/SDL -mabicalls" LDFLAGS="--sysroot $TS -Wl,--dynamic-linker=/lib/ld-uClibc.so.0" ./configure --platform=gcw0
PATH=$HOME/opt/mips-toolchain/usr/bin:$PATH make clean all
mv PicoDrive.opk release-$1/PicoDrive-gcw0_$1.opk

# rg350: JZ4770, SDK toolchain
#TC=$HOME/opt/rg350-toolchain PATH=$TC/usr/bin:$PATH CROSS_COMPILE=mipsel-linux- TS=$TC/usr/mipsel-gcw0-linux-uclibc/sysroot CFLAGS="-I$TS/usr/include -I$TS/usr/include/SDL" LDFLAGS="--sysroot $TS" ./configure --platform=rg350
#TC=$HOME/opt/rg350-toolchain PATH=$TC/usr/bin:$PATH make clean all

# rg350, gkd350h etc: JZ4770 or newer, newer generic gcc
# NB this may or may not work on the newer opendingux beta. undefine frame_info
# symbols to force linking them from libgcc.a - they might not be in the libc
TC=$HOME/opt/rg350-toolchain PATH=$HOME/opt/mips-toolchain/bin:$PATH CROSS_COMPILE=mipsel-linux- TS=$TC/usr/mipsel-gcw0-linux-uclibc/sysroot CFLAGS="-I$TS/usr/include -I$TS/usr/include/SDL -mabicalls" LDFLAGS="--sysroot $TS -Wl,--dynamic-linker=/lib/ld-uClibc.so.0 -u__register_frame_info -u__deregister_frame_info" ./configure --platform=opendingux
PATH=$HOME/opt/mips-toolchain/usr/bin:$PATH make clean all
mv PicoDrive.opk release-$1/PicoDrive-opendingux_$1.opk

# psp (experimental), pspdev SDK toolchain
TC=$HOME/opt/pspdev PATH=$TC/bin:$PATH CROSS_COMPILE=psp- CFLAGS="-I$TC/psp/sdk/include -D_POSIX_C_SOURCE=199506L" LDFLAGS="-L$TC/psp/sdk/lib" ./configure --platform=psp
TC=$HOME/opt/pspdev PATH=$TC/bin:$PATH make clean all
make -C platform/psp rel VER=$1
mv PicoDrive_psp_$1.zip release-$1/PicoDrive-psp_$1.zip
