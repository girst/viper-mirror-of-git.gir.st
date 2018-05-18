#ifndef __VIIPER_H__
#define __VIIPER_H__

#define SHORTHELP "%s [OPTIONS] [FIELDSIZE]\n"
#define LONGHELP \
	"OPTIONS:\n" \
	"    -h(elp)\n" \
	"FIELDSIZE:\n" \
	"    WxH (width 'x' height)\n" \
	"    defaults to 30x20\n" \
	"\n" \
	"Keybindings:\n" \
	"    hjkl: move left/down/up/right\n" \
	"    p:    pause / unpause\n" \
	"    r:    start a new game\n" \
	"    q:    quit\n"

struct snake {
	int r; /* row */
	int c; /* column */
	struct snake* next; /* points to tail */
};
struct item {
	int r; /* row */
	int c; /* column */
	int t; /* type */
	int v; /* value */ //TODO: make type only differentiate between food/bonus/etc, and use value (so we can randomly select one of each)
	int s; /* spawn time (for bonus) */
	struct item* prev;
	struct item* next;
};
struct directions {
	int d; /* direction */
	struct directions* next;
};
enum direction {
	NONE,
	NORTH,
	EAST,
	SOUTH,
	WEST,
};
enum item_type {
	NO_ITEM,
	FOOD,
	BONUS,
};
enum food_value {
	FOOD_5,
	FOOD_10,
	FOOD_20,
	NUM_FOODS,
};
enum bonus_value {
	NUM_BONI,
};

int viiper(void);
void snake_advance (void);
void spawn_item (int type, int value);
void consume_item (struct item* i);
void show_playfield (void);
void snake_append (struct snake* s, int row, int col);
void init_snake();
void quit (void);
int getctrlseq (void);
void append_movement (int d);
int get_movement (void);
void move_ph (int line, int col);
void clamp_fieldsize (void);
void timer_setup (int enable);
void signal_setup (void);
void signal_handler (int signum);
void screen_setup (int enable);
void raw_mode(int enable);
enum event {
	/* for getctrlseq() */
	CTRSEQ_NULL    =  0,
	CTRSEQ_EOF     = -1,
	CTRSEQ_INVALID = -2,
	CTRSEQ_MOUSE   = -3,
	CTRSEQ_CURSOR_LEFT  = -7,
	CTRSEQ_CURSOR_DOWN  = -8,
	CTRSEQ_CURSOR_UP    = -9,
	CTRSEQ_CURSOR_RIGHT = -10,
};

#endif
