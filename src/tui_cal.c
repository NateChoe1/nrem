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
	case 'N':
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
	case 'b':
		tui_day += 6;
		break;
	case 'n':
		tui_day += 8;
		break;
	case 'y':
		tui_day -= 8;
		break;
	case 'u':
		tui_day -= 6;
		break;
	case 'L': case 'J':
		tui_day = 0;
		++tui_mon;
		break;
	case 'H': case 'K':
		tui_day = 0;
		--tui_mon;
		break;
	case '\n': case KEY_ENTER:
		*state = VIEWDAY;
		return 0;
	}

	normdate(&tui_day, &tui_mon, &tui_year);
	return 0;
}
