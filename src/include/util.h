#ifndef HAVE_UTIL
#define HAVE_UTIL

#include <time.h>
#include <stdint.h>

/* All of these functions use 0 as the first day and month */

/* Gets the first weekday of a month */
int getfirstday(int year, int month);

/* Gets the number of weeks needed to display a month */
int getweeks(int year, int month);

/* Gets the length of a month */
int getmonthlen(int year, int month);

void normdate(int *day, int *month, int *year);

int utiltest(int *passed, int *total);

/* Finds the start and end of a day */
static inline time_t findstart(int day, int month, int year) {
	struct tm tm = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 0,
		.tm_mday = day + 1,
		.tm_mon = month,
		.tm_year = year - 1900,
		.tm_isdst = -1,
		.tm_gmtoff = 0,
	};
	return mktime(&tm);
}

/* The end of a given day is 1 second before the start of the next */
static inline time_t findend(int day, int month, int year) {
	++day;
	if (day >= getmonthlen(year, month)) {
		day = 0;
		++month;
		if (month >= 12) {
			month = 0;
			++year;
		}
	}
	return findstart(day, month, year)-1;
}

static inline int isinvalid(int year, int mon, int day,
		int hr, int min, int sec) {
	return mon < 0 || mon >= 12 ||
		day < 0 || day >= getmonthlen(year, mon) ||
		hr < 0 || hr >= 24 ||
		min < 0 || min >= 60 ||
		sec < 0 || sec >= 60;
}

static inline int64_t convtime(int year, int mon, int day,
		int hr, int min, int sec) {
	struct tm tm;
	tm.tm_year = year - 1900;
	tm.tm_mon = mon;
	tm.tm_mday = day + 1;
	tm.tm_hour = hr;
	tm.tm_min = min;
	tm.tm_sec = sec;
	tm.tm_gmtoff = 0;
	tm.tm_isdst = -1;
	return (int64_t) mktime(&tm);
}

#endif
