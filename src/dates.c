#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#include <stdint.h>
#include <stdio.h>

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
 *         char name[len];              The event name itself
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

#define STRUCTS \
	X(df_header, \
		Y(PADDING, magic, 8) \
		Y(U64, bit1, ~) \
		Y(U8, bitn, ~) \
		Y(PADDING, reserved, 16) \
	) \
	X(df_node, \
		Y(U64, child0, ~) \
		Y(U64, child1, ~) \
		Y(U64, event, ~) \
		Y(PADDING, reserved, 16) \
	) \
	X(df_event, \
		Y(U64, next, ~) \
		Y(U64, prev, ~) \
		Y(U64, nextsm, ~) \
		Y(U64, ptr, ~) \
		Y(PADDING, reserved, 16) \
	) \
	X(df_event_data, \
		Y(U64, functions, ~) \
		Y(U64, firstev, ~) \
		Y(I64, start, ~) \
		Y(I64, end, ~) \
		Y(STR, name, ~) \
	) \

#include "filestruct.h"
#undef STRUCTS

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
		return UINT64_MAX;
	}
	return ((uint64_t) 1ull << n)-1;
}

int dateopen(char *path, datefile *ret) {
	FILE *file = fopen(path, "rb+");
	struct df_header header;
	if (file == NULL) {
		return datecreate(path, ret);
	}

	if (read_df_header(&header, file)) {
		return -1;
	}

	if (memcmp(header.magic, "datefile", sizeof header.magic)) {
		return -1;
	}

	ret->file = file;
	ret->bit1 = header.bit1;
	ret->bitn = header.bitn;

	return 0;
}

static int datecreate(char *path, datefile *ret) {
	FILE *file = fopen(path, "w+");
	struct df_header header;
	struct df_node bit1;

	if (file == NULL) {
		return -1;
	}

	memcpy(header.magic, "datefile", sizeof header.magic);
	header.bit1 = 0; /* to be overwritten later */
	header.bitn = 64;
	memset(header.reserved, 0, sizeof header.reserved);

	if (write_df_header(&header, file) == -1) {
		return -1;
	}

	bit1.child0 = bit1.child1 = bit1.event = 0;
	memset(bit1.reserved, 0, sizeof bit1.reserved);
	if (write_df_node(&bit1, file) == -1) {
		return -1;
	}

	if (seek(file, header.bit1_pos, SEEK_SET) == -1 ||
	    writeu64(bit1.offset, file) == -1) {
		return -1;
	}

	ret->file = file;
	ret->bit1 = bit1.offset;
	ret->bitn = header.bitn;

	return 0;
}

static int dateaddbit(datefile *file, uint64_t prefix, int precision,
		uint64_t dataptr, uint64_t nextsmptr, uint64_t *newnextsmptr) {
	if (seek(file->file, file->bit1, SEEK_SET) == -1) {
		return -1;
	}

	/* For each bit */
	for (int i = 0; i < precision; ++i) {
		uint64_t mask = 1llu << (file->bitn-1-i);
		int bit = !!(prefix & mask);
		uint64_t next;
		struct df_node node;

		/* Read the node */
		if (read_df_node(&node, file->file)) {
			return -1;
		}
		next = bit ? node.child1 : node.child0;

		/* If the next node down doesn't exist yet, create it */
		if (next == 0) {
			uint64_t new_pos;
			struct df_node new_node;

			/* Go to the end of the file (where new nodes are
			 * added) */
			if (seek(file->file, 0, SEEK_END) == -1) {
				return -1;
			}

			/* Remember the location of the new node*/
			if (tell(file->file, &new_pos) == -1) {
				return -1;
			}

			/* Write the new node */
			new_node.child0 = 0;
			new_node.child1 = 0;
			new_node.event = 0;
			memset(new_node.reserved, 0, sizeof new_node.reserved);
			if (write_df_node(&new_node, file->file)) {
				return -1;
			}

			/* Update the old node */
			if (bit) {
				node.child1 = new_pos;
			}
			else {
				node.child0 = new_pos;
			}
			if (seek(file->file, node.offset, SEEK_SET) == -1) {
				return -1;
			}
			if (write_df_node(&node, file->file)) {
				return -1;
			}

			/* Get back to the new (child) node */
			if (seek(file->file, new_pos, SEEK_SET) == -1) {
				return -1;
			}
		}
		/* If the child node does exist, just go to it */
		else {
			if (seek(file->file, next, SEEK_SET) == -1) {
				return -1;
			}
		}
	}

	/* We are at the dest node */

	struct df_node node;
	struct df_event event;

	/* Read the node */
	if (read_df_node(&node, file->file)) {
		return -1;
	}

	/* Create a new event struct */
	event.next = node.event;
	event.prev = node.event_pos;
	event.nextsm = nextsmptr;
	event.ptr = dataptr;
	memset(event.reserved, 0, sizeof event.reserved);

	/* Write event data */
	if (seek(file->file, 0, SEEK_END) == -1 || /* Get to the EOF */
	    write_df_event(&event, file->file) == -1) { /* Write event */
		return -1;
	}
	/* Get new nextsm */
	*newnextsmptr = event.offset;

	/* Update the old timestamp head's prev value */
	if (node.event != 0 &&
	    ((seek(file->file, node.event + 8, SEEK_SET) == -1 ||
	     writeu64(event.offset, file->file)))) {
		return -1;
	}

	/* Update timestamp head pointer */
	if (seek(file->file, node.event_pos, SEEK_SET) == -1 ||
	    writeu64(*newnextsmptr, file->file)) {
		return -1;
	}

	return 0;

}

int dateadd(struct event *event, datefile *file) {
	if (file->bitn > 64) {
		return -1;
	}

	if (seek(file->file, 0, SEEK_END) == -1) {
		return -1;
	}

	uint64_t id;
	if (tell(file->file, &id) == -1) {
		return -1;
	}

	size_t eventlen = strlen(event->name);
	struct df_event_data data;
	data.functions = 0;
	data.firstev = 0;
	data.start = event->start;
	data.end = event->end;
	data.name_len = eventlen;
	data.name = event->name;

	/* Write event data */
	if (write_df_event_data(&data, file->file) == -1) {
		return -1;
	}
	event->id = id;

	uint64_t nextsmptr = 0;

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
	 * to find the maximum precision of a prefix, or even O(b) by memoizing
	 * the precision of the previous prefix, but that would make the code
	 * far more complicated.
	 * */
	while (lower <= end) {
		int precision;
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
					id, nextsmptr, &nextsmptr)) {
			return -1;
		}
		/* Update lower bound */
		++lower;
	}

	/* Set event data head */
	if (seek(file->file, data.firstev_pos, SEEK_SET) == -1 ||
	    writeu64(nextsmptr, file->file) == -1) {
		return -1;
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
	if (seek(file->file, ptr, SEEK_SET) == -1) {
		return -1;
	}
	struct df_node node;
	if (read_df_node(&node, file->file) == -1) {
		return -1;
	}

	/* If there is an event, read it */
	if (node.event != 0) {
		readtime(file, events, node.event);
	}

	/* Recurse with one more level of precision */
	if (datesearchrecursive(file, events, start, end,
				prefix,
				precision+1, node.child0) ||
	    datesearchrecursive(file, events, start, end,
				prefix | (1lu << (file->bitn-precision-1)),
				precision+1, node.child1)) {
		return -1;
	}

	return 0;
}

/* Reads an event struct */
static int readtime(datefile *file, struct eventlist *events, uint64_t ptr) {
	if (seek(file->file, ptr, SEEK_SET) == -1) {
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

		/* uint64_t prev, next, nextsm, dataptr; */
		struct df_event rawevent;
		if (read_df_event(&rawevent, file->file) == -1) {
			return -1;
		}

		/* Make sure we're not duplicating values
		 * TODO: Use a hashmap or some other O(1) data structure for
		 * this */
		for (int i = 0; i < events->len; ++i) {
			if (events->events[i].id == rawevent.ptr) {
				goto next;
			}
		}

		if (seek(file->file, rawevent.ptr, SEEK_SET) == -1) {
			return -1;
		}

		struct event *event = events->events + (events->len++);
		struct df_event_data data;
		if (read_df_event_data(&data, file->file) == -1) {
			return -1;
		}
		/* uint64_t functions, firstev, start, end, len; */
		event->start = data.start;
		event->end = data.end;
		event->name = data.name;
		event->id = rawevent.ptr;

next:
		if (rawevent.next == 0) {
			break;
		}
		if (seek(file->file, rawevent.next, SEEK_SET) == -1) {
			return -1;
		}
	}
	return 0;
}

void freeeventlist(struct eventlist *list) {
	if (list == NULL) {
		return;
	}
	for (int i = 0; i < list->len; ++i) {
		free(list->events[i].name);
	}
	free(list->events);
	free(list);
}

int dateremove(datefile *file, uint64_t id) {
	uint64_t iter;
	struct df_event_data data;

	/* Read the event data */
	if (read_df_event_data(&data, file->file)) {
		return -1;
	}

	/* Remove pointers to every event that points to this event data */
	iter = data.firstev;
	while (iter != 0) {
		if (eventremove(file, iter, &iter)) {
			return -1;
		}
	}
	return 0;
}

static int eventremove(datefile *file, uint64_t id, uint64_t *nextsmret) {
	struct df_event event;
	/* Read the event */
	if (seek(file->file, id, SEEK_SET) == -1 ||
	    read_df_event(&event, file->file) == -1) {
		return -1;
	}
	*nextsmret = event.nextsm;

	/* Update the previous node's next pointer */
	if (seek(file->file, event.prev, SEEK_SET) == -1 ||
	    writeu64(event.next, file->file) == -1) {
		return -1;
	}
	/* Update the next node's prev pointer*/
	if (event.next != 0 &&
	    (seek(file->file, event.next+8, SEEK_SET) == -1 ||
	    writeu64(event.prev, file->file))) {
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

#pragma GCC diagnostic pop
