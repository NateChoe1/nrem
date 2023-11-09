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

#ifndef HAVE_TESTS
#define HAVE_TESTS

#ifdef NREM_TESTS

#include <stdio.h>

#define NREM_ASSERT(condition) \
	++*total; \
	if (condition) { \
		++*passed; \
	} \
	else { \
		fprintf(stderr, "%s: ASSERT FAILED ON LINE %d: %s\n", \
				__FILE__, __LINE__, #condition); \
	}

#endif

int runtests(int *passed, int *total);

#endif
