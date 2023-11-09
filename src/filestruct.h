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

/* Usage:
 *  #define STRUCTS \
 *    X(struct 1 name, \
 *      Y(PADDING, name, size) \
 *      Y(U8, name, ~) \
 *      Y(U64, name, ~) \
 *      Y(I64, name, ~) \
 *      Y(STR, name, ~) \
 *      Y(PTR, name, struct which this points to) \
 *    ) \
 *    X(struct 2 name, \
 *      Y(PADDING, name, size) \
 *      Y(U8, name, ~) \
 *      ... \
 *    )
 *
 * Every call to X creates a new struct with a specified name and elements. Note
 * that STR MUST come at the end to avoid memory leaks */

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>

#define READ_FUNC(bits) \
static int readu##bits(uint##bits##_t *ret, FILE *file) { \
	*ret = 0; \
	for (int i = 0; i < sizeof *ret; ++i) { \
		int c; \
 \
		c = fgetc(file); \
		if (c < 0) { \
			return -1; \
		} \
		if (c > 0xff) { \
			return -1; \
		} \
		*ret <<= 8; \
		*ret |= (uint8_t) c; \
	} \
	return 0; \
}
READ_FUNC(8)
READ_FUNC(64)
#undef READ_FUNC

#define WRITE_FUNC(bits) \
static int writeu##bits(uint##bits##_t val, FILE *file) { \
	char buff[sizeof val]; \
	int ret; \
	for (int i = 0; i < sizeof val; ++i) { \
		buff[((int) sizeof val) - i - 1] = (char) (val & 0xff); \
		val >>= 8; \
	} \
	ret = fwrite(buff, sizeof buff, 1, file) == 1 ? 0:-1; \
	return ret; \
}
WRITE_FUNC(8)
WRITE_FUNC(64)
#undef WRITE_FUNC

/* Unsigned -> signed 64 bit int conversion. 0x80000... is zero */
static inline int64_t us64(uint64_t v) {
	if (v & (1llu << 63)) {
		return (int64_t) (v ^ ((uint64_t) 1llu << 63));
	}
	else {
		/* TODO: Find out how to fix this hack */
		if (v == 0) {
			return INT64_MIN;
		}
		return -(int64_t) ((1llu << 63) - v);
	}
}

/* Opposite of us64 */
static inline uint64_t su64(int64_t v) {
	return (1llu << 63) ^ (uint64_t) v;
}

#define CAT_PRIM(a, b) a ## b
#define CAT(a, b) CAT_PRIM(a, b)
#define N(n) CAT(NAMESPACE, n)

#define FTELL(var) \
	{ \
		long pos; \
		if ((pos = ftell(file)) == -1) { \
			return -1; \
		} \
		var = (uint64_t) pos; \
	} \

/* For everything except defragmentation, PTRs and U64s are the same */
#define PTR(name, arg) U64(name, arg)

/* structure definitions */
#define X(name, members) \
	struct N(name) { \
		uint64_t offset; \
		members \
	};
#define Y(type, name, arg) \
	uint64_t name##_pos; \
	type(name, arg)
#define PADDING(name, size) \
	char name[size];
#define U8(name, arg) \
	uint8_t name;
#define U64(name, arg) \
	uint64_t name;
#define I64(name, arg) \
	int64_t name;
#define STR(name, arg) \
	uint64_t name##_len; \
	char *name;

STRUCTS

#undef X
#undef Y
#undef PADDING
#undef U8
#undef U64
#undef I64
#undef STR

/* read functions */
#define X(name, members) \
	static int CAT(read_, N(name))(struct N(name) *ret, \
			FILE *file) { \
		FTELL(ret->offset); \
		members \
		return 0; \
	}
#define Y(type, name, arg) \
	FTELL(ret->name##_pos) \
	type(name, arg)
#define PADDING(name, size) \
	if (fread(ret->name, size, 1, file) < 1) { \
		return -1; \
	}
#define U8(name, arg) \
	if (readu8(&ret->name, file)) { \
		return -1; \
	}
#define U64(name, arg) \
	if (readu64(&ret->name, file)) { \
		return -1; \
	}
#define I64(name, arg) \
	{ \
		uint64_t tmp; \
		if (readu64(&tmp, file)) { \
			return -1; \
		} \
		ret->name = us64(tmp); \
	}
/* XXX: If there is a STR member, and then something else fails later, that's a
 * memory leak. Don't let that happen please. */
#define STR(name, arg) \
	if (readu64(&ret->name##_len, file)) { \
		return -1; \
	} \
	if ((ret->name = malloc(ret->name##_len + 1)) == NULL) { \
		return -1; \
	} \
	if (fread(ret->name, ret->name##_len, 1, file) < 1) { \
		free(ret->name); \
		return -1; \
	} \
	ret->name[ret->name##_len] = '\0';

STRUCTS

#undef X
#undef Y
#undef PADDING
#undef U8
#undef U64
#undef I64
#undef STR

/* write functions */
#define X(name, members) \
	static int CAT(write_, N(name))(struct N(name) *ret, \
			FILE *file) { \
		FTELL(ret->offset) \
		members \
		if (fflush(file) == EOF) { \
			return -1; \
		} \
		return 0; \
	}
#define Y(type, name, arg) \
	FTELL(ret->name##_pos) \
	type(name, arg)
#define PADDING(name, len) \
	if (fwrite(ret->name, len, 1, file) < 1) { \
		return -1; \
	}
#define U8(name, arg) \
	if (writeu8(ret->name, file)) { \
		return -1; \
	}
#define U64(name, arg) \
	if (writeu64(ret->name, file)) { \
		return -1; \
	}
#define I64(name, arg) \
	if (writeu64(su64(ret->name), file)) {  \
		return -1; \
	}
#define STR(name, arg) \
	if (writeu64(ret->name##_len, file)) { \
		return -1; \
	} \
	if (fwrite(ret->name, ret->name##_len, 1, file) < 1) { \
		return -1; \
	}

STRUCTS

#undef X
#undef Y
#undef PADDING
#undef U8
#undef U64
#undef I64
#undef STR

#undef PTR

/* To defragment a file, we just take the "root" element (in the case of nrem,
 * the file header), and follow the pointers. For each structure, if we've seen
 * it before, we know where to point to. If we haven't, we just write the
 * structure and point to that.
 *
 * This requres two things: a list of all the pointers we've seen, and a
 * defragmentation function. That list of pointers is just a hashmap, which is
 * implemented below.
 * */

/* Hashmaps */
#ifndef FILESTRUCT_ONCE
#define FILESTRUCT_MAP_SPACE 131 /* arbitrary prime number */

struct filestruct_map_item {
	uint64_t key;
	uint64_t value;
	struct filestruct_map_item *next;
};

/* a filestruct_map is an array of FILESTRUCT_MAP_SPACE filestruct_map_items,
 * where a filestruct_map_item is a linkedlist of keys and values */
typedef struct filestruct_map_item *filestruct_map[FILESTRUCT_MAP_SPACE];

static void init_filestruct_map(filestruct_map map) {
	for (int i = 0; i < FILESTRUCT_MAP_SPACE; ++i) {
		map[i] = NULL;
	}
}

static void free_filestruct_map(filestruct_map map) {
	for (int i = 0; i < FILESTRUCT_MAP_SPACE; ++i) {
		struct filestruct_map_item *iter = map[i];
		while (iter != NULL) {
			struct filestruct_map_item *next = iter->next;
			free(iter);
			iter = next;
		}
	}
}

static uint64_t filestruct_map_search(filestruct_map map, uint64_t key) {
	const int index = (int) (key % FILESTRUCT_MAP_SPACE);
	struct filestruct_map_item *iter = map[index];
	while (iter != NULL) {
		if (iter->key == key) {
			return iter->value;
		}
		iter = iter->next;
	}
	return UINT64_MAX;
}

static int filestruct_map_contains(filestruct_map map, uint64_t key) {
	return filestruct_map_search(map, key) != UINT64_MAX;
}

static int add_filestruct_map(filestruct_map map,
		uint64_t key, uint64_t value) {
	if (filestruct_map_contains(map, key)) {
		return 0;
	}

	const int index = (int) (key % FILESTRUCT_MAP_SPACE);
	struct filestruct_map_item *newitem;
	if ((newitem = malloc(sizeof *newitem)) == NULL) {
		return -1;
	}
	newitem->key = key;
	newitem->value = value;
	newitem->next = map[index];
	map[index] = newitem;
	return 0;
}

#undef FILESTRUCT_MAP_SPACE

static inline int seek(FILE *file, uint64_t position, int whence) {
	if (position > LONG_MAX) {
		return -1;
	}
	return fseek(file, (long) position, whence);
}
static inline int tell(FILE *file, uint64_t *ret) {
	long val = ftell(file);
	if (val < 0) {
		return -1;
	}
	*ret = (uint64_t) val;
	return 0;
}

#endif

/* Hashmap definitions */
#define X(name, members) \
	static filestruct_map CAT(N(name), _map) = {0};
STRUCTS
#undef X

#define X(name, members) \
	free_filestruct_map(CAT(N(name), _map)); \
	init_filestruct_map(CAT(N(name), _map));
static int N(reset_hashmaps)() {
	STRUCTS
	return 0;
}
#undef X

/* Naive defrag declarations */
#define X(name, members) \
static uint64_t CAT(defragnaive_, N(name))(uint64_t ptr, FILE *in, FILE *out);
STRUCTS
#undef X

/* Naive defrag functions */
#define X(name, members) \
	static uint64_t CAT(defragnaive_, N(name)) \
			(uint64_t ptr, FILE *in, FILE *out) { \
		filestruct_map * map = &CAT(N(name), _map); \
		uint64_t ret; \
		struct N(name) orig; \
		/* No duplicate structures */ \
		if (filestruct_map_contains(*map, ptr)) { \
			return filestruct_map_search(*map, ptr); \
		} \
\
		if (seek(in, ptr, SEEK_SET) || \
		    seek(out, 0, SEEK_END) || \
		    tell(out, &ret) || \
		    add_filestruct_map(*map, ptr, ret)) { \
			goto error; \
		} \
\
		/* First, naively copy from input to output */\
		if (CAT(read_, N(name))(&orig, in) || \
		    CAT(write_, N(name))(&orig, out)) { \
			goto error; \
		} \
		/* Then, correct for pointers */ \
		members \
		return ret; \
error: \
		return UINT64_MAX; \
	}
#define Y(type, name, arg) \
	do { \
		type(name, arg) \
	} while (0);

/* These can all be copied naively without issues */
#define PADDING(name, len) ;
#define U8(name, arg) ;
#define U64(name, arg) ;
#define I64(name, arg) ;
#define STR(name, arg) ;

#define PTR(name, type) \
	uint64_t dstval; \
	/* Sanitize pointers */ \
	if (orig.name == 0) { \
		break; \
	} \
	dstval = CAT(defragnaive_, N(type))(orig.name, in, out); \
	if (seek(out, orig.name##_pos, SEEK_SET) || \
	    writeu64(dstval, out)) { \
		goto error; \
	}

STRUCTS

#undef X
#undef Y
#undef PADDING
#undef U8
#undef U64
#undef I64
#undef STR
#undef PTR

#define X(name, members) \
	static int CAT(defrag_, N(name))(uint64_t ptr, FILE *in, FILE *out) { \
		if (CAT(NAMESPACE, reset_hashmaps)()) { \
			return -1; \
		} \
		return CAT(defragnaive_, N(name))(ptr, in, out) == UINT64_MAX; \
	}
STRUCTS
#undef X

#undef FTELL
#undef CAT_PRIM
#undef CAT
#undef N

#ifndef FILESTRUCT_ONCE
#define FILESTRUCT_ONCE
#endif
