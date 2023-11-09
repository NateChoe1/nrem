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
