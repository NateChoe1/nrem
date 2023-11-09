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

#ifndef HAVE_DATES
#define HAVE_DATES

#include <stdio.h>
#include <stdint.h>

typedef struct {
	FILE *file;
	char *path;
	uint64_t bit1;
	uint8_t bitn;
} datefile;

int dateopen(char *path, datefile *ret);

struct event {
	int64_t start;
	int64_t end;
	char *name;
	uint64_t id; /* A unique identifier for this event within a file.
	              * Guaranteed to be set by every function in `dates.c` that
	              * takes or returns an event, MUST NOT be set outside of
	              * `dates.c`. Internally just a pointer to this event data
	              * in the file, but don't worry about that. */
};

struct eventlist {
	size_t len;
	size_t alloc;
	struct event *events;
};

int dateadd(struct event *event, datefile *file);

struct eventlist *datesearch(datefile *file, int64_t start, int64_t end);
void freeeventlist(struct eventlist *list);

int dateremove(datefile *file, uint64_t id);

int datedefrag(datefile *file);

#ifdef NREM_TESTS

int datestest(int *passed, int *total);

#endif

#endif
