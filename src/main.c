#include <stdio.h>
#include <string.h>

#include <interfaces.h>

time_t now;

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [command] [args]\n", argv[0]);
		return 1;
	}

	now = time(NULL);

	if (strcmp(argv[1], "cli") == 0) {
		return nremcli(argc-1, argv+1);
	}
	if (strcmp(argv[1], "tui") == 0) {
		return nremtui(argc-1, argv+1);
	}

	fprintf(stderr, "Invalid command %s\n", argv[1]);
	return 1;
}
