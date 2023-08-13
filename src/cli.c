#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dates.h>
#include <dateparse.h>
#include <interfaces.h>

static int nremcliadd(int argc, char **argv);
static int nremclisearch(int argc, char **argv);
static int nremcliremove(int argc, char **argv);

static int printpart(struct event *ev, char *part);

static datefile f;

int nremcli(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [add/search/remove] [options]\n",
				argv[0]);
		return 1;
	}

	char path[256];
	char *env;
	if ((env = getenv("DATEFILE")) != NULL) {
		strncpy(path, env, sizeof path - 1);
		path[sizeof path - 1] = '\0';
	}
	else if ((env = getenv("HOME")) != NULL) {
		snprintf(path, sizeof path, "%s/.config/nrem/datefile", env);
	}
	else {
		fputs("Failed to get datefile path, set $DATEFILE\n", stderr);
		return 1;
	}
	if (dateopen(path, &f)) {
		fprintf(stderr, "Failed to open datefile %s\n", path);
		return 1;
	}

	if (strcmp(argv[1], "add") == 0) {
		return nremcliadd(argc-1, argv+1);
	}
	if (strcmp(argv[1], "search") == 0) {
		return nremclisearch(argc-1, argv+1);
	}
	if (strcmp(argv[1], "remove") == 0) {
		return nremcliremove(argc-1, argv+1);
	}
	fprintf(stderr, "Invalid command %s\n", argv[0]);
	return 1;
}

static int nremcliadd(int argc, char **argv) {
	struct event event;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [time] [event name]\n", argv[0]);
		return 1;
	}
	event.time = parsetime(argv[1]);
	event.name = argv[2];
	if (dateadd(&event, &f)) {
		fputs("Failed to add event\n", stderr);
		return 1;
	}
	return 0;
}

static int nremclisearch(int argc, char **argv) {
	char *format = "DATE,TIME12,NAME";
	struct eventlist *list;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [start] [end] (format)\n",
				argv[0]);
		return 1;
	}
	if (argc >= 4) {
		format = argv[3];
	}

	list = datesearch(&f, parsetime(argv[1]), parsetime(argv[2]));
	if (list == NULL) {
		fputs("Search failed\n", stderr);
		return 1;
	}
	/* This code is awful, do not copy it */
	for (long i = 0; i < list->len; ++i) {
		char part[50];
		int partlen;
		int pcount;
		pcount = partlen = 0;
		for (int j = 0;; ++j) {
			if (partlen >= sizeof part) {
				fputs("Oversized format component detected, "
						"quitting\n", stderr);
				return 1;
			}

			char c = format[j];
			if (c == ',' || c == '\0') {
				part[partlen] = '\0';
				partlen = 0;

				if (pcount != 0) {
					putchar('\t');
				}
				if (printpart(list->events + i, part)) {
					return 1;
				}

				++pcount;
				if (c == '\0') {
					putchar('\n');
					break;
				}
			}
			else {
				part[partlen++] = c;
			}
		}
	}
	return 0;
}

static int nremcliremove(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [event id]\n", argv[0]);
		return 1;
	}
	dateremove(&f, strtoull(argv[1], NULL, 10));
	return 0;
}

static int printpart(struct event *ev, char *part) {
	time_t t = (time_t) ev->time;
	struct tm *tm = localtime(&t);
	if (strcmp(part, "DATE") == 0) {
		printf("%04d-%02d-%02d",
				tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
		return 0;
	}
	if (strcmp(part, "TIME12") == 0) {
		int hour = tm->tm_hour % 12;
		if (hour == 0) {
			hour += 12;
		}
		printf("%02d:%02d:%02d %s", hour, tm->tm_min, tm->tm_sec,
				(tm->tm_hour < 12) ? "AM":"PM");
		return 0;
	}
	if (strcmp(part, "TIME24") == 0) {
		printf("%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
		return 0;
	}
	if (strcmp(part, "NAME") == 0) {
		printf("%s", ev->name);
		return 0;
	}
	if (strcmp(part, "ID") == 0) {
		printf("%llu", (unsigned long long) ev->id);
		return 0;
	}
	fprintf(stderr, "Invalid format part %s\n", part);
	return 1;
}
