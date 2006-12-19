*update*
Use the "compress jumtable" Cyclone config.h option to fix this issue
(no patcher will be needed then).


There is a problem with Cyclone on symbian platform:
GNU as generates COFF object files, which allow max of 0xFFFF (65535) relocation
entries. Cyclone uses a jumptable of 0x10000 (65536, 1 for every opcode-word)
antries. When the executable is loaded, all jumptable entries must be relocated
to point to right code location. Because of this limitation, Cyclone's jumptable is
incomplete (misses 2 entries), and if M68k opcodes 0xFFFE or 0xFFFF are ever
encoundered when emulating, your emulator will crash.

I have written a little patcher to fix that. It writes those two missing entries and
marks them as relocatable. Placeholders must not be deleted just after the jumttable
in the Cyclone source code.

This version works with intermediate PE executable, which is used both for APPs and EXEs,
and is produced by gcc toolkit just before running petran. So it's best to insert
this in your makefile, in the rule which builds your APP/EXE, just after last 'ld'
statement, for example:

$(EPOCTRGUREL)\PICODRIVEN.APP : $(EPOCBLDUREL)\PICODRIVEN.in $(EPOCSTATLINKUREL)\EDLL.LIB $(LIBSUREL)
	...
	ld  -s -e _E32Dll -u _E32Dll --dll \
		"$(EPOCBLDUREL)\PICODRIVEN.exp" \
		-Map "$(EPOCTRGUREL)\PICODRIVEN.APP.map" -o "$(EPOCBLDUREL)\PICODRIVEN.APP" \
		"$(EPOCSTATLINKUREL)\EDLL.LIB" --whole-archive "$(EPOCBLDUREL)\PICODRIVEN.in" \
		--no-whole-archive $(LIBSUREL) $(USERLDFLAGS)
	-$(ERASE) "$(EPOCBLDUREL)\PICODRIVEN.exp"
	
	patchtable_symb2 "$(EPOCBLDUREL)\PICODRIVEN.APP"

	petran  "$(EPOCBLDUREL)\PICODRIVEN.APP" "$@" \
		 -nocall -uid1 0x10000079 -uid2 0x100039ce -uid3 0x1000c193
	-$(ERASE) "$(EPOCBLDUREL)\PICODRIVEN.APP"
	perl -S ecopyfile.pl "$@" "PICODRIVEN.APP"


This is also compatible with ECompXL.

To test if this thing worked, you can load crash_cyclone.bin in your emulator.
