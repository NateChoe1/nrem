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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <dateparse.h>

enum VALUE {
	WEEK,
	DAY,
	HOUR,
	MINUTE,
	SECOND
};

static time_t tosec(enum VALUE v);
static int getlocaltime(struct tm *tm);

typedef struct {
	union {
		struct tm tm;
		time_t time;
	};
	enum VALUE value;
	int64_t num;
} TYPE;

static char *literal(char *s, char *r) {
	while (r[0] != '\0') {
		if (s[0] != r[0]) {
			return NULL;
		}
		++s;
		++r;
	}
	return s;
}

#define DECLARE(token) \
static char *match_##token(char *s, TYPE *ret);

#define TS(name, start, process) \
static char *match_##name(char *s, TYPE *ret) { \
	start; \
	process; \
}

#define T(name, process) \
	TS(name,, process)

#define G(rule, match, nomatch) \
	TYPE backup; \
	char *olds = s; \
	memcpy(&backup, ret, sizeof backup); \
	if ((s = rule) != NULL) { \
		match; \
		return s; \
	} \
	else { \
		memcpy(ret, &backup, sizeof backup); \
		s = olds; \
		nomatch; \
		return s; \
	} \

#define R(rule, match, nomatch) \
	G(match_##rule(s, ret), match, nomatch)

#define L(litval, match, nomatch) \
	G(literal(s, litval), match, nomatch)

#define E return NULL;
#define noop ;

DECLARE(date)
DECLARE(absolute_time)
DECLARE(absolute_time_parse)
DECLARE(offsets)
DECLARE(offset)
DECLARE(offset_value)
DECLARE(iso_date)
DECLARE(time)
DECLARE(digit)
DECLARE(hour_minute)
DECLARE(number)
DECLARE(digseq)
DECLARE(value)
DECLARE(digit)

T(date,
	R(absolute_time, R(offsets, noop, noop), E))

T(absolute_time,
	R(absolute_time_parse,
		ret->time = mktime(&ret->tm), E))

TS(absolute_time_parse, if (getlocaltime(&ret->tm)) { E },
	L("now", noop, ret->tm.tm_isdst = -1;
	R(iso_date,
		L(",",
		R(time, noop, E), noop),
	R(time, noop, E))))

T(iso_date,
	R(number, ret->tm.tm_year = ret->num - 1900;
	L("-",
	R(number, ret->tm.tm_mon = ret->num-1; /* Jan = 0 */
	L("-",
	R(number, ret->tm.tm_mday = ret->num, E), E), E), E), E))

T(time,
	R(hour_minute,
		L("AM", noop,
		L("PM", ret->tm.tm_hour += 12, noop)), E))

T(hour_minute, ret->tm.tm_hour = ret->tm.tm_min = ret->tm.tm_sec = 0;
	       R(number, ret->tm.tm_hour = ret->num;
	L(":", R(number, ret->tm.tm_min = ret->num;
	L(":", R(number, ret->tm.tm_sec = ret->num, E), noop), E), noop), E))

T(offsets,
	R(offset, R(offsets, noop, noop), noop))

T(offset,
	L("+", R(offset_value, ret->time += ret->num, E),
	L("-", R(offset_value, ret->time -= ret->num, E), E)))

T(offset_value,
	R(number, R(value, ret->num = tosec(ret->value) * ret->num, E), E))

TS(number, ret->num = 0,
	R(digseq, noop, E))

T(digseq,
	R(digit, R(digseq, noop, noop), E))

T(value,
	L("w", ret->value = WEEK,
	L("d", ret->value = DAY,
	L("h", ret->value = HOUR,
	L("m", ret->value = MINUTE,
	L("s", ret->value = SECOND, E))))))

T(digit,
	L("0", ret->num = ret->num * 10 + 0,
	L("1", ret->num = ret->num * 10 + 1,
	L("2", ret->num = ret->num * 10 + 2,
	L("3", ret->num = ret->num * 10 + 3,
	L("4", ret->num = ret->num * 10 + 4,
	L("5", ret->num = ret->num * 10 + 5,
	L("6", ret->num = ret->num * 10 + 6,
	L("7", ret->num = ret->num * 10 + 7,
	L("8", ret->num = ret->num * 10 + 8,
	L("9", ret->num = ret->num * 10 + 9, E)))))))))))

static time_t tosec(enum VALUE v) {
	switch (v) {
	case SECOND:
		return 1;
	case MINUTE:
		return 60;
	case HOUR:
		return 60*60;
	case DAY:
		return 24*60*60;
	case WEEK:
		return 7*24*60*60;
	default:
		return 0;
	}
}

static int getlocaltime(struct tm *tm) {
	time_t rawtime;

	errno = 0;
	time(&rawtime);
	if (errno != 0) {
		return -1;
	}
	memcpy(tm, localtime(&rawtime), sizeof *tm);
	if (errno != 0) {
		return -1;
	}
	return 0;
}

int64_t parsetime(char *s) {
	TYPE ret;
	if (match_date(s, &ret) == NULL) {
		return 0;
	}
	return ret.time;
}

#pragma GCC diagnostic pop
