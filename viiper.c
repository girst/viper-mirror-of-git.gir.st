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
#include <ctype.h>
#include <poll.h>
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

#define COL_OFFSET 1
#define LINE_OFFSET 1
#define LINES_AFTER 1
#define CW 2 /* cell width */

struct game {
	int w; /* field width */
	int h; /* field height */
	int d; /* direction the snake is looking */
	int t; /* time of game start */
	int p; /* score */
	float v; /* velocity in moves per second */
	struct snake* s; /* snek */
	struct item* i; /* items (food, boni) */
	struct directions* n;/* next direction events to process */
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
	op.scheme = &unic0de; //TODO: expose to getopt() once more sets are available

	int optget;
	opterr = 0; /* don't print message on unrecognized option */
	while ((optget = getopt (argc, argv, "+h")) != -1) {
		switch (optget) {
		case 'h':
		default: 
			fprintf (stderr, SHORTHELP LONGHELP, argv[0]);
			return !(optget=='h');
		}
	} if (optind < argc) { /* parse Fieldspec */
	}

	srand(time(0));
	signal_setup();
	screen_setup(1);
	atexit (*quit);

	if (sigsetjmp(game_over, 1)) {
		timer_setup(0);
		move_ph (g.h/2+LINE_OFFSET, g.w);
		printf ("you died :(");
		fflush(stdout);
		sleep(5);
	}

	//TODO: call viiper() in a game loop
	viiper();
quit:
	return 0;
}

int viiper(void) {
	init_snake();
	show_playfield ();
	g.d = EAST;
	g.v = op.s;

	timer_setup(1);

	spawn_item(FOOD, rand() % NUM_FOODS); //TODO: shape distribution, so bigger values get selected less

	for(;;) {
		switch (getctrlseq()) {
		case '+': g.v++;timer_setup(1);break; //TODO: temporary, to set speed
		case '#': if (g.v > 1) g.v--;timer_setup(1); break; //TODO: temporary, to set speed

		case CTRSEQ_CURSOR_LEFT:
		case 'h': append_movement(WEST);  break;
		case CTRSEQ_CURSOR_DOWN:
		case 'j': append_movement(SOUTH); break;
		case CTRSEQ_CURSOR_UP:
		case 'k': append_movement(NORTH); break;
		case CTRSEQ_CURSOR_RIGHT:
		case 'l': append_movement(EAST);  break;
		case 'p': /*TODO: pause*/ break;
		case 'r': /*TODO:restart*/ return 0;
		case 'q': return 0;
		case CTRL_'L':
			screen_setup(1);
			show_playfield();
			break;
		}

		print ("\033[H\033[J");
		show_playfield ();//TODO: only redraw diff
	}

}

void snake_advance (void) {
	if (g.n) {/* new direction in the buffer */
		int possible_new_dir = get_movement();
		if (g.d == EAST && possible_new_dir == WEST) goto ignore_new;
		if (g.d == WEST && possible_new_dir == EAST) goto ignore_new;
		if (g.d == NORTH && possible_new_dir == SOUTH) goto ignore_new;
		if (g.d == SOUTH && possible_new_dir == NORTH) goto ignore_new;
		g.d = possible_new_dir; /*pop off a new direction if available*/
ignore_new:
		1;
	}

	int new_row = g.s->r +(g.d==SOUTH) -(g.d==NORTH);
	int new_col = g.s->c +(g.d==EAST)  -(g.d==WEST);

	/* detect food hit and spawn a new food */
	for (struct item* i = g.i; i; i = i->next) {
		if (i->r == new_row && i->c == new_col) {
			consume_item (i);
			spawn_item(FOOD, rand() % NUM_FOODS);
		}
	}

	/*NOTE:no idea why I have to use g.w+1, but otherwise we die too early*/
	if (new_row >= g.h || new_col >= g.w+1 || new_row < 0 || new_col < 0)
		siglongjmp(game_over, 1/*<-will be the retval of setjmp*/);

	struct snake* new_head;
	struct snake* new_tail; /* former second-to-last element */
	for (new_tail = g.s; new_tail->next->next; new_tail = new_tail->next)
		/* use the opportunity of looping to check if we eat ourselves*/
		if(new_tail->next->r == new_row && new_tail->next->c == new_col)
			siglongjmp(game_over, 1/*<-will be the retval of setjmp*/);
	new_head = new_tail->next; /* reuse element instead of malloc() */
	new_tail->next = NULL;
	
	new_head->r = new_row;
	new_head->c = new_col;
	new_head->next = g.s;

	g.s = new_head;
}

void spawn_item (int type, int value) {
	int row, col;
try_again:
	row = rand() % g.h;
	col = rand() % g.w;
	/* loop through snake to check if we aren't on it */
	//TODO: inefficient as snake gets longer; near impossible in the end
	for (struct snake* s = g.s; s; s = s->next)
		if (s->r == row && s->c == col) goto try_again;

	//3. get item from category TODO
	struct item* new_item = malloc (sizeof(struct item));
	new_item->r = row;
	new_item->c = col;
	new_item->t = type;
	new_item->v = value;
	new_item->s = time(0);
	if (g.i) g.i->prev = new_item;
	new_item->next = g.i;

	g.i = new_item;
}

void consume_item (struct item* i) {
	struct item* predecessor = i->prev;
	struct item* successor = i->next;

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

	if (predecessor == NULL) {
		g.i = successor;
		if (successor) successor->prev = NULL;
	} else {
		predecessor->next = successor;
		successor->prev = predecessor;
	}

	free (i);
}

void show_playfield (void) {
	//int score_width = g.p > 9999?6:4;
	int score_width = 4;             //        v- = the -| spacer |-
	float half_width = (g.w - score_width/CW - 2)/2.0; //TODO: this whole endavour is ugly
	/* top border */
	print(BORDER(T,L));
	printm (half_width, BORDER(T,C)); //TODO: i bet this breaks in dec mode
	printf ("%s %0*d %s", BORDER(S,L), score_width, g.p, BORDER(S,R));
	printm ((int)(half_width+.5), BORDER(T,C));
	printf ("%s\n", BORDER(T,R));

	/* main area */
	for (int row = 0; row < g.h; row++)
		printf ("%s%*s%s\n", BORDER(C,L), CW*g.w, "", BORDER(C,R));

	/* bottom border */
	print(BORDER(B,L));
	printm (g.w, BORDER(B,C));
	print (BORDER(B,R));

	/* print snake */
	struct snake* last = NULL;
	int color = -1; //TODO: that's a hack
	for (struct snake* s = g.s; s; s = s->next) {
		move_ph (s->r+COL_OFFSET, s->c*CW+LINE_OFFSET);
		
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

		printf ("\033[%sm", color==-1?"1m\033[92":color?"92":"32"); //TODO: clean this up
		print (op.scheme->snake[predecessor][successor]);
		printf ("\033[0m");
		last = s;
		color = (color+1) % 2;
	}

	/* print item queue */
	for (struct item* i = g.i; i; i = i->next) {
		move_ph (i->r+LINE_OFFSET, i->c*CW+COL_OFFSET);
		if (i->t == FOOD) print (op.scheme->item[i->v]);
		else if (i->t==BONUS) /* TODO: print bonus */;
	}

	//TODO: temporarily print speed
	move_ph (g.h+LINE_OFFSET, g.w);
	printf ("%f", g.v);
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

void init_snake() {
	for (int i = 0; i < op.l; i++)
		snake_append(&g.s, g.h/2, g.w/2-i);
}

#define free_ll(head) do{ \
	while (head) { \
		void* tmp = head; \
		head = head->next; \
		free(tmp); \
	} \
}while(0)

void quit (void) {
	screen_setup(0);
	free_ll(g.s);
	free_ll(g.i);
	free_ll(g.n); //TODO: doesn't get free'd correctly
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
	int offset = 0x20; /* never sends control chars as data */
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
	struct directions* n;
	for (n = g.n; n && n->next; n = n->next); /* advance to the end */
	if (n && n->d == dir) return; /* don't add the same direction twice */

	struct directions* new_event = malloc (sizeof(struct directions));
	new_event->d = dir;
	new_event->next = NULL;

	if (g.n == NULL)
		g.n = new_event;
	else
		n->next = new_event;
}

int get_movement (void) {
	if (g.n == NULL) return -1;

	int retval = g.n->d;
	struct directions* delet_this = g.n;
	g.n = g.n->next;
	free(delet_this);
	return retval;
}

void move_ph (int line, int col) {
	/* move printhead to zero-indexed position */
	printf ("\033[%d;%dH", line+1, col+1);
}

void clamp_fieldsize (void) { //TODO: use
	/* clamp field size to terminal size and mouse maximum: */
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	if (g.w < 1) g.w = 1;
	if (g.h < 1) g.h = 1;

	if (COL_OFFSET + g.w*CW + COL_OFFSET > w.ws_col)
		g.w = (w.ws_col - COL_OFFSET - COL_OFFSET)/CW; //TODO: does not work in `-d' (in xterm)
	if (LINE_OFFSET + g.h + LINES_AFTER > w.ws_row)
		g.h = w.ws_row - (LINE_OFFSET+LINES_AFTER);
}

void timer_setup (int enable) {
	static struct itimerval tbuf;
	tbuf.it_interval.tv_sec  = 0;//TODO: make it speed up automatically
	tbuf.it_interval.tv_usec = (1000000/g.v)-1; /*WARN: 1 <= g.v <= 999999*/

	if (enable) {
		g.t = time(NULL);//TODO: interferes with 'pause'
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
}

void signal_handler (int signum) {
	//int dtime;
	switch (signum) {
	case SIGALRM:
		//dtime = difftime (time(NULL), g.t);
		//move_ph (1, g.w*CW-(CW%2)-3-(dtime>999));
		//printf ("[%03d]", g.t?dtime:0);
		snake_advance();
		break;
	case SIGINT:
		exit(128+SIGINT);
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
