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
	if (month == 2) {
		/* Leap years */
		return year % 4 ? 28 : year % 100 ? 29 : year % 400 ? 28:29;
	}
	else {
		return monthlens[month];
	}
}

time_t findstart(int day, int month, int year) {
	/* Best guess of the start */
	struct tm baseline = {
		.tm_sec = 0,
		.tm_min = 0,
		.tm_hour = 0,
		.tm_mday = day + 1,
		.tm_mon = month,
		.tm_year = year - 1900,
		.tm_isdst = -1,
		.tm_gmtoff = nowb.tm_gmtoff,
	};
	time_t guess = mktime(&baseline);

	/* We assume that the guess at least has the same GMT offset as the real
	 * date */
	struct tm *real = localtime(&guess);
	real->tm_sec = real->tm_min = real->tm_hour = 0;
	real->tm_mday = day + 1;
	real->tm_mon = month;
	real->tm_year = year - 1900;
	
	return mktime(real);
}

/* The end of a given day is 1 second before the start of the next */
time_t findend(int day, int month, int year) {
	++day;
	if (day >= getmonthlen(year, month)) {
		day = 0;
		++month;
		if (month >= 12) {
			month = 0;
			++year;
		}
	}
	return findstart(day+1, month, year)-1;
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
