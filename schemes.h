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

	char* item[NUM_ITEMS];
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
		[FOOD_10] = "🍎"
	},
};

#endif
