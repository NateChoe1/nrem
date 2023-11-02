#include <curses.h>

#include <tui.h>
#include <util.h>
#include <interfaces.h>

int tui_cal(enum tui_state *state, WINDOW *win) {
	int w, h;
	getmaxyx(win, h, w);
	if (w == -1 || h == -1) {
		return 1;
	}

	wclear(win);

	/* Draw calendar header */
	char header[50];
	int headerlen;
	headerlen = snprintf(header, sizeof header,
			"%s, %d", months[tui_mon], tui_year);
	header[sizeof header - 1] = '\0';
	if (headerlen < 0) {
		return 1;
	}
	mvwaddstr(win, 0, w/2 - headerlen/2, header);

	/* Draw the calendar itself */
	tui_calwidget(win, 1, 0, -1, -1, tui_year, tui_mon, tui_day);

	int c = wgetch(win);

	switch (c) {
	case 'q':
		*state = DONE;
		return 0;
	case 'n':
		*state = NEWEVENT;
		return 0;
	case 'h': case KEY_LEFT:
		--tui_day;
		break;
	case 'l': case KEY_RIGHT:
		++tui_day;
		break;
	case 'j': case KEY_DOWN:
		tui_day += 7;
		break;
	case 'k': case KEY_UP:
		tui_day -= 7;
		break;
	case 'L': case 'J':
		tui_day = 0;
		++tui_mon;
		break;
	case 'H': case 'K':
		tui_day = 0;
		--tui_mon;
		break;
	}

	normdate(&tui_day, &tui_mon, &tui_year);
	return 0;
}
