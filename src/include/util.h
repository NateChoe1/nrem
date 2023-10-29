#ifndef HAVE_UTIL
#define HAVE_UTIL

#include <time.h>

/* All of these functions use 0 as the first day and month */

/* Gets the first weekday of a month */
int getfirstday(int year, int month);

/* Gets the number of weeks needed to display a month */
int getweeks(int year, int month);

/* Gets the length of a month */
int getmonthlen(int year, int month);

/* Finds the start and end of a day */
time_t findstart(int day, int month, int year);
time_t findend(int day, int month, int year);

int utiltest(int *passed, int *total);

#endif
