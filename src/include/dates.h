#ifndef HAVE_DATES
#define HAVE_DATES

#include <stdio.h>
#include <stdint.h>

typedef struct {
	FILE *file;
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

#ifdef NREM_TESTS

int datestest(int *passed, int *total);

#endif

#endif
