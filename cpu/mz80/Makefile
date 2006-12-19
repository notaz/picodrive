CFLAGS = -Wno-conversion -Wno-sign-compare # -Wno-pointer-sign

all : mz80.asm

mz80.asm : makez80
	./makez80 -s -l -x86 $@

makez80 : makez80.o


clean :
	$(RM) makez80 makez80.o mz80.asm

