/*******************************************************************************
 viiper 0.1
 By Tobias Girstmair, 2018

 ./viiper 40x25
 (see ./viiper -h for full list of options)

 KEYBINDINGS:  - hjkl to move
               - p to pause and resume
               - r to restart
               - q to quit
               - (see `./minesviiper -h' for all keybindings)

 GNU GPL v3, see LICENSE or https://www.gnu.org/licenses/gpl-3.0.txt
*******************************************************************************/


#define _POSIX_C_SOURCE 2 /*for getopt and sigaction in c99, sigsetjmp*/
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "viiper.h"
#include "schemes.h"

#define MIN(a,b) (a>b?b:a)
#define MAX(a,b) (a>b?a:b)
#define CLAMP(a,m,M) (a<m?m:(a>M?M:a))
#define printm(n, s) for (int _loop = 0; _loop < n; _loop++) fputs (s, stdout)
#define print(str) fputs (str?str:"", stdout)
#define CTRL_ 0x1F &

#define OPPOSITE(dir) ( \
	dir == EAST  ? WEST  :   \
	dir == WEST  ? EAST  :   \
	dir == NORTH ? SOUTH :   \
	dir == SOUTH ? NORTH : -1)

#define COL_OFFSET 1
#define LINE_OFFSET 1
#define LINES_AFTER 1
#define CW op.sch->cell_width
#define DW op.sch->display_width

#define SPEEDUP_AFTER 100 /* increment speed every n points */
#define BONUS_INTERVAL 90 /* how often a bonus item is spawned */
#define BONUS_DURATION 15 /* how long one can catch the bonus */
#define BONUS_WARN      5 /* how long before the removal to warn */
#define BONUS_STOP_NUM 10 /* how many steps the time freezes */

struct game {
	int w; /* field width */
	int h; /* field height */
	int d; /* direction the snake is looking */
	int p; /* score */
	float v; /* velocity in moves per second */
	time_t t; /* time of game start */
	struct snake* s; /* snek */
	struct item* i; /* items (food, boni) */
	struct bonus {
		int t; /* bonus type (bitmask based on enum bonus_value) */
		int s; /* steps left of time freeze bonus */
		time_t n; /* time of next bonus item spawn */
		time_t w; /* time of wall-wrap end */
	} b; /* bonus-related values */
	struct directions {
		int h; /* write head */
		int n; /* number of elements */
		int c[16];
	} k; /* ring buffer for direction events */
} g;

struct opt {
	int l; /* initial snake length */
	int s; /* initial snake speed */
	struct scheme* sch;
} op;

jmp_buf game_over;

int main (int argc, char** argv) {
	/* defaults: */
	g.w = 30;
	g.h = 20;
	op.l = 10;
	op.s = 8;
	op.sch = &unic0de;

	int optget;
	opterr = 0; /* don't print message on unrecognized option */
	while ((optget = getopt (argc, argv, "+s:l:dh")) != -1) {
		switch (optget) {
		case 's':
			op.s = atof(optarg);
			if (op.s < 1) {
				fprintf (stderr, "speed must be >= 1\n");
				return 1;
			}
			break;
		case 'l':
			op.l = atoi(optarg);
			if (op.s < 2 || op.s > g.w*g.h-1) {
				fprintf (stderr, "length must be >= 2 and < W*H\n");
				return 1;
			}
			break;
		case 'h':
		default: 
			fprintf (stderr, SHORTHELP LONGHELP, argv[0]);
			return !(optget=='h');
		}
	} if (optind < argc) { /* parse Fieldspec */
		int n = sscanf (argv[optind], "%dx%d", &g.w, &g.h);

		if (n < 2) {
			fprintf(stderr,"FIELDSIZE is WxH (width 'x' height)\n");
			return 1;
		}
	}

	clamp_fieldsize();

	srand(time(0));
	signal_setup();
	screen_setup(1);
	atexit (*quit);

	char* end_screen_msg = "";
restart:
	switch (sigsetjmp(game_over, 1)) {
	case GAME_INIT:
	case GAME_START:
		viiper();
		break; /* function doesn't return, but `-Wextra' complains */
	case GAME_OVER:
		end_screen_msg = " GAME  OVER ";
		goto end_screen;
	case GAME_WON:
		end_screen_msg = "CONGRATULATIONS!";
		goto end_screen;
end_screen:
		timer_setup(0);
		show_playfield();
		for (;;) switch (end_screen(end_screen_msg)) {
		case 'r': goto restart;
		case 'q': goto quit;
		case CTRL_'L':
			screen_setup(1);
			show_playfield();
			break;
		default: continue;
		}
	case GAME_EXIT:
		goto quit;
	}

quit:
	return 0;
}

int viiper(void) {
	init_snake();
	show_playfield ();
	g.d = EAST;
	g.v = op.s;

	timer_setup(1);
	g.t = time(NULL);
	g.p = 0;
	g.k.n = 0;
	g.b.n = time(NULL) + BONUS_INTERVAL;
	g.b.s = 0;
	g.b.t = 0;

	spawn_item(FOOD, rand() % NUM_FOODS, NULL);

	for(;;) {
		switch (getctrlseq()) {
		case 'h': case CTRSEQ_CURSOR_LEFT: append_movement(WEST); break;
		case 'j': case CTRSEQ_CURSOR_DOWN: append_movement(SOUTH);break;
		case 'k': case CTRSEQ_CURSOR_UP:   append_movement(NORTH);break;
		case 'l': case CTRSEQ_CURSOR_RIGHT:append_movement(EAST); break;
		case 'p': pause_game(); break;
		case 'r': siglongjmp(game_over, GAME_START);
		case 'q': siglongjmp(game_over, GAME_EXIT);
		case CTRL_'L':
			screen_setup(1);
			show_playfield();
			break;
		case 0x02: /* STX; gets sent when returning from SIGALRM */
			continue;
		}

		if (g.b.s) { /* highjack keyboard function for time freeze */
			snake_advance();
			if (!--g.b.s) timer_setup(1);/*turn back on afterwards*/
		}
	}

}

#define pop_dir() (g.k.n? g.k.c[(16+g.k.h-g.k.n--)%16] : NONE)
void snake_advance (void) {
	int respawn = 0;
	struct item* i; /*temporary item (defined here to respawn at the end) */
	int new_dir = pop_dir();
	/* switch direction if new one is in the buffer and it won't kill us: */
	if (new_dir && g.d != OPPOSITE(new_dir)) g.d = new_dir;

	int new_row = g.s->r +(g.d==SOUTH) -(g.d==NORTH);
	int new_col = g.s->c +(g.d==EAST)  -(g.d==WEST);

	/* detect food hit and spawn a new food */
	for (i = g.i; i; i = i->next) {
		if (i->r == new_row && i->c == new_col) {
			consume_item (i);

			switch (i->t) {
			case FOOD: respawn = 1; break;
			case BONUS: remove_bonus(i); break;
			}
			break;
		}
	}

	if (g.b.t & 1<<BONUS_WRAP) {
	  if (new_row >= g.h) new_row = 0;
	  if (new_col >= g.w) new_col = 0;
	  if (new_row < 0) new_row = g.h-1;
	  if (new_col < 0) new_col = g.w-1;
	} else {
	  if (new_row >= g.h || new_col >= g.w || new_row < 0 || new_col < 0)
		siglongjmp(game_over, GAME_OVER);
	}

	struct snake* new_head;
	struct snake* new_tail; /* former second-to-last element */
	for (new_tail = g.s; new_tail->next->next; new_tail = new_tail->next)
		/*use the opportunity of looping to check if we eat ourselves:*/
		if(new_tail->next->r == new_row && new_tail->next->c == new_col)
			siglongjmp(game_over, GAME_OVER);
	int old_tail[2] = {new_tail->next->r, new_tail->next->c};/*gets erased*/
	new_head = new_tail->next; /* reuse element instead of malloc() */
	new_tail->next = NULL;
	
	new_head->r = new_row;
	new_head->c = new_col;
	new_head->next = g.s;

	g.s = new_head;

	/* bonus mode(s) still active? */
	if (g.b.t&1<<BONUS_WRAP && g.b.w < time(NULL)) {
		g.b.t &= ~(1<<BONUS_WRAP);
		show_playfield();
	}

	if (respawn) spawn_item(FOOD, rand() % NUM_FOODS, i);
	draw_sprites (old_tail[0], old_tail[1]);
}

void spawn_item (int type, int value, struct item* p_item) {
	int row, col;
	char occupied[g.w][g.h]; int snake_len = 0;
	memset(*occupied, 0, g.w*g.h);
try_again:
	row = rand() % g.h;
	col = rand() % g.w;
	if (snake_len >= g.w*g.h-1) siglongjmp(game_over, GAME_WON);
	/* loop through snake to check if we aren't on it */
	if (occupied[row][col]) goto try_again; /* shortcut */
	for (struct snake* s = g.s; s; s = s->next)
		if (s->r == row && s->c == col) {
			occupied[s->r][s->c] = 1; snake_len++;
			goto try_again;
		}

	/* if we got a item buffer reuse it, otherwise create a new one: */
	struct item* new_item;
	if (p_item) new_item = p_item;
	else new_item = malloc (sizeof(struct item));

	new_item->r = row;
	new_item->c = col;
	new_item->t = type;
	new_item->v = value;
	new_item->s = time(0);
	if (g.i) g.i->prev = new_item;
	new_item->next = g.i;
	new_item->prev = NULL;

	g.i = new_item;
}

void consume_item (struct item* i) {
	int old_score = g.p;

	switch (i->t) {
	case FOOD:
		switch (i->v) {
		case FOOD_PEAR:
		case FOOD_WMELON:
		case FOOD_BANANA:
		case FOOD_KIWI:
			g.p +=  5;
			break;
		case FOOD_APPLER:
		case FOOD_CHERRY:
			g.p += 10;
			break;
		case FOOD_AVOCADO:
			g.p += 20;
			break;
		}
		snake_append(&g.s, -1, -1);
		break; /* will be reused as the head before it is drawn */
	case BONUS:
		switch (i->v) {
		case BONUS_SNIP:
			for (int i = 5; i && g.s->next->next; i--) {
				struct snake* p = g.s;
				while (p->next->next) p = p->next;
				free (p->next);
				p->next = NULL;
			}
			show_playfield();
			break;
		case BONUS_GROW:
			for (int i = 5; i; i--) snake_append(&g.s, -1, -1);
			break;
		case BONUS_SLOW:
			if (g.v > 1) g.v--;
			timer_setup(1);
			break;
		case BONUS_FAST:
			g.v++;
			timer_setup(1);
			break;
		case BONUS_WRAP:
			g.b.w = time(NULL)+30;
			g.b.t |= 1<<BONUS_WRAP;
			show_playfield();
			break;
		case BONUS_STOP:
			g.b.s = BONUS_STOP_NUM; /* stop the time for 5 steps */
			timer_setup(0);
			break;
		}
		g.b.n = time(NULL) + BONUS_INTERVAL; /*next bonus in x seconds*/
		break;
	}

	if (i->next) i->next->prev = i->prev;
	if (i->prev) i->prev->next = i->next;
	else g.i = i->next;

	/* snake speedup every 100 points: */
	if (g.p/SPEEDUP_AFTER - old_score/SPEEDUP_AFTER) g.v++, timer_setup(1);
}

void spawn_bonus(void) {
	for (struct item* i = g.i; i; i = i->next)
		if (i->t == BONUS) { /* bonus already there */
			if (((i->s+BONUS_DURATION)-time(NULL)) < 0) {
				remove_bonus(i); /* remove if over lifetime */
			}
			return;
		}
	if (g.b.n < time(NULL)) { /* time to spawn item: */
		spawn_item(BONUS, rand() % NUM_BONI, NULL);
		g.b.n = time(NULL) + BONUS_INTERVAL + BONUS_DURATION;
	}
}

void show_playfield (void) {
	move_ph (0,0);

	/* top border */
	print(BORDER(T,L));
	printm (g.w, BORDER(T,C));
	printf ("%s\n", BORDER(T,R));

	/* main area */
	for (int row = 0; row < g.h; row++)
		printf ("%s%*s%s\n", BORDER(C,L), CW*g.w, "", BORDER(C,R));

	/* bottom border */
	print(BORDER(B,L));
	printm (g.w, BORDER(B,C));
	print (BORDER(B,R));

	draw_sprites (-1,-1);
}

void draw_sprites (int erase_r, int erase_c) {
	/* erase old tail, if any */
	if (erase_r >= 0 && erase_c >= 0) {
		move_ph (erase_r+LINE_OFFSET, erase_c*CW+COL_OFFSET);
		printm (CW, " ");
	}

	/* print snake */
	struct snake* last = NULL;
	int color = 2;
	for (struct snake* s = g.s; s; s = s->next) {
		if (s->r < 0 || s->c < 0) continue; /*can't draw outside term.*/
		move_ph (s->r+LINE_OFFSET, s->c*CW+COL_OFFSET);
		
		int predecessor = (last==NULL)?NONE:
			(last->r < s->r) ? NORTH:
			(last->r > s->r) ? SOUTH:
			(last->c > s->c) ? EAST:
			(last->c < s->c) ? WEST:NONE;
		int successor = (s->next == NULL)?NONE:
			(s->next->r < s->r) ? NORTH:
			(s->next->r > s->r) ? SOUTH:
			(s->next->c > s->c) ? EAST:
			(s->next->c < s->c) ? WEST:NONE;

		printf ("\033[%sm", op.sch->color[color]);
		print (op.sch->snake[predecessor][successor]);
		printf ("\033[0m");
		last = s;
		color = !color;
	}

	/* print item queue */
	for (struct item* i = g.i; i; i = i->next) {
		move_ph (i->r+LINE_OFFSET, i->c*CW+COL_OFFSET);
		if      (i->t == FOOD)  print(op.sch->food[i->v]);
		else if (i->t == BONUS) {
			if (i->s + BONUS_DURATION-BONUS_WARN < time(NULL))
				printf("\033[5m%s\033[25m", op.sch->boni[i->v]);
			else print(op.sch->boni[i->v]);
		}
	}

	/* print score */
	int score_width = g.p > 9999?6:4;
	move_ph (0, (g.w*CW-score_width)/2-COL_OFFSET);
	printf ("%s %0*d %s", BORDER(S,L), score_width, g.p, BORDER(S,R));
}

void pause_game (void) {
	time_t pause_start = time(NULL);
	time_t pause_diff;
	timer_setup(0);

	move_ph (g.h/2+LINE_OFFSET, g.w*CW/2);
	printf ("\033[5;7m PAUSE \033[0m"); /* blinking reverse text */
	if (getchar() == 'q')
		siglongjmp(game_over, GAME_EXIT);

	/* update all timers, so bonus items don't get out of sync: */
	pause_diff = time(NULL) - pause_start;
	g.b.n += pause_diff;
	for (struct item* i = g.i; i; i = i->next)
		i->s += pause_diff;

	show_playfield();
	timer_setup(1);
}

#define MOVE_POPUP(WIDTH, LINE) \
	move_ph(g.h/2+LINE_OFFSET-1+LINE,(g.w*DW-WIDTH)/2)
	//TODO: macro does not correctly centre in DEC mode
int end_screen(char* message) {
	int msg_w = strlen(message);
	MOVE_POPUP(msg_w, -1);
	print(BORDER(T,L));
	printm ((msg_w+2)/DW, BORDER(T,C));
	print (BORDER(T,R));

	MOVE_POPUP(msg_w, 0);
	printf("%s %s %s", BORDER(C,L), message, BORDER(C,R));
	MOVE_POPUP(msg_w, 1);
	//TODO: requires shifting into ASCII/multilingual charset in DEC mode -- put graphics into upper/right charset?
	printf("%s `r' restart%*s%s", BORDER(C,L), msg_w-10, "", BORDER(C,R));
	MOVE_POPUP(msg_w, 2);
	printf("%s `q' quit%*s%s", BORDER(C,L), msg_w-7, "", BORDER(C,R));

	MOVE_POPUP(msg_w, 3);
	print(BORDER(B,L));
	printm ((msg_w+2)/DW, BORDER(B,C));
	print (BORDER(B,R));
	fflush(stdout);

	return getctrlseq();
}

void snake_append (struct snake** s, int row, int col) {
	struct snake* new = malloc (sizeof(struct snake));
	new->r = row;
	new->c = col;
	new->next = NULL;

	if (*s) {
		struct snake* p = *s;
		while (p->next) p = p->next;
		p->next = new;
	} else {
		*s = new;
	}
}

void remove_bonus (struct item*  i) {
	if (i->next) i->next->prev = i->prev;
	if (i->prev) i->prev->next = i->next;
	else g.i = i->next;

	draw_sprites (i->r, i->c); /* overwrite sprite */

	free(i);
}

#define free_ll(head) do{ \
	while (head) { \
		void* tmp = head; \
		head = head->next; \
		free(tmp); \
	} \
	head = NULL; \
}while(0)

void init_snake() {
	free_ll(g.s);
	free_ll(g.i);

	int direction = WEST;
	int r = g.h/2;
	int c = g.w/2;
	int min_r = 0;
	int max_r = g.h-1;
	int min_c = 0;
	int max_c = g.w-1;
	for (int i = 0; i < op.l; i++) {
		switch (direction) {
		case NORTH: r--; if(r <= min_r){direction=EAST; min_r++;}break;
		case EAST:  c++; if(c >= max_c){direction=SOUTH;max_c--;}break;
		case SOUTH: r++; if(r >= max_r){direction=WEST; max_r--;}break;
		case WEST:  c--; if(c <= min_c){direction=NORTH;min_c++;}break;
		}

		snake_append(&g.s, r, c);
	}
}

void quit (void) {
	screen_setup(0);
	free_ll(g.s);
	free_ll(g.i);
}

enum esc_states {
	START,
	ESC_SENT,
	CSI_SENT,
	MOUSE_EVENT,
};
int getctrlseq (void) {
	int c;
	int state = START;
	while ((c = getchar()) != EOF) {
		switch (state) {
		case START:
			switch (c) {
			case '\033': state=ESC_SENT; break;
			default: return c;
			}
			break;
		case ESC_SENT:
			switch (c) {
			case '[': state=CSI_SENT; break;
			default: return CTRSEQ_INVALID;
			}
			break;
		case CSI_SENT:
			switch (c) {
			case 'A': return CTRSEQ_CURSOR_UP;
			case 'B': return CTRSEQ_CURSOR_DOWN;
			case 'C': return CTRSEQ_CURSOR_RIGHT;
			case 'D': return CTRSEQ_CURSOR_LEFT;
			default: return CTRSEQ_INVALID;
			}
			break;
		default:
			return CTRSEQ_INVALID;
		}
	}
	return 2;
}

void append_movement (int dir) {
	if (g.k.n > 15) return; /* no space in buffer */
	if (g.k.n && g.k.c[(16+g.k.h-1)%16] == dir)
		return; /* don't add the same direction twice */

	g.k.c[g.k.h] = dir;
	g.k.n++;
	g.k.h = (g.k.h+1) % 16;
}

void move_ph (int line, int col) {
	/* move printhead to zero-indexed position */
	printf ("\033[%d;%dH", line+1, col+1);
}

void clamp_fieldsize (void) {
	/* clamp field size to terminal size and mouse maximum: */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	if (g.w < 10) g.w = 10;
	if (g.h < 10) g.h = 10;

	if (COL_OFFSET + g.w*CW + COL_OFFSET > w.ws_col)
		g.w = (w.ws_col - 2*COL_OFFSET) / DW;
	if (LINE_OFFSET + g.h + LINES_AFTER > w.ws_row)
		g.h = w.ws_row - (LINE_OFFSET+LINES_AFTER);
}

void timer_setup (int enable) {
	static struct itimerval tbuf;
	tbuf.it_interval.tv_sec  = 0;
	tbuf.it_interval.tv_usec = (1000000/g.v)-1; /*WARN: 1 <= g.v <= 999999*/

	if (enable) {
		tbuf.it_value.tv_sec  = tbuf.it_interval.tv_sec;
		tbuf.it_value.tv_usec = tbuf.it_interval.tv_usec;
	} else {
		tbuf.it_value.tv_sec  = 0;
		tbuf.it_value.tv_usec = 0;
	}

	if ( setitimer(ITIMER_REAL, &tbuf, NULL) == -1 ) {
		perror("setitimer");
		exit(1);
	}

}

void signal_setup (void) {
	struct sigaction saction;

	saction.sa_handler = signal_handler;
	sigemptyset(&saction.sa_mask);
	saction.sa_flags = 0;
	if (sigaction(SIGALRM, &saction, NULL) < 0 ) {
		perror("SIGALRM");
		exit(1);
	}

	if (sigaction(SIGINT, &saction, NULL) < 0 ) {
		perror ("SIGINT");
		exit (1);
	}

	if (sigaction(SIGCONT, &saction, NULL) < 0 ) {
		perror ("SIGCONT");
		exit (1);
	}
}

void signal_handler (int signum) {
	switch (signum) {
	case SIGALRM:
		snake_advance();
		spawn_bonus();
		break;
	case SIGINT:
		exit(128+SIGINT);
	case SIGCONT:
		screen_setup(1);
		show_playfield();
	}
}

void screen_setup (int enable) {
	if (enable) {
		raw_mode(1);
		printf ("\033[s\033[?47h"); /* save cursor, alternate screen */
		printf ("\033[H\033[J"); /* reset cursor, clear screen */
		printf ("\033[?25l"); /* hide cursor */
	} else {
		printf ("\033[?25h"); /* show cursor */
		printf ("\033[?47l\033[u"); /* primary screen, restore cursor */
		raw_mode(0);
	}
}

/* http://users.csc.calpoly.edu/~phatalsk/357/lectures/code/sigalrm.c */
void raw_mode(int enable) {
	static struct termios saved_term_mode;
	struct termios raw_term_mode;

	if (enable) {
		tcgetattr(STDIN_FILENO, &saved_term_mode);
		raw_term_mode = saved_term_mode;
		raw_term_mode.c_lflag &= ~(ICANON | ECHO);
		raw_term_mode.c_cc[VMIN] = 1 ;
		raw_term_mode.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_term_mode);
	} else {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_term_mode);
	}
}
