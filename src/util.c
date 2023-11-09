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

#include <time.h>

#include <util.h>
#include <tests.h>
#include <interfaces.h>

static int monthlens[] = {
	31,
	-1, /* February, we need to check for leap years */
	31,
	30,
	31,
	30,
	31,
	31,
	30,
	31,
	30,
	31
};

int getfirstday(int year, int month) {
	struct tm tm = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 12,
		/* tm_isdst may be changed, so we set the hour to 12 so that it
		 * makes no difference */
		.tm_mday = 1,
		.tm_mon = month,
		.tm_year = year - 1900,
		.tm_isdst = 0,
		.tm_gmtoff = 0,
		/* If the date is invalid, wday is unchanged. I abuse this
		 * fact. */
		.tm_wday = -1,
	};
	mktime(&tm);
	return tm.tm_wday;
}

int getweeks(int year, int month) {
	int monthlen, firstday, ret;
	if (month < 0 || month >= 12) {
		return -1;
	}
	monthlen = getmonthlen(year, month);

	if ((firstday = getfirstday(year, month)) < 0) {
		return -1;
	}

	/* First week */
	ret = 1;
	monthlen -= 7 - firstday;

	/* Middle weeks*/
	ret += monthlen / 7;
	monthlen %= 7;

	/* Possible last week */
	ret += monthlen ? 1:0;

	return ret;
}

int getmonthlen(int year, int month) {
	if (month == 1) {
		/* Leap years */
		return year % 4 ? 28 : year % 100 ? 29 : year % 400 ? 28:29;
	}
	else {
		return monthlens[month];
	}
}

void normdate(int *day, int *month, int *year) {
	for (;;) {
		/* Normalize month */
		if (*month < 0) {
			*month += 12;
			--*year;
			continue;
		}
		if (*month >= 12) {
			*month -= 12;
			++*year;
			continue;
		}

		/* Normalize day */
		if (*day < 0) {
			if (--*month < 0) {
				--*year;
				*month += 12;
			}
			*day = getmonthlen(*year, *month) + *day;
			continue;
		}
		int monthlen = getmonthlen(*year, *month);
		if (*day >= monthlen) {
			*day -= monthlen;
			++*month;
			continue;
		}

		/* months and days are normalized, end */
		break;
	}
}

#ifdef NREM_TESTS
int utiltest(int *passed, int *total) {
	NREM_ASSERT(getfirstday(2023, 8) == 5);
	NREM_ASSERT(getweeks(2023, 8) == 5);

	NREM_ASSERT(getfirstday(2023, 11) == 5);
	NREM_ASSERT(getweeks(2023, 11) == 6);

	return 0;
}
#else
int utiltest(int *passed, int *total) {
	++*total;
	return 1;
}
#endif
