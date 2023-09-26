#ifndef HAVE_TESTS
#define HAVE_TESTS

#ifdef NREM_TESTS

#include <stdio.h>

#define NREM_ASSERT(condition) \
	++*total; \
	if (condition) { \
		++*passed; \
	} \
	else { \
		fprintf(stderr, "%s: ASSERT FAILED ON LINE %d: %s\n", \
				__FILE__, __LINE__, #condition); \
	}

#endif

int runtests(int *passed, int *total);

#endif
