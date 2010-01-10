@ vim:filetype=armasm

@ .equiv START_ROW,         1
@ .equiv END_ROW,          27
@ one row means 8 pixels. If above example was used, (27-1)*8=208 lines would be rendered.
.equiv START_ROW,               0
.equiv END_ROW,                28

.equiv UNALIGNED_DRAWLINEDEST,  0

@ this should be set to one only for GP2X port
.equiv EXTERNAL_YM2612,         0

@ also update VID_BORDER_R in port_config.h
.equiv BORDER_R,		16
