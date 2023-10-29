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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <tests.h>
#include <interfaces.h>

datefile f;
time_t now;
struct tm nowb;

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [command] [args]\n", argv[0]);
		return 1;
	}

	now = time(NULL);
	memcpy(&nowb, localtime(&now), sizeof nowb);

	char path[256];
	char *env;
	if ((env = getenv("DATEFILE")) != NULL) {
		strncpy(path, env, sizeof path - 1);
		path[sizeof path - 1] = '\0';
	}
	else if ((env = getenv("HOME")) != NULL) {
		snprintf(path, sizeof path, "%s/.config/nrem/datefile", env);
	}
	else {
		fputs("Failed to get datefile path, set $DATEFILE\n", stderr);
		return 1;
	}
	if (dateopen(path, &f)) {
		fprintf(stderr, "Failed to open datefile %s\n", path);
		return 1;
	}

	if (strcmp(argv[1], "cli") == 0) {
		return nremcli(argc-1, argv+1);
	}
	if (strcmp(argv[1], "tui") == 0) {
		return nremtui(argc-1, argv+1);
	}
	if (strcmp(argv[1], "test") == 0) {
		int passed, total, status;
		passed = total = 0;
		status = runtests(&passed, &total);
		if (passed < total) {
			fputs("NOT ALL TESTS PASSED!\n", stderr);
		}
		fprintf(stderr, "%d/%d tests passed\n", passed, total);
		return status;
	}

	fprintf(stderr, "Invalid command %s\n", argv[1]);
	return 1;
}
