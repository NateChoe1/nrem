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

#ifndef HAVE_TUI
#define HAVE_TUI

#include <curses.h>

#define KEY_ESCAPE '\x1b'

enum tui_state {
	VIEWCAL,
	VIEWDAY,
	NEWEVENT,
	DONE,
};

extern int tui_day, tui_mon, tui_year;

int tui_cal(enum tui_state *state, WINDOW *win);
int tui_viewday(enum tui_state *state, WINDOW *win);
int tui_newevent(enum tui_state *state, WINDOW *win);

int tui_viewday_reset(WINDOW *win);
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
