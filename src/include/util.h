#ifndef HAVE_UTIL
#define HAVE_UTIL

/* Gets the first weekday of a month */
int getfirstday(int year, int month);

/* Gets the number of weeks needed to display a month */
int getweeks(int year, int month);

int utiltest(int *passed, int *total);

#endif
