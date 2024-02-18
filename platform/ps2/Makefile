
# ----------- release -----------
ifneq ($(findstring rel,$(MAKECMDGOALS)),)
ifeq ($(VER),)
$(error need VER)
endif
endif

../../tools/textfilter: ../../tools/textfilter.c
	make -C ../../tools/ textfilter

readme.txt: ../../tools/textfilter ../base_readme.txt ../../ChangeLog
	../../tools/textfilter ../base_readme.txt $@ PS2

# ?
rel: ../../PicoDrive readme.txt ../game_def.cfg
	mkdir -p PicoDrive/skin/
	cp $^ PicoDrive/
	cp ../../skin/* PicoDrive/skin/
	zip -9 -r ../../PicoDrive_ps2_$(VER).zip PicoDrive
	rm -rf PicoDrive
	mkdir bin_to_cso_mp3
	cp ../../tools/bin_to_cso_mp3/* bin_to_cso_mp3/
	zip -9 -r ../../PicoDrive_ps2_$(VER).zip bin_to_cso_mp3
	rm -rf bin_to_cso_mp3


