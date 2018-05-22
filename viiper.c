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
#define CW op.scheme->cell_width

struct game {
	int w; /* field width */
	int h; /* field height */
	int d; /* direction the snake is looking */
	int t; /* time of game start */
	int p; /* score */
	float v; /* velocity in moves per second */
	struct snake* s; /* snek */
	struct item* i; /* items (food, boni) */
	struct directions {
		int h; /* write head */
		int n; /* number of elements */
		int c[16];
	} k; /* ring buffer for direction events */
} g;

struct opt {
	int l; /* initial snake length */
	int s; /* initial snake speed */
	struct scheme* scheme;
} op;

jmp_buf game_over;

int main (int argc, char** argv) {
	/* defaults: */
	g.w = 30; //two-char-width
	g.h = 20;
	op.l = 10;
	op.s = 8;
	op.scheme = &unic0de;

	int optget;
	opterr = 0; /* don't print message on unrecognized option */
	while ((optget = getopt (argc, argv, "+s:dh")) != -1) {
		switch (optget) {
		case 's':
			op.s = atof(optarg);
			if (op.s < 1) {
				fprintf (stderr, "speed must be >= 1\n");
				return 1;
			}
			break;
		case 'd': op.scheme = &vt220_charset; break;
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

	switch (sigsetjmp(game_over, 1)) {
	case GAME_INIT:
	case GAME_START:
		viiper();
	case GAME_OVER:
		timer_setup(0);
		show_playfield();
		move_ph (g.h/2+LINE_OFFSET, g.w);
		printf ("snek ded :(");
		fflush(stdout);
		sleep(2);
	case GAME_EXIT:
		exit(0);
	}

	return 0;
}

int viiper(void) {
	init_snake();
	show_playfield ();
	g.d = EAST;
	g.v = op.s;

	timer_setup(1);
	g.t = time(NULL);

	spawn_item(FOOD, rand() % NUM_FOODS, NULL); //TODO: shape distribution, so bigger values get selected less

	for(;;) {
		switch (getctrlseq()) {
		case CTRSEQ_CURSOR_LEFT: case 'h':append_movement(WEST);  break;
		case CTRSEQ_CURSOR_DOWN: case 'j':append_movement(SOUTH); break;
		case CTRSEQ_CURSOR_UP:   case 'k':append_movement(NORTH); break;
		case CTRSEQ_CURSOR_RIGHT:case 'l':append_movement(EAST);  break;
		case 'p':
			timer_setup(0);
			move_ph (g.h/2+LINE_OFFSET, g.w*CW/2);
			printf ("\033[5mPAUSE\033[0m"); /* blinking text */
			if (getchar() == 'q') exit(0);
			show_playfield();
			timer_setup(1);
			break;
		case 'r': siglongjmp(game_over, GAME_START);
		case 'q': siglongjmp(game_over, GAME_EXIT);
		case CTRL_'L':
			screen_setup(1);
			show_playfield();
			break;
		case 0x02: /* STX; gets sent when returning from SIGALRM */
			continue;
		}
	}

}

#define pop_dir() (g.k.n? g.k.c[(16+g.k.h-g.k.n--)%16] : NONE)
void snake_advance (void) {
	int respawn = 0;
	struct item* i; /* temporary item (defined here to respawn at the end) */
	int new_dir = pop_dir();
	/* switch direction if new one is in the buffer and it won't kill us: */
	if (new_dir && g.d != OPPOSITE(new_dir)) g.d = new_dir;

	int new_row = g.s->r +(g.d==SOUTH) -(g.d==NORTH);
	int new_col = g.s->c +(g.d==EAST)  -(g.d==WEST);

	/* detect food hit and spawn a new food */
	for (i = g.i; i; i = i->next) {
		if (i->r == new_row && i->c == new_col) {
			consume_item (i);
			respawn = 1; /* must respawn after advancing head */
			break;
		}
	}

	if (new_row >= g.h || new_col >= g.w || new_row < 0 || new_col < 0)
		siglongjmp(game_over, GAME_OVER);

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

	if (respawn) spawn_item(FOOD, rand() % NUM_FOODS, i);
	draw_sprites (old_tail[0], old_tail[1]);
}

void spawn_item (int type, int value, struct item* p_item) {
	int row, col;
try_again:
	row = rand() % g.h;
	col = rand() % g.w;
	/* loop through snake to check if we aren't on it */
	//WARN: inefficient as snake gets longer; near impossible in the end
	for (struct snake* s = g.s; s; s = s->next)
		if (s->r == row && s->c == col) goto try_again;

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
	switch (i->t) {
	case FOOD:
		switch (i->v) {
		case FOOD_5:  g.p +=  5; break;
		case FOOD_10: g.p += 10; break;
		case FOOD_20: g.p += 20; break;
		}
		snake_append(&g.s, 0,0);  /* position doesn't matter, as item */
		break;       /* will be reused as the head before it is drawn */
	case BONUS:
		//TODO: handle bonus
		break;
	}

	if (i->next) i->next->prev = i->prev;
	if (i->prev) i->prev->next = i->next;
	else g.i = i->next;
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

	draw_sprites (0,0);
}

void draw_sprites (int erase_r, int erase_c) {
	/* erase old tail */
	move_ph (erase_r+LINE_OFFSET, erase_c*CW+COL_OFFSET);
	printm (CW, " ");

	/* print snake */
	struct snake* last = NULL;
	int color = 2;
	for (struct snake* s = g.s; s; s = s->next) {
		move_ph (s->r+LINE_OFFSET, s->c*CW+COL_OFFSET); /*NOTE: all those are actually wrong; draws snake 1 col too far left*/
		
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

		printf ("\033[%sm", op.scheme->color[color]);
		print (op.scheme->snake[predecessor][successor]);
		printf ("\033[0m");
		last = s;
		color = !color;
	}

	/* print item queue */
	for (struct item* i = g.i; i; i = i->next) {
		move_ph (i->r+LINE_OFFSET, i->c*CW+COL_OFFSET);
		if (i->t == FOOD) print (op.scheme->item[i->v]);
		else if (i->t==BONUS) /* TODO: print bonus */;
	}

	/* print score */
	int score_width = g.p > 9999?6:4;
	move_ph (0, (g.w*CW-score_width)/2-COL_OFFSET);
	printf ("%s %0*d %s", BORDER(S,L), score_width, g.p, BORDER(S,R));
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

#define free_ll(head) do{ \
	while (head) { \
		void* tmp = head; \
		head = head->next; \
		free(tmp); \
	} \
	head = NULL; \
}while(0)

void init_snake() {
	if (g.s)
		free_ll(g.s);
	if (g.i)
		free_ll(g.i);
	for (int i = 0; i < op.l; i++) {
		if (g.w/2-i < 0) /* go upwards if we hit left wall */
			snake_append(&g.s, g.h/2-(i-g.w/2), 0);
		else /* normally just keep goint left */
			snake_append(&g.s, g.h/2, g.w/2-i);
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
	g.k.h = ++g.k.h % 16;
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
		g.w = (w.ws_col - (COL_OFFSET+COL_OFFSET))/op.scheme->display_width;
	if (LINE_OFFSET + g.h + LINES_AFTER > w.ws_row)
		g.h = w.ws_row - (LINE_OFFSET+LINES_AFTER);
}

void timer_setup (int enable) {
	static struct itimerval tbuf;
	tbuf.it_interval.tv_sec  = 0;//TODO: make it speed up automatically
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
		print (op.scheme->init_seq); /* swich charset, if necessary */
	} else {
		print (op.scheme->reset_seq); /* reset charset, if necessary */
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
