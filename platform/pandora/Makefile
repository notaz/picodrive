# release packaging makefile

VER ?= $(shell head -n 1 ../common/version.h | \
	sed 's/.*"\(.*\)\.\(.*\)".*/\1\2/g')

PND_MAKE ?= $(HOME)/dev/pnd/src/pandora-libraries/testdata/scripts/pnd_make.sh

all: rel

../../tools/textfilter:
	make -C ../../tools/

/tmp/readme.txt: ../../tools/textfilter ../base_readme.txt
	../../tools/textfilter ../base_readme.txt $@ PANDORA

/tmp/PicoDrive.pxml: PicoDrive.pxml.template
	./make_pxml.sh $^ $@

rel: ../../PicoDrive PicoDrive.sh picorestore \
	PicoDrive.png PicoDrive_p.png \
	../../pico/carthw.cfg skin \
	/tmp/readme.txt /tmp/PicoDrive.pxml
	rm -rf out
	mkdir out
	cp -r $^ out/
	$(PND_MAKE) -p PicoDrive_$(VER).pnd -d out -x out/PicoDrive.pxml -i out/PicoDrive.png -c

