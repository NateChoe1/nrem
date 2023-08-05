#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dates.h>

int main(int argc, char **argv) {
	if (argc < 5) {
		fprintf(stderr, "Usage: %s [datefile] [command] [arg1] [arg2]\n", argv[0]);
	}

	datefile f;
	if (dateopen(argv[1], &f)) {
		fprintf(stderr, "Failed to open datefile %s\n", argv[1]);
		return 1;
	}

	if (strcmp(argv[2], "add") == 0) {
		struct event event;
		event.time = strtoull(argv[3], NULL, 10);
		event.name = argv[4];

		if (dateadd(&event, &f)) {
			fprintf(stderr, "Failed to add event\n");
			return 1;
		}

		return 0;
	}
	if (strcmp(argv[2], "search") == 0) {
		struct eventlist *events;
		events = datesearch(&f, strtoull(argv[3], NULL, 10), strtoull(argv[4], NULL, 10));
		for (int i = 0; i < events->len; ++i) {
			printf("%lu\t%lu\t%s\n", events->events[i].time, events->events[i].id, events->events[i].name);
		}
		return 0;
	}
	if (strcmp(argv[2], "remove") == 0) {
		dateremove(&f, strtoull(argv[3], NULL, 10));
		return 0;
	}
	fprintf(stderr, "Invalid command %s\n", argv[2]);
	return 1;
}
