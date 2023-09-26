#include <time.h>

#include <util.h>
#include <tests.h>

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
	if (month == 2) {
		/* Leap years */
		monthlen = year % 4 ? 28 : year % 100 ? 29 : year % 400 ? 28:29;
	}
	else {
		monthlen = monthlens[month];
	}

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
