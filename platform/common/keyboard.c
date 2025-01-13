#include "input_pico.h"

// keyboard description
struct key {
	int xpos;
	char *lower, *upper;
	int key;
};

// pico
static struct key kbd_pico_row1[] = {
	{  0, "esc", "esc",	PEVB_KBD_ESCAPE },
	{  4, "1", "!",		PEVB_KBD_1 },
	{  7, "2", "\"",	PEVB_KBD_2 },
	{ 10, "3", "#",		PEVB_KBD_3 },
	{ 13, "4", "$",		PEVB_KBD_4 },
	{ 16, "5", "%",		PEVB_KBD_5 },
	{ 19, "6", "&",		PEVB_KBD_6 },
	{ 22, "7", "'",		PEVB_KBD_7 },
	{ 25, "8", "(",		PEVB_KBD_8 },
	{ 28, "9", ")",		PEVB_KBD_9 },
	{ 31, "0", "0",		PEVB_KBD_0 },
	{ 34, "-", "=",		PEVB_KBD_MINUS },
	{ 37, "^", "~",		PEVB_KBD_CARET },
	{ 40, "Y", "|",		PEVB_KBD_YEN },
	{ 43, "bs", "bs",	PEVB_KBD_BACKSPACE },
	{ 0 },
};
static struct key kbd_pico_row2[] = {
	{  5, "q", "Q",		PEVB_KBD_q },
	{  8, "w", "W",		PEVB_KBD_w },
	{ 11, "e", "E",		PEVB_KBD_e },
	{ 14, "r", "R",		PEVB_KBD_r },
	{ 17, "t", "T",		PEVB_KBD_t },
	{ 20, "y", "Y",		PEVB_KBD_y },
	{ 23, "u", "U",		PEVB_KBD_u },
	{ 26, "i", "I",		PEVB_KBD_i },
	{ 29, "o", "O",		PEVB_KBD_o },
	{ 32, "p", "P",		PEVB_KBD_p },
	{ 35, "@", "`",		PEVB_KBD_AT },
	{ 38, "[", "{",		PEVB_KBD_LEFTBRACKET },
	{ 43, "ins", "ins",	PEVB_KBD_INSERT },
	{ 0 },
};
static struct key kbd_pico_row3[] = {
	{  0, "caps", "caps",	PEVB_KBD_CAPSLOCK },
	{  6, "a", "A",		PEVB_KBD_a },
	{  9, "s", "S",		PEVB_KBD_s },
	{ 12, "d", "D",		PEVB_KBD_d },
	{ 15, "f", "F",		PEVB_KBD_f },
	{ 18, "g", "G",		PEVB_KBD_g },
	{ 21, "h", "H",		PEVB_KBD_h },
	{ 24, "j", "J",		PEVB_KBD_j },
	{ 27, "k", "K",		PEVB_KBD_k },
	{ 30, "l", "L",		PEVB_KBD_l },
	{ 33, ";", "+",		PEVB_KBD_SEMICOLON },
	{ 36, ":", "*",		PEVB_KBD_COLON },
	{ 39, "]", "}",		PEVB_KBD_RIGHTBRACKET },
	{ 43, "del", "del",	PEVB_KBD_DELETE },
	{ 0 },
};
static struct key kbd_pico_row4[] = {
	{  0, "shift", "shift",	PEVB_KBD_SHIFT },
	{  7, "z", "Z",		PEVB_KBD_z },
	{ 10, "x", "X",		PEVB_KBD_x },
	{ 13, "c", "C",		PEVB_KBD_c },
	{ 16, "v", "V",		PEVB_KBD_v },
	{ 19, "b", "B",		PEVB_KBD_b },
	{ 22, "n", "N",		PEVB_KBD_n },
	{ 25, "m", "M",		PEVB_KBD_m },
	{ 28, ",", "<",		PEVB_KBD_COMMA },
	{ 31, ".", ">",		PEVB_KBD_PERIOD },
	{ 34, "/", "?",		PEVB_KBD_SLASH },
	{ 37, "_", "_",		PEVB_KBD_RO },
	{ 41, "enter", "enter",	PEVB_KBD_RETURN },
	{ 0 },
};
static struct key kbd_pico_row5[] = {
	{  0, "muhenkan", "muhenkan",	PEVB_KBD_SOUND },
	{ 13, "space", "space",		PEVB_KBD_SPACE },
	{ 22, "henkan", "henkan",	PEVB_KBD_HOME },
	{ 29, "kana", "kana",		PEVB_KBD_CJK },
	{ 34, "romaji", "romaji",	PEVB_KBD_ROMAJI },
	{ 0 },
};
static struct key *kbd_pico[] =
	{ kbd_pico_row1, kbd_pico_row2, kbd_pico_row3, kbd_pico_row4, kbd_pico_row5, NULL };


//sc3000
static struct key kbd_sc3000_row1[] = {
	{  4, "1", "!",		PEVB_KBD_1 },
	{  7, "2", "\"",	PEVB_KBD_2 },
	{ 10, "3", "#",		PEVB_KBD_3 },
	{ 13, "4", "$",		PEVB_KBD_4 },
	{ 16, "5", "%",		PEVB_KBD_5 },
	{ 19, "6", "&",		PEVB_KBD_6 },
	{ 22, "7", "'",		PEVB_KBD_7 },
	{ 25, "8", "(",		PEVB_KBD_8 },
	{ 28, "9", ")",		PEVB_KBD_9 },
	{ 31, "0", "0",		PEVB_KBD_0 },
	{ 34, "-", "=",		PEVB_KBD_MINUS },
	{ 37, "^", "~",		PEVB_KBD_CARET },
	{ 40, "Y", "|",		PEVB_KBD_YEN },
	{ 49, "brk", "brk",	PEVB_KBD_ESCAPE },
	{ 0 },
};
static struct key kbd_sc3000_row2[] = {
	{  0, "func", "func",	PEVB_KBD_FUNC },
	{  5, "q", "Q",		PEVB_KBD_q },
	{  8, "w", "W",		PEVB_KBD_w },
	{ 11, "e", "E",		PEVB_KBD_e },
	{ 14, "r", "R",		PEVB_KBD_r },
	{ 17, "t", "T",		PEVB_KBD_t },
	{ 20, "y", "Y",		PEVB_KBD_y },
	{ 23, "u", "U",		PEVB_KBD_u },
	{ 26, "i", "I",		PEVB_KBD_i },
	{ 29, "o", "O",		PEVB_KBD_o },
	{ 32, "p", "P",		PEVB_KBD_p },
	{ 35, "@", "`",		PEVB_KBD_AT },
	{ 38, "[", "{",		PEVB_KBD_LEFTBRACKET },
	{ 50, "^", "^",		PEVB_KBD_UP },
	{ 0 },
};
static struct key kbd_sc3000_row3[] = {
	{  0, "ctrl", "ctrl",	PEVB_KBD_CTRL },
	{  6, "a", "A",		PEVB_KBD_a },
	{  9, "s", "S",		PEVB_KBD_s },
	{ 12, "d", "D",		PEVB_KBD_d },
	{ 15, "f", "F",		PEVB_KBD_f },
	{ 18, "g", "G",		PEVB_KBD_g },
	{ 21, "h", "H",		PEVB_KBD_h },
	{ 24, "j", "J",		PEVB_KBD_j },
	{ 27, "k", "K",		PEVB_KBD_k },
	{ 30, "l", "L",		PEVB_KBD_l },
	{ 33, ";", "+",		PEVB_KBD_SEMICOLON },
	{ 36, ":", "*",		PEVB_KBD_COLON },
	{ 39, "]", "}",		PEVB_KBD_RIGHTBRACKET },
	{ 41, "enter", "enter",	PEVB_KBD_RETURN },
	{ 49, "<", "<",		PEVB_KBD_LEFT },
	{ 51, ">", ">",		PEVB_KBD_RIGHT },
	{ 0 },
};
static struct key kbd_sc3000_row4[] = {
	{  0, "shift", "shift",	PEVB_KBD_SHIFT },
	{  7, "z", "Z",		PEVB_KBD_z },
	{ 10, "x", "X",		PEVB_KBD_x },
	{ 13, "c", "C",		PEVB_KBD_c },
	{ 16, "v", "V",		PEVB_KBD_v },
	{ 19, "b", "B",		PEVB_KBD_b },
	{ 22, "n", "N",		PEVB_KBD_n },
	{ 25, "m", "M",		PEVB_KBD_m },
	{ 28, ",", "<",		PEVB_KBD_COMMA },
	{ 31, ".", ">",		PEVB_KBD_PERIOD },
	{ 34, "/", "?",		PEVB_KBD_SLASH },
	{ 37, "pi", "pi",	PEVB_KBD_RO },
	{ 41, "shift", "shift",	PEVB_KBD_SHIFT },
	{ 50, "v", "v",		PEVB_KBD_DOWN },
	{ 0 },
};
static struct key kbd_sc3000_row5[] = {
	{  8, "graph", "graph",	PEVB_KBD_SOUND },
	{ 14, "kana", "kana",	PEVB_KBD_CJK },
	{ 22, "space", "space",	PEVB_KBD_SPACE },
	{ 31, "clr", "home",	PEVB_KBD_HOME },
	{ 36, "del", "ins",	PEVB_KBD_BACKSPACE },
	{ 0 },
};

static struct key *kbd_sc3000[] =
	{ kbd_sc3000_row1, kbd_sc3000_row2, kbd_sc3000_row3, kbd_sc3000_row4, kbd_sc3000_row5, NULL };


static void kbd_draw(struct key *desc[], int shift, int xoffs, int yoffs, struct key *hi)
{
	int i, j;
	struct key *key;

	for (i = 0; desc[i]; i++) {
		for (j = 0, key = &desc[i][j]; key->lower; j++, key++) {
			int color = (key != hi ? PXMAKE(0xa0, 0xa0, 0xa0) :
						 PXMAKE(0xff, 0xff, 0xff));
			char *text = (shift ? key->upper : key->lower);
			smalltext_out16(xoffs + key->xpos*me_sfont_w, yoffs + i*me_sfont_h, text, color);
		}
	}
}
