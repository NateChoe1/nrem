#ifndef HAVE_TUI
#define HAVE_TUI

#include <curses.h>

enum tui_state {
	VIEWCAL,
	VIEWDAY,
	NEWEVENT,
	DONE,
};

extern int tui_day, tui_mon, tui_year;

int tui_cal(enum tui_state *state, WINDOW *win);
int tui_newevent(enum tui_state *state, WINDOW *win);

int tui_newevent_reset(WINDOW *win);

/* Draws a calendar, filling the specified area. -1 for w or h means fill the
 * rest of the screen */
void tui_calwidget(WINDOW *win, int top, int left, int w, int h,
		int year, int mon, int day);

extern char const * const months[12];
extern char const * const weekdays[7];
extern int tui_hascolor;

#define COL_BRIGHT 1

#endif
