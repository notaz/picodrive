
PATH=\s60v1\epoc32\tools\;\s60v1\epoc32\gcc\bin\;C:\winnt\system32;C:\winnt;C:\winnt\System32\Wbem;C:\Program Files\ATI Technologies\ATI Control Panel;C:\Program Files\Common Files\Adaptec Shared\System;C:\Perl\bin;\s60v1\epoc32\tools;c:\MSVC6\VC98\Bin;C:\s60v1\epoc32\tools\nokia_compiler\Symbian_Tools\Command_Line_Tools;C:\Program Files\CSL Arm Toolchain\arm-none-symbianelf\bin;C:\Program Files\CSL Arm Toolchain\bin

# EPOC DEFINITIONS

EPOCBLD = \s60v1\EPOC32\BUILD\PICODRIVE\S60\PICO\WINS\ #
EPOCTRG = \s60v1\EPOC32\RELEASE\WINS\ #
EPOCLIB = \s60v1\EPOC32\RELEASE\WINS\ #
EPOCLINK = \s60v1\EPOC32\RELEASE\WINS\ #
EPOCSTATLINK = \s60v1\EPOC32\RELEASE\WINS\ #


RECREATEWORKSPACE :
	cd \PICODRIVE\S60
	perl -S makmake.pl -D \PICODRIVE\S60\PICO.MMP VC6

