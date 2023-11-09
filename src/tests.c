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

#include <util.h>
#include <tests.h>
#include <dates.h>

#ifdef NREM_TESTS

int runtests(int *passed, int *total) {
	int ret;

	ret = 0;

	if (datestest(passed, total)) {
		ret = 1;
	}
	if (utiltest(passed, total)) {
		ret = 1;
	}

	return ret;
}

#else

int runtests(int *passed, int *total) {
	*passed = 0;
	*total = 1;
	return 1;
}

#endif
