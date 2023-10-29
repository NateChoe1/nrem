/* Usage:
 *  #define STRUCTS \
 *    X(struct 1 name, \
 *      Y(PADDING, name, size) \
 *      Y(U8, name, ~) \
 *      Y(U64, name, ~) \
 *      Y(I64, name, ~) \
 *      Y(STR, name, ~) \
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

#define FTELL(var) \
	{ \
		long pos; \
		if ((pos = ftell(file)) == -1) { \
			return -1; \
		} \
		var = (uint64_t) pos; \
	} \

/* structure definitions */
#define X(name, members) \
	struct name { \
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
	static int read_##name(struct name *ret, FILE *file) { \
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
	static int write_##name(struct name *ret, FILE *file) { \
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
