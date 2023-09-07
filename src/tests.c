#include <tests.h>
#include <dates.h>

#ifdef NREM_TESTS

int runtests(int *passed, int *total) {
	int ret;

	ret = 0;

	if (datestest(passed, total)) {
		ret = 1;
	}

	return ret;
}

#else

int runtests(int *passed, int *total) {
	*passed = 0;
	*total = 1;
	return 1;
}

#endif
