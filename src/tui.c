/* @LEGAL_HEAD [0]
 *
 * nrem, a cli friendly calendar
 * Copyright (C) 2023  Nate Choe <nate@natechoe.dev>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * @LEGAL_TAIL */

#include <curses.h>

#include <tui.h>
#include <util.h>
#include <interfaces.h>

int tui_day, tui_mon, tui_year;
char const * const months[] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December",
};
char const * const weekdays[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};
int tui_hascolor;
static int dump;

int nremtui(int argc, char **argv) {
	WINDOW *win;
	enum tui_state state, prevstate;
	int ret;

	initscr();
	cbreak();
	noecho();
	refresh(); /* I have no idea why this line is necessary */

	win = newwin(0, 0, 0, 0);

	keypad(win, TRUE);
	nl();

	state = prevstate = VIEWCAL;

	tui_day = nowb.tm_mday - 1;
	tui_mon = nowb.tm_mon;
	tui_year = nowb.tm_year + 1900;
	if ((tui_hascolor = has_colors())) {
		start_color();
		init_pair(COL_BRIGHT, COLOR_WHITE, COLOR_BLUE);
	}

	int (*modes[])(enum tui_state *state, WINDOW *win) = {
		[VIEWCAL] = tui_cal,
		[VIEWDAY] = tui_viewday,
		[NEWEVENT] = tui_newevent,
	};
	int (*resets[])(WINDOW *win) = {
		[VIEWCAL] = NULL,
		[VIEWDAY] = tui_viewday_reset,
		[NEWEVENT] = tui_newevent_reset,
	};

	for (;;) {
		if (state != prevstate && resets[state] != NULL) {
			if ((ret = resets[state](win)) != 0) {
				goto end;
			}
		}
		prevstate = state;
		if (modes[state] != NULL) {
			if ((ret = modes[state](&state, win)) != 0) {
				goto end;
			}
		}
		else {
			ret = 1;
			goto end;
		}
		if (state == DONE) {
			ret = 0;
			goto end;
		}
	}

end:
	endwin();
	return ret;
}

void tui_calwidget(WINDOW *win, int top, int left, int w, int h,
		int year, int mon, int day) {
	int winw, winh;
	getmaxyx(win, winh, winw);

	if (w < 0) {
		w = winw - left;
	}
	if (h < 0) {
		h = winh - top;
	}

	int boxwidth = (w-1)/7;
	int boxheight = (h-1)/6;
	int calwidth = boxwidth*7+1;
	int leftmargin = left + (w - calwidth) / 2;
	int topmargin = top + 2;
	wmove(win, top, leftmargin+1);
	wattron(win, A_UNDERLINE);
	for (int i = 0; i < calwidth-2; ++i) {
		waddch(win, ' ');
	}
	wattroff(win, A_UNDERLINE);
	wmove(win, top + 1, leftmargin);
	waddch(win, '|');
	wattron(win, A_UNDERLINE);
	for (int i = 0; i < 7; ++i) {
		int curx;
		wmove(win, top + 1, i * boxwidth + leftmargin + 1);
		waddstr(win, weekdays[i]);
		for (getyx(win, dump, curx);
				curx < (i+1) * boxwidth + leftmargin;
				++curx) {
			waddch(win, ' ');
		}
		if (i == 6) {
			wattroff(win, A_UNDERLINE);
		}
		waddch(win, '|');
	}

	int weeks = getweeks(year, mon);
	int firstday = getfirstday(year, mon);
	int cursorx, cursory;
	int monthlen = getmonthlen(year, mon);
	cursorx = cursory = -1;

	for (int i = 0; i < weeks; ++i) {
		for (int j = 0; j < 7; ++j) {
			struct eventlist *events = NULL;
			int currday = i*7 + j - firstday;
			if (0 <= currday && currday < monthlen) {
				time_t start, end;
				start = findstart(currday, mon, year);
				end = findend(currday, mon, year);
				events = datesearch(&f, start, end);
			}
			else {
				currday = -1;
			}
			for (int r = 0; r < boxheight; ++r) {
				wmove(win, topmargin + i*boxheight+r,
						leftmargin + j*boxwidth);
				waddch(win, '|');

				if (currday == day && tui_hascolor) {
					wattron(win, COLOR_PAIR(COL_BRIGHT));
				}
				if (r == boxheight-1) {
					wattron(win, A_UNDERLINE);
				}

				int cy, cx;
				getyx(win, cy, cx);

				if (r == 0 && currday == day) {
					cursory = cy;
					cursorx = cx;
				}

				if (currday != -1 && r == 0) {
					char date[20];
					sprintf(date, "%d", currday+1);
					waddnstr(win, date, boxwidth-1);
				}
				if (events != NULL &&
						r > 0 && r <= events->len) {
					waddnstr(win, events->events[r-1].name,
							boxwidth-1);
				}

				int newcx;
				for (getyx(win, dump, newcx);
						newcx < cx + boxwidth - 1;
						++newcx) {
					waddch(win, ' ');
				}
				wattroff(win, A_UNDERLINE);
				if (currday == day && tui_hascolor) {
					wattroff(win, COLOR_PAIR(COL_BRIGHT));
				}
				waddch(win, '|');
			}
			freeeventlist(events);
		}
	}

	if (cursorx != -1 && cursory != -1) {
		wmove(win, cursory, cursorx);
	}

	wrefresh(win);
}
