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
	uint64_t time;
	char *name;
};

struct eventlist {
	size_t len;
	size_t alloc;
	struct event *events;
};

int dateadd(struct event *event, datefile *file);

struct eventlist *datesearch(datefile *file, uint64_t start, uint64_t end);
void freeeventlist(struct eventlist *list);

#endif
