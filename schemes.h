#ifndef __SCHEMES_H__
#define __SCHEMES_H__

#include "viiper.h" /* for enum items */

#define BORDER_T 0
#define BORDER_C 1
#define BORDER_B 2
#define BORDER_S 3
#define BORDER_L 0
#define BORDER_R 2
#define BORDER(v,h) op.scheme->border[BORDER_ ## v][BORDER_ ## h]

struct scheme {
	char* border[4][3];

	char* snake[5][5]; /* [predecessor][successor] */

	char* item[NUM_FOODS];

	/* for en-/disabling e.g. DEC charset: */
	char* init_seq;
	char* reset_seq;

	int cell_width;
};

struct scheme unic0de = {
	.border = {
		{"╔═", "══", "═╗"},
		{"║ ", "  ", " ║"},
		{"╚═", "══", "═╝"},
		{   "╡","","╞"   },
	},

	.snake = { /* sorted like in the enum directions */
		{/* NONE -> */
			/*NONE */ "",
			/*NORTH*/ "⢿⡿",
			/*EAST */ "⢾⣿",
			/*SOUTH*/ "⣾⣷",
			/*WEST */ "⣿⡷",
		},{/* NORTH -> */
			/*NONE */ "⢇⡸",
			/*NORTH*/ "",
			/*EAST */ "⢇⣈",
			/*SOUTH*/ "⡇⢸",
			/*WEST */ "⣁⡸",
		},{/* EAST -> */
			/*NONE */ "⢎⣉",
			/*NORTH*/ "⢇⣈",
			/*EAST */ "",
			/*SOUTH*/ "⡎⢉",
			/*WEST */ "⣉⣉",
		},{/* SOUTH -> */
			/*NONE */ "⡎⢱",
			/*NORTH*/ "⡇⢸",
			/*EAST */ "⡎⢉",
			/*SOUTH*/ "",
			/*WEST */ "⡉⢱",
		},{/* WEST -> */
			/*NONE */ "⣉⡱",
			/*NORTH*/ "⣁⡸",
			/*EAST */ "⣉⣉",
			/*SOUTH*/ "⡉⢱",
			/*WEST */ "",
		},
	},

	.item = {
		[FOOD_5] = "🍐",
		[FOOD_10] = "🍎",
		[FOOD_20] = "🥑",
	},

	.cell_width = 2,
};

struct scheme vt220_charset = {
	.border = {
		{"\033#6\x6c","\x71","\x6b"},
		{"\033#6\x78","    ","\x78"},
		{"\033#6\x6d","\x71","\x6a"},
		{  "=","","="  },
	},

	.snake = {
		{"@","A",">","V","<"}, //head
		{"#","#","#","#","#"},
		{"#","#","#","#","#"},
		{"#","#","#","#","#"},
		{"#","#","#","#","#"},
	},

	.item = { "$", "%", "&"},

	.init_seq = "\033(0\033*B\x0f"  /* G0=Graphics, G2=ASCII, invoke G0  */
	            "\033[?3l",         /* disable 132 column mode (DECCOLM) */
	.reset_seq = "\033(B"    /* reset to DEC Multinational Character Set */
	             "\033[?3h", /* reenable DECCOLM (WARN: unconditionally!)*/

	.cell_width = 1,
};

#endif
