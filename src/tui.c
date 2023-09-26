#include <curses.h>

#include <util.h>
#include <interfaces.h>

static void displaycal(WINDOW *win, int month, int year);

static char *months[] = {
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
static char *weekdays[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
};
static int dump;

int nremtui(int argc, char **argv) {
	WINDOW *cal;

	int month = nowb.tm_mon;
	int year = nowb.tm_year + 1900;

	initscr();
	cbreak();
	noecho();
	refresh(); /* I have no idea why this line is necessary */

	cal = newwin(0, 0, 0, 0);

	for (;;) {
		displaycal(cal, month, year);

		int c = getch();

		if (c == 'q') {
			break;
		}
	}

	endwin();
	return 0;
}

static void displaycal(WINDOW *win, int month, int year) {
	char header[50];
	int headerlen;
	int w, h;

	headerlen = snprintf(header, sizeof header,
			"%s, %d", months[month], year);
	header[sizeof header - 1] = '\0';
	if (headerlen < 0) {
		return;
	}

	getmaxyx(win, h, w);
	mvwaddstr(win, 0, w/2 - headerlen/2, header);

	int boxwidth = (w-1)/7;
	int boxheight = (h-3)/6;
	int calwidth = boxwidth*7+1;
	int leftmargin = (w - calwidth) / 2;
	int topmargin = 3;
	wmove(win, 1, leftmargin+1);
	wattron(win, A_UNDERLINE);
	for (int i = 0; i < calwidth-2; ++i) {
		waddch(win, ' ');
	}
	wattroff(win, A_UNDERLINE);
	wmove(win, 2, leftmargin);
	waddch(win, '|');
	wattron(win, A_UNDERLINE);
	for (int i = 0; i < 7; ++i) {
		int curx;
		wmove(win, 2, i * boxwidth + leftmargin + 1);
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

	int weeks = getweeks(year, month);
	int firstday = getfirstday(year, month);
	int day = 0;
	for (int i = 0; i < weeks; ++i) {
		for (int j = 0; j < 7; ++j) {
			if (day > 0 || j >= firstday) {
				++day;
			}
			for (int r = 0; r < boxheight; ++r) {
				wmove(win, topmargin + i*boxheight+r,
						leftmargin + j*boxwidth);
				waddch(win, '|');

				if (r == boxheight-1) {
					wattron(win, A_UNDERLINE);
				}
				int cx;
				getyx(win, dump, cx);

				if (day != 0 && r == 0) {
					char date[20];
					sprintf(date, "%d", day);
					waddnstr(win, date, boxwidth-1);
				}
				/* TODO: WRITE EVENT NAMES */

				int newcx;
				for (getyx(win, dump, newcx);
						newcx < cx + boxwidth - 1;
						++newcx) {
					waddch(win, ' ');
				}
				wattroff(win, A_UNDERLINE);
				waddch(win, '|');
			}
		}
	}

	wrefresh(win);
}
