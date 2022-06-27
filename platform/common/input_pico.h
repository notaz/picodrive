#ifndef INCLUDE_c48097f3ff2a6a9af1cce8fd7a9b3f0c
#define INCLUDE_c48097f3ff2a6a9af1cce8fd7a9b3f0c 1

/* gamepad - MXYZ SACB RLDU */
#define GBTN_UP         0
#define GBTN_DOWN       1
#define GBTN_LEFT       2
#define GBTN_RIGHT      3
#define GBTN_B          4
#define GBTN_C          5
#define GBTN_A          6
#define GBTN_START      7
#define GBTN_Z          8
#define GBTN_Y          9
#define GBTN_X          10
#define GBTN_MODE       11

/* ui events */
#define PEVB_VOL_DOWN   30
#define PEVB_VOL_UP     29
#define PEVB_STATE_LOAD 28
#define PEVB_STATE_SAVE 27
#define PEVB_SWITCH_RND 26
#define PEVB_SSLOT_PREV 25
#define PEVB_SSLOT_NEXT 24
#define PEVB_MENU       23
#define PEVB_FF         22
#define PEVB_PICO_PNEXT 21
#define PEVB_PICO_PPREV 20
#define PEVB_PICO_STORY 19
#define PEVB_PICO_PAD   18
#define PEVB_PICO_PENST 17
#define PEVB_PICO_SWPS2 16
#define PEVB_RESET      15

#define PEV_VOL_DOWN    (1 << PEVB_VOL_DOWN)
#define PEV_VOL_UP      (1 << PEVB_VOL_UP)
#define PEV_STATE_LOAD  (1 << PEVB_STATE_LOAD)
#define PEV_STATE_SAVE  (1 << PEVB_STATE_SAVE)
#define PEV_SWITCH_RND  (1 << PEVB_SWITCH_RND)
#define PEV_SSLOT_PREV  (1 << PEVB_SSLOT_PREV)
#define PEV_SSLOT_NEXT  (1 << PEVB_SSLOT_NEXT)
#define PEV_MENU        (1 << PEVB_MENU)
#define PEV_FF          (1 << PEVB_FF)
#define PEV_PICO_PNEXT  (1 << PEVB_PICO_PNEXT)
#define PEV_PICO_PPREV  (1 << PEVB_PICO_PPREV)
#define PEV_PICO_STORY  (1 << PEVB_PICO_STORY)
#define PEV_PICO_PAD    (1 << PEVB_PICO_PAD)
#define PEV_PICO_PENST  (1 << PEVB_PICO_PENST)
#define PEV_PICO_SWPS2  (1 << PEVB_PICO_SWPS2)
#define PEV_RESET       (1 << PEVB_RESET)

#define PEV_MASK 0x7fff8000

/* Keyboard Pico */

// Blue buttons
#define PEVB_PICO_PS2_SPACE 0x29
#define PEVB_PICO_PS2_EXCLAIM 0
#define PEVB_PICO_PS2_QUOTEDBL 0
#define PEVB_PICO_PS2_HASH 0
#define PEVB_PICO_PS2_DOLLAR 0
#define PEVB_PICO_PS2_AMPERSAND 0
#define PEVB_PICO_PS2_LEFTPAREN 0
#define PEVB_PICO_PS2_RIGHTPAREN 0
#define PEVB_PICO_PS2_ASTERISK 0x7c
#define PEVB_PICO_PS2_PLUS 0x79
#define PEVB_PICO_PS2_MINUS 0x4e
#define PEVB_PICO_PS2_COMMA 0x41
#define PEVB_PICO_PS2_PERIOD 0x49
#define PEVB_PICO_PS2_SLASH 0x4a
#define PEVB_PICO_PS2_1 0x16
#define PEVB_PICO_PS2_2 0x1e
#define PEVB_PICO_PS2_3 0x26
#define PEVB_PICO_PS2_4 0x25
#define PEVB_PICO_PS2_5 0x2e
#define PEVB_PICO_PS2_6 0x36
#define PEVB_PICO_PS2_7 0x3d
#define PEVB_PICO_PS2_8 0x3e
#define PEVB_PICO_PS2_9 0x46
#define PEVB_PICO_PS2_0 0x45
#define PEVB_PICO_PS2_COLON 0x52
#define PEVB_PICO_PS2_SEMICOLON 0x4c
#define PEVB_PICO_PS2_LESS 0
#define PEVB_PICO_PS2_EQUALS 0x55
#define PEVB_PICO_PS2_GREATER 0
#define PEVB_PICO_PS2_QUESTION 0
#define PEVB_PICO_PS2_AT 0
#define PEVB_PICO_PS2_DAKUTEN 0x54 // ゛
#define PEVB_PICO_PS2_LEFTBRACKET 0x5b
#define PEVB_PICO_PS2_RIGHTBRACKET 0x5d
#define PEVB_PICO_PS2_CARET 0
#define PEVB_PICO_PS2_UNDERSCORE 0
#define PEVB_PICO_PS2_YEN 0x6a // ￥
#define PEVB_PICO_PS2_RO 0x51 // ろ
#define PEVB_PICO_PS2_KE 0x52 // け

#define PEVB_PICO_PS2_a 0x1c
#define PEVB_PICO_PS2_b 0x32
#define PEVB_PICO_PS2_c 0x21
#define PEVB_PICO_PS2_d 0x23
#define PEVB_PICO_PS2_e 0x24
#define PEVB_PICO_PS2_f 0x2b
#define PEVB_PICO_PS2_g 0x34
#define PEVB_PICO_PS2_h 0x33
#define PEVB_PICO_PS2_i 0x43
#define PEVB_PICO_PS2_j 0x3b
#define PEVB_PICO_PS2_k 0x42
#define PEVB_PICO_PS2_l 0x4b
#define PEVB_PICO_PS2_m 0x3a
#define PEVB_PICO_PS2_n 0x31
#define PEVB_PICO_PS2_o 0x44
#define PEVB_PICO_PS2_p 0x4d
#define PEVB_PICO_PS2_q 0x15
#define PEVB_PICO_PS2_r 0x2d
#define PEVB_PICO_PS2_s 0x1b
#define PEVB_PICO_PS2_t 0x2c
#define PEVB_PICO_PS2_u 0x3c
#define PEVB_PICO_PS2_v 0x2a
#define PEVB_PICO_PS2_w 0x1d
#define PEVB_PICO_PS2_x 0x22
#define PEVB_PICO_PS2_y 0x35
#define PEVB_PICO_PS2_z 0x1a

// Green button on top-left
#define PEVB_PICO_PS2_ESCAPE 0x76

// Orange buttons on left
#define PEVB_PICO_PS2_CAPSLOCK 0x58
#define PEVB_PICO_PS2_LSHIFT 0x12

// Green buttons on right
#define PEVB_PICO_PS2_BACKSPACE 0x66
#define PEVB_PICO_PS2_INSERT 0x81
#define PEVB_PICO_PS2_DELETE 0x85

// Red button on bottom-right
#define PEVB_PICO_PS2_RETURN 0x5a

// Orange buttons on bottom
#define PEVB_PICO_PS2_SOUND 0x67
#define PEVB_PICO_PS2_HOME 0x64
#define PEVB_PICO_PS2_CJK 0x13
#define PEVB_PICO_PS2_ROMAJI 0x17

#endif /* INCLUDE_c48097f3ff2a6a9af1cce8fd7a9b3f0c */
