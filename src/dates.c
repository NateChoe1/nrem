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
 * Event representation
 *     struct {
 *         uint64_t next;               The next event that occurs at this same
 *                                      time stamp
 *         uint64_t prev;               The location of the pointer to this
 *                                      event
 *         uint64_t functions;          Unused for now, MUST be 0
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

/* Unsigned -> signed 64 bit int conversion. Note that this implementation does
 * create two zeros*/
static inline int64_t us64(uint64_t v) {
	if (v >= 1llu << 63) {
		uint64_t normalized = v & ~(1llu << 63);
		uint64_t abs = (1llu << 63) - normalized;
		return -(int64_t) abs;
	}
	else {
		return v;
	}
}

/* Opposite of us64 */
static inline uint64_t su64(int64_t v) {
	return (uint64_t) v;
}

/* Creates a datefile. This function will truncate `path` */
static int datecreate(char *path, datefile *ret);

/* Add a date with a certain prefix */
static int dateaddbit(struct event *event, datefile *file,
		uint64_t prefix, int precision);

static int datesearchrecursive(datefile *file, struct eventlist *events,
		uint64_t start, uint64_t end,
		uint64_t prefix, uint8_t precision, uint8_t bitn,
		uint64_t ptr);
static int readtime(datefile *file, struct eventlist *events,
		uint64_t time, uint64_t ptr);

static inline uint64_t fill1(int n) {
	uint64_t ret = 0;
	for (int i = 0; i < n; ++i) {
		ret <<= 1;
		ret |= 1;
	}
	return ret;
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
	if (writeu64(0, file) ||
	    writeu64(0, file)) {
		return -1;
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

static int dateaddbit(struct event *event, datefile *file,
		uint64_t prefix, int precision) {
	return 0;
}

int dateadd(struct event *event, datefile *file) {
	if (file->bitn > 64) {
		return -1;
	}

	if (fseek(file->file, file->bit1, SEEK_SET) == -1) {
		return -1;
	}

//	/* Go through each bit of the timestamp */
//	for (uint64_t mask = 1llu << (file->bitn-1); mask > 0; mask >>= 1) {
//		int bit = !!(su64(event->time) & mask);
//		uint64_t next;
//		long field_pos;
//
//		/* Find the field */
//		if (fseek(file->file, bit ? 8:0, SEEK_CUR) == -1) {
//			return -1;
//		}
//
//		/* Store the field position */
//		if ((field_pos = ftell(file->file)) == -1) {
//			return -1;
//		}
//
//		/* Read the field */
//		if (readu64(&next, file->file)) {
//			return -1;
//		}
//
//		/* If the field is empty (the node doesn't exist yet), create
//		 * it */
//		if (next == 0) {
//			long new_pos;
//
//			/* Go to the end of the file (where new nodes are
//			 * added) */
//			if (fseek(file->file, 0, SEEK_END) == -1) {
//				return -1;
//			}
//
//			/* Remember the location of the new node*/
//			if ((new_pos = ftell(file->file)) == -1) {
//				return -1;
//			}
//
//			/* Initialize the new node with zeroes */
//			for (int i = 0; i < 32; ++i) {
//				if (fputc(0, file->file) == EOF) {
//					return -1;
//				}
//			}
//
//			/* Update the parent node's child pointer */
//			if (fseek(file->file, field_pos, SEEK_SET) == -1) {
//				return -1;
//			}
//			if (writeu64((uint64_t) new_pos, file->file)) {
//				return -1;
//			}
//			
//			/* Return to the new node position */
//			if (fseek(file->file, new_pos, SEEK_SET) == -1) {
//				return -1;
//			}
//		}
//		/* If the child node does exist, just go to it */
//		else {
//			if (fseek(file->file, next, SEEK_SET) == -1) {
//				return -1;
//			}
//		}
//	}
//
//	/* We are at the dest node */
//
//	long leafpos, newhead;
//	uint64_t oldhead;
//	size_t eventlen = strlen(event->name);
//
//	/* Mark the location of the leaf node data */
//	if ((leafpos = ftell(file->file)) == -1) {
//		return -1;
//	}
//
//	/* Get the old head location */
//	if (readu64(&oldhead, file->file)) {
//		return -1;
//	}
//
//	/* Seek to the end and get the new head */
//	if (fseek(file->file, 0, SEEK_END) == -1 ||
//	    (newhead = ftell(file->file)) == -1) {
//		return -1;
//	}
//
//	/* Write event data */
//	if (writeu64(oldhead, file->file) ||  /* next (prepend to linkedlist) */
//	    writeu64(leafpos, file->file) ||  /* prev (leaf node) */
//	    writeu64(0, file->file) ||        /* functions (0) */
//	    writeu64(eventlen, file->file) || /* event len */
//	    fwrite(event->name, eventlen, 1, file->file) < 1 || /* event name */
//	    fflush(file->file) == EOF         /* flush file */ ) {
//		return -1;
//	}
//
//	/* Update the old head's prev value */
//	if (oldhead != 0 && 
//		(fseek(file->file, oldhead+8, SEEK_SET) == -1 ||
//	         writeu64(newhead, file->file))) {
//		return -1;
//	}
//
//	/* Update head pointer  */
//	if (fseek(file->file, leafpos, SEEK_SET) == -1 ||
//	    writeu64(newhead, file->file)) {
//		return -1;
//	}
//
//	event->id = newhead;

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
			0, 0, file->bitn, file->bit1);
	if (status) {
		freeeventlist(ret);
		return NULL;
	}

	return ret;
}

static int datesearchrecursive(datefile *file, struct eventlist *events,
		uint64_t start, uint64_t end,
		uint64_t prefix, uint8_t precision, uint8_t bitn,
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
	late = prefix | fill1(bitn-precision);
	if (early > end || late < start) {
		return 0;
	}

	/* Handle leaf nodes */
	if (precision >= bitn) {
		return readtime(file, events, prefix, ptr);
	}

	/* Else, read the inner node */
	if (fseek(file->file, ptr, SEEK_SET) == -1) {
		return -1;
	}
	uint64_t child0, child1;
	if (readu64(&child0, file->file) ||
	    readu64(&child1, file->file)) {
		return -1;
	}

	/* Recurse with one more level of precision */
	if (datesearchrecursive(file, events, start, end,
				prefix,
				precision+1, bitn, child0) ||
	    datesearchrecursive(file, events, start, end,
				prefix | (1lu << (bitn-precision-1)),
				precision+1, bitn, child1)) {
		return -1;
	}

	return 0;
}

static int readtime(datefile *file, struct eventlist *events,
		uint64_t time, uint64_t ptr) {
//	if (fseek(file->file, ptr, SEEK_SET) == -1) {
//		return -1;
//	}
//
//	uint64_t iter;
//	if (readu64(&iter, file->file)) {
//		return -1;
//	}
//	while (iter != 0) {
//		if (fseek(file->file, iter, SEEK_SET) == -1) {
//			return -1;
//		}
//
//		/* Reallocate events if OOM */
//		if (events->len >= events->alloc) {
//			struct event *newevents;
//			size_t newalloc;
//			newalloc = events->alloc * 2;
//			newevents = realloc(events->events,
//					newalloc * sizeof *newevents);
//			if (newevents == NULL) {
//				return -1;
//			}
//			events->alloc = newalloc;
//			events->events = newevents;
//		}
//
//		uint64_t prev, next, functions, len;
//		if (readu64(&next, file->file) ||
//		    readu64(&prev, file->file) ||
//		    readu64(&functions, file->file) ||
//		    readu64(&len, file->file)) {
//			return -1;
//		}
//
//		struct event *event = events->events + (events->len++);
//		event->time = us64(time);
//		if ((event->name = malloc(len+1)) == NULL) {
//			return -1;
//		}
//		if (fread(event->name, len, 1, file->file) < 1) {
//			return -1;
//		}
//		event->name[len] = '\0';
//		event->id = iter;
//
//		iter = next;
//	}
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
	uint64_t prev, next;
	if (fseek(file->file, id, SEEK_SET) == -1 ||
	    readu64(&next, file->file) ||
	    readu64(&prev, file->file)) {
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
	return 0;
}
#else
int datestest(int *passed, int *total) {
	++*total;
	return 1;
}
#endif
