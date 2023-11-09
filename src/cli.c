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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <dates.h>
#include <dateparse.h>
#include <interfaces.h>

static int nremcliadd(int argc, char **argv);
static int nremclisearch(int argc, char **argv);
static int nremcliremove(int argc, char **argv);
static int nremclidefrag(int argc, char **argv);

static int printpart(struct event *ev, char *part);

int nremcli(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr,
"Usage: %s [add/search/remove/defrag] [options]\n",
				argv[0]);
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
	if (strcmp(argv[1], "defrag") == 0) {
		return nremclidefrag(argc-1, argv+1);
	}
	fprintf(stderr, "Invalid command %s\n", argv[0]);
	return 1;
}

static int nremcliadd(int argc, char **argv) {
	struct event event;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s [event name] [start] (end)\n",
				argv[1]);
		return 1;
	}

	event.name = argv[1];

	event.start = parsetime(argv[2]);
	if (argc == 3) {
		event.end = event.start;
	}
	else {
		event.end = parsetime(argv[3]);
	}
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
				argv[1]);
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
	for (int i = 0; i < list->len; ++i) {
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
		fprintf(stderr, "Usage: %s [event id]\n", argv[1]);
		return 1;
	}
	dateremove(&f, strtoull(argv[1], NULL, 10));
	return 0;
}

static int nremclidefrag(int argc, char **argv) {
	return datedefrag(&f);
}

static int printpart(struct event *ev, char *part) {
	time_t t = (time_t) ev->start;
	struct tm *tm = localtime(&t);
	if (tm == NULL) {
		fprintf(stderr, "Failed to convert timestamp %ld\n", ev->start);
		return -1;
	}
	if (strcmp(part, "DATE") == 0) {
		printf("%04d-%02d-%02d",
				tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
		return 0;
	}
	if (strcmp(part, "UNIX") == 0) {
		printf("%ld", t);
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
	return -1;
}
