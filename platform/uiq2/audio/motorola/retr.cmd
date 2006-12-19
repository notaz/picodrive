copy %EPOCROOT%\epoc32\release\armi\urel\audio_motorola.dll ..\
..\..\..\qconsole-1.52\qtty\release\qtty --qc-addr P800 --qc-channel 5 --user qconsole --pass server --cmds "put d:\system\apps\picodriven\audio_motorola.dll ..\audio_motorola.dll" exit
