#ifndef HAVE_TESTS
#define HAVE_TESTS

#ifdef NREM_TESTS

#define NREM_ASSERT(condition) \
	++*total; \
	if (condition) { \
		++*passed; \
	} \
	else { \
		fprintf(stderr, "%s: TEST FAILED ON LINE %d\n", \
				__FILE__, __LINE__); \
	}

#endif

int runtests(int *passed, int *total);

#endif
