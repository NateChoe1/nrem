#ifndef HAVE_INTERFACES
#define HAVE_INTERFACES

#include <time.h>

int nremcli(int argc, char **argv);
int nremtui(int argc, char **argv);
extern time_t now; /* The current time, initialized on startup to avoid race
                      conditions */

#endif
