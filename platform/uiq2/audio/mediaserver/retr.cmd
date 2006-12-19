copy %EPOCROOT%\epoc32\release\armi\urel\audio_mediaserver.dll ..\
..\..\..\qconsole-1.52\qtty\release\qtty --qc-addr P800 --qc-channel 5 --user qconsole --pass server --cmds "put d:\system\apps\picodriven\audio_mediaserver.dll ..\audio_mediaserver.dll" exit
