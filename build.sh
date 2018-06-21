make -f Makefile.libretro clean
make -f Makefile.libretro platform=windows_msvc2017_desktop_x64 DEBUG=1
cp -f *.dll /C/Users/Alberto/Downloads/RetroArch_x64/cores