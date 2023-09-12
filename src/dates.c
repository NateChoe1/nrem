#include <string.h>
#include <stdlib.h>

#include <dates.h>
#include <tests.h>

/* datefile format
 * NOTE: all integer values are stored in big endian (most significant byte
 *   first)
 *
 * datefile is a file format that associates dates with text events. The format
 * is designed so that adding events at random timestamps can be done in O(1)
 * and reading events within a certain time range can be done in O(n) time where
 * n is the number of events.
 *
 * datefiles begin with the following header:
 *
 *     struct {
 *         char magic_number[8];        Always "datefile"
 *         uint64_t bit1;               Location of the root node
 *         uint8_t bitn;                Bits per timestamp
 *         char reserved[16];           Ignored for now, MUST be all 0s
 *     };
 *
 * datefiles contain a binary tree with a max depth of `bitn`. Events are placed
 * into a linked list represented in this binary tree as a node whose position
 * is determined by the binary representation of the UNIX timestamp of the
 * event. An event inside of a node indicates that all the timestamps below
 * that node contain that event.
 *
 *     EXAMPLE EVENT INSERTION
 *       1. The UNIX timestamp of the event is obtained (truncated to 8 bits for
 *          brevity)
 *             75 seconds after epoch
 *       2. The event is converted to binary
 *             0b01001011 seconds after epoch
 *       3. A set of bit masks that encompasses the entire event is determined
 *             0b01001011/8
 *       4. A node corresponding to each of the masks is found
 *             Starting from the root node, go:
 *               left, right, left, left, right, left, right, right
 *               0     1      0      0    1      0     1      1
 *       5. The event is added to the beginning of a linked list located at each
 *          node
 *
 *     EXAMPLE EVENT INSERTION
 *       1. The UNIX timestamp of the event is obtained (truncated to 8 bits for
 *          brevity)
 *             32-95 seconds after epoch (this event lasts 63 seconds)
 *       2. The event is converted to binary
 *             0b00100000-0b01111111 seconds after epoch
 *       3. A set of bit masks that encompasses the entire event is determined
 *             0b00100000/3
 *             0b01000000/2
 *       4. A node corresponding to each of the masks is found
 *             Starting from the root node, go:
 *               left, left, right
 *               0     0     1
 *             Starting from the root node, go:
 *               left, right
 *               0     1
 *       5. The event is added to the beginning of a linked list located at each
 *          node
 *
 * Node representation:
 *     struct {
 *         uint64_t child0;
 *         uint64_t child1;
 *         uint64_t event;
 *         char reserved[16];
 *     };
 *
 * Event representation:
 *     struct {
 *         uint64_t next;               The next event that occurs at this same
 *                                      time stamp
 *         uint64_t prev;               The location of the pointer to this
 *                                      event
 *         uint64_t nextsm;             A pointer to the next event with the
 *                                      same event data
 *         uint64_t ptr;                A pointer to event data
 *         char reserved[16];
 *     }
 *
 * Event data representation:
 *         uint64_t functions;          Unused for now, MUST be 0
 *         uint64_t firstev;            The first event that points to this data
 *
 *         int64_t start;               The UNIX timestamp of the start of the
 *                                      event, stored as a conversion by su64()
 *         int64_t end;                 The UNIX timestamp of the end of the
 *                                      event, stored as a conversion by su64()
 *
 *         uint64_t len;                The length of this event name
 *         char event[len];             The event name itself
 *     };
 * */

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
		*ret <<= 8; \
		*ret |= c; \
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
		buff[sizeof val - i - 1] = val & 0xff; \
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
		return v ^ (1llu << 63);
	}
	else {
		/* This specific case is implementation-defined behavior */
		if (v == 0) {
			return -0x8000000000000000llu;
		}
		return -(int64_t) ((1llu << 63) - v);
	}
}

/* Opposite of us64 */
static inline uint64_t su64(int64_t v) {
	return (1llu << 63) ^ (uint64_t) v;
}

/* Creates a datefile. This function will truncate `path` */
static int datecreate(char *path, datefile *ret);

/* Add a date with a certain prefix */
static int dateaddbit(datefile *file, uint64_t prefix, int precision,
		uint64_t dataptr, uint64_t nextsmptr, uint64_t *newnextsmptr);

static int datesearchrecursive(datefile *file, struct eventlist *events,
		uint64_t start, uint64_t end,
		uint64_t prefix, uint8_t precision,
		uint64_t ptr);
static int readtime(datefile *file, struct eventlist *events, uint64_t ptr);
static int eventremove(datefile *file, uint64_t id, uint64_t *nextsmret);

static inline uint64_t fill1(int n) {
	/* Undefined behavior :( */
	if (n >= 64) {
		return -1;
	}
	return (0ull << n)-1;
}

int dateopen(char *path, datefile *ret) {
	FILE *file = fopen(path, "rb+");
	if (file == NULL) {
		return datecreate(path, ret);
	}

	char magic[8];
	if (fread(magic, sizeof magic, 1, file) < 1) {
		return -1;
	}
	if (memcmp(magic, "datefile", sizeof magic)) {
		return -1;
	}

	ret->file = file;
	if (readu64(&ret->bit1, file)) {
		return -1;
	}
	if (readu8(&ret->bitn, file)) {
		return -1;
	}

	return 0;
}

static int datecreate(char *path, datefile *ret) {
	FILE *file = fopen(path, "w+");

	if (file == NULL ||
	    fwrite("datefile", 8, 1, file) < 1) {
		return -1;
	}

	/* Store the bit1 position in the header to write to later */
	long bit1header = ftell(file);
	if (bit1header == -1) {
		return -1;
	}

	/* Write a temporary bit1 position value */
	if (writeu64(0, file)) {
		return -1;
	}

	/* 64 bits per timestamp */
	if (writeu8(64, file)) {
		return -1;
	}

	/* Write reserved area */
	for (int i = 0; i < 16; ++i) {
		if (fputc(0, file) == EOF) {
			return -1;
		}
	}

	/* Find bit1 */
	long bit1loc = ftell(file);
	if (bit1loc == -1) {
		return -1;
	}

	/* Write bit1 */
	for (int i = 0; i < 40; ++i) {
		if (fputc(0, file) == EOF) {
			return -1;
		}
	}

	if (fseek(file, bit1header, SEEK_SET) == -1) {
		return -1;
	}
	if (writeu64(bit1loc, file)) {
		return -1;
	}

	ret->file = file;
	ret->bit1 = bit1loc;
	ret->bitn = 64;

	return 0;
}

static int dateaddbit(datefile *file, uint64_t prefix, int precision,
		uint64_t dataptr, uint64_t nextsmptr, uint64_t *newnextsmptr) {
	if (fseek(file->file, file->bit1, SEEK_SET) == -1) {
		return -1;
	}

	uint64_t mask = 1llu << (file->bitn-1);

	for (int i = 0; i < precision; ++i) {
		int bit = !!(prefix & mask);
		uint64_t next;
		long field_pos;

		/* Find the field */
		if (fseek(file->file, bit ? 8:0, SEEK_CUR) == -1) {
			return -1;
		}

		/* Store the field position */
		if ((field_pos = ftell(file->file)) == -1) {
			return -1;
		}

		/* Read the field */
		if (readu64(&next, file->file)) {
			return -1;
		}

		/* If the field is empty (the node doesn't exist yet), create
		 * it */
		if (next == 0) {
			long new_pos;

			/* Go to the end of the file (where new nodes are
			 * added) */
			if (fseek(file->file, 0, SEEK_END) == -1) {
				return -1;
			}

			/* Remember the location of the new node*/
			if ((new_pos = ftell(file->file)) == -1) {
				return -1;
			}

			/* Initialize the new node with zeroes */
			for (int j = 0; j < 40; ++j) {
				if (fputc(0, file->file) == EOF) {
					return -1;
				}
			}

			/* Update the parent node's child pointer */
			if (fseek(file->file, field_pos, SEEK_SET) == -1) {
				return -1;
			}
			if (writeu64((uint64_t) new_pos, file->file)) {
				return -1;
			}
			
			/* Return to the new node position */
			if (fseek(file->file, new_pos, SEEK_SET) == -1) {
				return -1;
			}
		}
		/* If the child node does exist, just go to it */
		else {
			if (fseek(file->file, next, SEEK_SET) == -1) {
				return -1;
			}
		}

		mask >>= 1;
	}

	/* We are at the dest node */

	long headptr, evptr;
	uint64_t oldhead;

	/* Seek to the timestamp linked list head */
	if (fseek(file->file, 16, SEEK_CUR) == -1) {
		return -1;
	}

	/* Mark the location of the timestamp head pointer */
	if ((headptr = ftell(file->file)) == -1) {
		return -1;
	}

	/* Get the old timestamp head location */
	if (readu64(&oldhead, file->file)) {
		return -1;
	}

	long newnextsm;

	/* Write event data */
	if (fseek(file->file, 0, SEEK_END) == -1 || /* Get to the EOF */
	    (evptr = ftell(file->file)) == -1 || /* Get event location */
	    writeu64(oldhead, file->file) ||  /* next (prepend to linkedlist) */
	    writeu64(headptr, file->file) ||  /* prev (head pointer) */
	    (newnextsm = ftell(file->file)) == -1 || /* Store nextsm loc */
	    writeu64(0, file->file) ||        /* nextsm (tmp 0 val) */
	    writeu64(dataptr, file->file)) {  /* dataptr */
		return -1;
	}
	/* Padding */
	for (int i = 0; i < 16; ++i) {
		if (fputc(0, file->file) == EOF) {
			return -1;
		}
	}
	*newnextsmptr = newnextsm;

	/* Update the old timestamp head's prev value */
	if (oldhead != 0 && 
		(fseek(file->file, oldhead+8, SEEK_SET) == -1 ||
	         writeu64(evptr, file->file))) {
		return -1;
	}

	/* Update timestamp head pointer */
	if (fseek(file->file, headptr, SEEK_SET) == -1 ||
	    writeu64(evptr, file->file)) {
		return -1;
	}

	/* Update nextsm */
	if (fseek(file->file, nextsmptr, SEEK_SET) == -1 ||
	    writeu64(evptr, file->file)) {
		return -1;
	}

	return 0;

}

int dateadd(struct event *event, datefile *file) {
	if (file->bitn > 64) {
		return -1;
	}

	if (fseek(file->file, 0, SEEK_END) == -1) {
		return -1;
	}

	long id;
	if ((id = ftell(file->file)) == -1) {
		return -1;
	}

	uint64_t nextsmptr;
	size_t eventlen = strlen(event->name);

	/* Write event data */
	if (writeu64(0, file->file) ||    /* functions */
	    (nextsmptr = ftell(file->file)) == -1 || /* firstev */
	    writeu64(0, file->file) ||
	    writeu64(su64(event->start), file->file) ||
	    writeu64(su64(event->end), file->file) ||
	    writeu64(eventlen, file->file) ||
	    fwrite(event->name, eventlen, 1, file->file) < 1 ||
	    fflush(file->file) == EOF) {
		return -1;
	}

	event->id = id;

	uint64_t start = su64(event->start);
	uint64_t end = su64(event->end);
	uint64_t lower = start;

	/* Some dark magic I thought of at midnight while trying to go to
	 * sleep. This code generates the smallest possible set of binary string
	 * prefixes that fully encompasses a range. Here's how it works:
	 *
	 * Let's simplify this problem and try to find the smallest set of
	 * prefixes that goes from some number `n` to 0xffffff... Let's say,
	 * for example, n=0b00110100
	 *
	 * You can find the lower bound of a binary string prefix by setting all
	 * the non-captured bits to zero and the upper bound by setting all the
	 * non-captured bits to one. Since we only care about the lower bound,
	 * we want the largest possible prefix where all the non-captured bits
	 * are already zero. In this case, 0b00110100/6
	 *
	 * That string captures 0b00110100-0b00110111 inclusive. Now the goal is
	 * to find the set of prefixes that capture 0b00111000-0b11111111. In
	 * this case, that's 0b00111000/5.
	 *
	 * We can continue on like this forever. That last group captures
	 * 0b00111000-0b00111111, so our next prefix has to start at 0b01000000.
	 * Algorithmically, the next prefix is always the least precise prefix
	 * where the non-captured bits of the lower bound are all zero. Then,
	 * we fill the non-captured bits with ones, add one, and that's the new
	 * lower bound. That code looks like this:
	 *
	 * 	uint64_t lower = start;
	 * 	for (;;) {
	 * 		uint64_t bit = 1;
	 * 		int precision = 64;
	 *
	 * 		// While the iterated bit is zero
	 * 		while (lower ^ bit > lower) {
	 *			// For efficiency, we fill the uncaptured bits
	 *			// with ones as we check them
	 *			lower ^= bit;
	 *
	 * 			// Iterate to the next bit
	 *			bit <<= 1;
	 *			--precision;
	 * 		}
	 * 		
	 * 		report_prefix(lower, precision);
	 * 		++lower;
	 * 	}
	 *
	 * The code below is just a golfed version of this with a check to make
	 * sure we don't go too big. The separate `bit` and `precision`
	 * variables are combined, and the while loop is turned to a for.
	 *
	 * This code runs in O(b^2) time where b is the width of the integer.
	 * This is because for each prefix we may loop b times, and there may be
	 * b prefixes. This can be seen in the prefixes for 0x1-0xffffff...
	 *
	 * In theory this could be reduced to O(b log b) by using binary search
	 * to find the maximum precision of a prefix, or even O(b) by caching
	 * the precision of the previous prefix, but that would make the code
	 * far more complicated.
	 * */
	while (lower <= end) {
		int precision;
		uint64_t newnextsmptr;
		for (precision = 0; /* For each bit */
				/* Make sure we're not leaving uncaptured
				 * ones */
				(lower ^ (1llu << precision)) > lower &&
				/* Make sure we don't go too high */
				(lower ^ (1llu << precision)) <= end;
				++precision) {
			/* Fill uncaptured bits with ones */
			lower ^= (1llu << precision);
		}
		/* Report a prefix */
		if (dateaddbit(file, lower, 64-precision,
					id, nextsmptr, &newnextsmptr)) {
			return -1;
		}
		nextsmptr = newnextsmptr;
		/* Update lower bound */
		++lower;
	}

	return 0;
}

struct eventlist *datesearch(datefile *file, int64_t start, int64_t end) {
	struct eventlist *ret;
	int status;
	if ((ret = malloc(sizeof *ret)) == NULL) {
		return NULL;
	}
	ret->len = 0;
	ret->alloc = 20;
	if ((ret->events = malloc(ret->alloc * sizeof *ret->events)) == NULL) {
		free(ret);
		return NULL;
	}

	status = datesearchrecursive(file, ret, su64(start), su64(end),
			0, 0, file->bit1);
	if (status) {
		freeeventlist(ret);
		return NULL;
	}

	return ret;
}

static int datesearchrecursive(datefile *file, struct eventlist *events,
		uint64_t start, uint64_t end,
		uint64_t prefix, uint8_t precision,
		uint64_t ptr) {
	/* Sanitize invalid pointers */
	if (ptr == 0) {
		return 0;
	}

	/* Earliest and latest possible times given the current prefix and
	 * precision */
	uint64_t early, late;

	/* Make sure we're still within the bounds */
	early = prefix;
	late = prefix | fill1(file->bitn-precision);
	if (early > end || late < start) {
		return 0;
	}

	/* Sanity check */
	if (precision > file->bitn) {
		return -1;
	}

	/* Read the fields */
	if (fseek(file->file, ptr, SEEK_SET) == -1) {
		return -1;
	}
	uint64_t child0, child1, headptr;
	if (readu64(&child0, file->file) ||
	    readu64(&child1, file->file) ||
	    readu64(&headptr, file->file)) {
		return -1;
	}

	if (headptr != 0) {
		readtime(file, events, headptr);
	}

	/* Recurse with one more level of precision */
	if (datesearchrecursive(file, events, start, end,
				prefix,
				precision+1, child0) ||
	    datesearchrecursive(file, events, start, end,
				prefix | (1lu << (file->bitn-precision-1)),
				precision+1, child1)) {
		return -1;
	}

	return 0;
}

static int readtime(datefile *file, struct eventlist *events, uint64_t ptr) {
	if (fseek(file->file, ptr, SEEK_SET) == -1) {
		return -1;
	}

	for (;;) {
		/* Reallocate events if OOM */
		if (events->len >= events->alloc) {
			struct event *newevents;
			size_t newalloc;
			newalloc = events->alloc * 2;
			newevents = realloc(events->events,
					newalloc * sizeof *newevents);
			if (newevents == NULL) {
				return -1;
			}
			events->alloc = newalloc;
			events->events = newevents;
		}

		uint64_t prev, next, nextsm, dataptr;
		if (readu64(&next, file->file) ||
		    readu64(&prev, file->file) ||
		    readu64(&nextsm, file->file) ||
		    readu64(&dataptr, file->file)) {
			return -1;
		}

		/* Make sure we're not duplicating values
		 * TODO: Use a hashmap or some other O(1) data structure for
		 * this */
		for (int i = 0; i < events->len; ++i) {
			if (events->events[i].id == dataptr) {
				goto next;
			}
		}

		if (fseek(file->file, dataptr, SEEK_SET) == -1) {
			return -1;
		}

		struct event *event = events->events + (events->len++);
		uint64_t functions, firstev, start, end, len;
		if (readu64(&functions, file->file) ||
		    readu64(&firstev, file->file) ||
		    readu64(&start, file->file) ||
		    readu64(&end, file->file) ||
		    readu64(&len, file->file)) {
			return -1;
		}
		event->start = us64(start);
		event->end = us64(end);
		if ((event->name = malloc(len+1)) == NULL) {
			return -1;
		}
		if (fread(event->name, len, 1, file->file) < 1) {
			return -1;
		}
		event->name[len] = '\0';
		event->id = dataptr;

next:
		if (next == 0) {
			break;
		}
		if (fseek(file->file, next, SEEK_SET) == -1) {
			return -1;
		}
	}
	return 0;
}

void freeeventlist(struct eventlist *list) {
	for (int i = 0; i < list->len; ++i) {
		free(list->events[i].name);
	}
	free(list->events);
	free(list);
}

int dateremove(datefile *file, uint64_t id) {
	uint64_t iter = id;
	while (iter != 0) {
		if (eventremove(file, iter, &iter)) {
			return -1;
		}
	}
	return 0;
}

static int eventremove(datefile *file, uint64_t id, uint64_t *nextsmret) {
	uint64_t prev, next;
	if (fseek(file->file, id, SEEK_SET) == -1 ||
	    readu64(&next, file->file) ||
	    readu64(&prev, file->file) ||
	    readu64(nextsmret, file->file)) {
		return -1;
	}

	if (fseek(file->file, prev, SEEK_SET) == -1 ||
	    writeu64(next, file->file)) {
		return -1;
	}
	if (next != 0 &&
		(fseek(file->file, next+8, SEEK_SET) == -1 ||
		 writeu64(prev, file->file))) {
		return -1;
	}
	return 0;
}

#ifdef NREM_TESTS
int datestest(int *passed, int *total) {
	NREM_ASSERT(su64(us64(0)) == 0);
	NREM_ASSERT(su64(us64(10)) == 10);
	NREM_ASSERT(su64(us64((1llu << 63) + 100)) == (1llu << 63) + 100);
	NREM_ASSERT(us64(su64(0)) == 0);
	NREM_ASSERT(us64(su64(10)) == 10);
	NREM_ASSERT(us64(su64(-10)) == -10);
	NREM_ASSERT(us64(su64((1llu << 63) - 1)) == ((1llu << 63) - 1));
	NREM_ASSERT(su64(-5) < su64(5));
	NREM_ASSERT(su64(0) < su64(1));
	NREM_ASSERT(su64(-5) < su64(-1));
	NREM_ASSERT(su64(1) < su64(5));
	return 0;
}
#else
int datestest(int *passed, int *total) {
	++*total;
	return 1;
}
#endif
