#include <curses.h>

#include <tui.h>
#include <util.h>
#include <dates.h>
#include <interfaces.h>

static struct eventlist *events;
static int selected;
static int scratch;

static int refreshevents() {
	events = datesearch(&f,
			findstart(tui_day, tui_mon, tui_year),
			findend(tui_day, tui_mon, tui_year));
	return events == NULL;
}

int tui_viewday_reset(WINDOW *win) {
	selected = 0;
	return refreshevents();
}

int tui_viewday(enum tui_state *state, WINDOW *win) {
	int w;
	if (events->len == 0) {
		*state = VIEWCAL;
		goto cstate;
	}
	getmaxyx(win, scratch, w);
	wclear(win);
	for (int i = 0; i < events->len; ++i) {
		char line[256];
		struct tm *start;

		if (i == selected) {
			wattron(win, COLOR_PAIR(COL_BRIGHT));
		}

		start = localtime(&events->events[i].start);

		snprintf(line, sizeof line, "%2d:%02d:%02d - %s",
				start->tm_hour, start->tm_min, start->tm_sec,
				events->events[i].name);
		line[sizeof line - 1] = '\0';

		mvwaddnstr(win, i, 0, line, w);

		if (i == selected) {
			wattroff(win, COLOR_PAIR(COL_BRIGHT));
		}
	}
	wmove(win, selected, 0);
	wrefresh(win);

	int c = getch();

	switch (c) {
	case 'j': case KEY_DOWN:
		selected = (selected + 1) % (int) events->len;
		break;
	case 'k': case KEY_UP:
		selected = (selected - 1 + (int) events->len)
			% (int) events->len;
		break;
	case 'n':
		*state = NEWEVENT;
		goto cstate;
	case 'd':
		if (dateremove(&f, events->events[selected].id)) {
			return 1;
		}
		freeeventlist(events);
		return refreshevents();
	case 'q': case KEY_ESCAPE:
		*state = VIEWCAL;
		goto cstate;
	}

	return 0;
cstate:
	freeeventlist(events);
	return 0;
}
