#! /bin/bash
#
# picodrive release build script
#
# creates builds for the supported platforms in the release directory
#
# usage: release.sh <version> [platform...]
#	platforms:	gph dingux retrofw gcw0 rg350 miyoo psp pandora odbeta-gcw0 odbeta-lepus
#
# expects toolchains to be installed in these docker containers:
#	gph:		ghcr.io/irixxxx/toolchain-gp2x
#	dingux:		ghcr.io/irixxxx/toolchain-dingux
#	retrofw:	ghcr.io/irixxxx/toolchain-retrofw
#	gcw0, rg350:	ghcr.io/irixxxx/toolchain-opendingux
#	miyoo:		miyoocfw/toolchain
#	psp:		ghcr.io/pspdev/pspdev
#	odbeta-gcw0:	ghcr.io/irixxxx/toolchain-odbeta-gcw0
#	odbeta-lepus:	ghcr.io/irixxxx/toolchain-odbeta-lepus

trap "exit" ERR

rel=$1
mkdir -p release-$rel

shift; plat=" $* "
[ -z "$(echo $plat|tr -d ' ')" ] && plat=" gph dingux retrofw gcw0 rg350 miyoo psp "


[ -z "${plat##* gph *}" ] && {
# GPH devices: gp2x, wiz, caanoo, with ubuntu arm gcc 4.7
docker pull ghcr.io/irixxxx/toolchain-gp2x
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=gp2x &&\
	make clean && make -j2 all &&\
	make -C platform/gp2x rel VER=$rel "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-gp2x sh &&
mv PicoDrive_$rel.zip release-$rel/PicoDrive-gph_$rel.zip
}

[ -z "${plat##* dingux *}" ] && {
# dingux: dingoo a320, ritmix rzx-50, JZ4755 or older (mips32r1 w/o fpu)
# NB works for legacy dingux and possibly opendingux before gcw0
docker pull ghcr.io/irixxxx/toolchain-dingux
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=dingux &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-dingux sh &&
mv PicoDrive-dge.zip release-$rel/PicoDrive-dge_$rel.zip
}

[ -z "${plat##* retrofw *}" ] && {
# retrofw: rs-97 and similar, JZ4760 (mips32r1 with fpu)
docker pull ghcr.io/irixxxx/toolchain-retrofw
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=retrofw &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-retrofw sh &&
mv PicoDrive.opk release-$rel/PicoDrive-retrofw_$rel.opk
}

[ -z "${plat##* gcw0 *}" ] && {
# gcw0: JZ4770 (mips32r2 with fpu), swapped X/Y buttons
docker pull ghcr.io/irixxxx/toolchain-opendingux
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=gcw0 &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-opendingux sh &&
mv PicoDrive.opk release-$rel/PicoDrive-gcw0_$rel.opk
}

[ -z "${plat##* rg350 *}" ] && {
# rg350, gkd350h etc: JZ4770 or newer
docker pull ghcr.io/irixxxx/toolchain-opendingux
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=opendingux &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-opendingux sh &&
mv PicoDrive.opk release-$rel/PicoDrive-opendingux_$rel.opk
}

[ -z "${plat##* miyoo *}" ] && {
# miyoo: BittBoy >=v1, PocketGo, Powkiddy [QV]90/Q20 (Allwinner F1C100s, ARM926)
docker pull miyoocfw/toolchain
echo "	export CROSS_COMPILE=arm-buildroot-linux-musleabi- &&\
	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=miyoo &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm miyoocfw/toolchain sh &&
mv PicoDrive.zip release-$rel/PicoDrive-miyoo_$rel.zip
}

[ -z "${plat##* psp *}" ] && {
# psp (experimental), pspdev SDK toolchain
docker pull --platform=linux/amd64 ghcr.io/pspdev/pspdev
echo "	apk add git gcc g++ zip && export CROSS_COMPILE=psp- &&\
	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=psp &&\
	make clean && make -j2 all &&\
	make -C platform/psp rel VER=$rel "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/pspdev/pspdev sh &&
mv PicoDrive_psp_$rel.zip release-$rel/PicoDrive-psp_$rel.zip
}

[ -z "${plat##* pandora *}" ] && {
# pandora (untested), openpandora SDK toolchain
docker pull ghcr.io/irixxxx/toolchain-pandora
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=pandora &&\
	make clean && make -j2 all &&\
	\${CROSS_COMPILE}strip -o PicoDrive-pandora-$rel PicoDrive"\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-pandora sh &&
mv PicoDrive-pandora-$rel release-$rel/
}

[ -z "${plat##* odbeta-gcw0 *}" ] && {
# gcw0 (untested): JZ4770 (mips32r2 with fpu), swapped X/Y buttons
docker pull ghcr.io/irixxxx/toolchain-odbeta-gcw0
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=gcw0 &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-odbeta-gcw0 sh &&
mv PicoDrive.opk release-$rel/PicoDrive-odbeta-gcw0_$rel.opk
}

[ -z "${plat##* odbeta-lepus *}" ] && {
# lepus (untested): JZ4760 (mips32r1 with fpu)
docker pull ghcr.io/irixxxx/toolchain-odbeta-lepus
echo "	git config --global --add safe.directory /home/picodrive &&\
	./configure --platform=opendingux &&\
	make clean && make -j2 all "\
  | docker run -i -v$PWD:/home/picodrive -w/home/picodrive --rm ghcr.io/irixxxx/toolchain-odbeta-lepus sh &&
mv PicoDrive.opk release-$rel/PicoDrive-odbeta-lepus_$rel.opk
}
