#include <ctype.h>
#include <string.h>

#include <curses.h>

#include <tui.h>
#include <util.h>
#include <interfaces.h>

static int sday, smon, syear, shr, smin, ssec;
static int eday, emon, eyear, ehr, emin, esec;
static char name[256];
static int namelen;
static int namecur;

static enum {
	NAME,
	SYEAR, SMON, SDAY, SHR, SMIN, SSEC,
	EYEAR, EMON, EDAY, EHR, EMIN, ESEC, COPY,
	CANCEL, GO,
	OVERFLOW_BOX
} currbox;

enum caltype {
	YEAR,
	MONTH,
	DAY,
	HOUR,
	MINUTE,
	SECOND,
};

static int const BASE_LEFT = 16;

static inline void add_prompt(WINDOW *win, char *prompt, int height_offset,
		int h, int w) {
	mvwaddstr(win, h/2 + height_offset,
			w/2-BASE_LEFT - (int) strlen(prompt),
			prompt);
}

int tui_newevent_reset(WINDOW *win) {
	namelen = namecur = 0;
	sday = eday = tui_day;
	smon = emon = tui_mon;
	syear = eyear = tui_year;
	shr = smin = ssec = ehr = emin = esec = 0;
	currbox = NAME;
	return 0;
}

static void tui_showdate(WINDOW *win,
		int year, int mon, int day, int hr, int min, int sec,
		enum caltype selected, int *yret, int *xret) {
	char buff[15];
#define PART(type, template, ...) \
	if (selected == type) { \
		getyx(win, *yret, *xret); \
	} \
	snprintf(buff, sizeof buff, template, __VA_ARGS__); \
	buff[sizeof buff - 1] = '\0'; \
	waddstr(win, buff);
	PART(YEAR, "%04d", year);
	waddch(win, '-');
	PART(MONTH, "%02d", mon + 1);
	waddch(win, '-');
	PART(DAY, "%02d", day + 1);
	waddch(win, ' ');
	PART(HOUR, "%02d", hr);
	waddch(win, ':');
	PART(MINUTE, "%02d", min);
	waddch(win, ':');
	PART(SECOND, "%02d", sec);
#undef PART
}

int tui_newevent(enum tui_state *state, WINDOW *win) {
	int w, h, cx, cy;
	getmaxyx(win, h, w);
	cx = cy = 0;

	wclear(win);

	add_prompt(win, "Name: ", -2, h, w);
	waddnstr(win, name, namecur);
	if (currbox == NAME) {
		getyx(win, cy, cx);
	}
	waddnstr(win, name+namecur, namelen-namecur);
	for (int i = 0; i < BASE_LEFT*2 - namelen; ++i) {
		waddch(win, '_');
	}

	add_prompt(win, "Start: ", -1, h, w);
	tui_showdate(win, syear, smon, sday, shr, smin, ssec,
			currbox - SYEAR, &cy, &cx);
	add_prompt(win, "End: ", 0, h, w);
	tui_showdate(win, eyear, emon, eday, ehr, emin, esec,
			currbox - EYEAR, &cy, &cx);
	waddch(win, ' ');
	if (currbox == COPY) {
		getyx(win, cy, cx);
	}
	waddstr(win, "[Copy start]");

	wmove(win, h/2+1, w/2 - BASE_LEFT/2);
	if (currbox == CANCEL) {
		getyx(win, cy, cx);
	}
	waddstr(win, "CANCEL");

	wmove(win, h/2+1, w/2 + BASE_LEFT/2 - (int) strlen("GO"));
	if (currbox == GO) {
		getyx(win, cy, cx);
	}
	waddstr(win, "GO");

	wmove(win, cy, cx);

	wrefresh(win);

	int c = wgetch(win);

	switch (c) {
	case '\n':
		if (currbox == COPY || currbox == CANCEL || currbox == GO) {
			break;
		}
		/* fallthrough */
	case '\t': case KEY_DOWN: case KEY_ENTER: case KEY_NPAGE:
		goto next;
	case KEY_BTAB: case KEY_UP: case KEY_PPAGE: case KEY_SR:
		goto prev;
	default:
		break;
	}

	switch (currbox) {
	case NAME:
		switch (c) {
		case KEY_BACKSPACE:
			memmove(name+namecur-1, name+namecur,
					(size_t) (namelen-namecur));
			--namelen;
			--namecur;
			break;
		case KEY_LEFT:
			--namecur;
			break;
		case KEY_RIGHT:
			++namecur;
			break;
		case KEY_HOME:
			namecur = 0;
			break;
		case KEY_END:
			namecur = namelen;
			break;
		case KEY_DC:
			memmove(name+namecur, name+namecur+1,
					(size_t) (namelen-namecur-1));
			--namelen;
			break;
		case 'u' & 0x1f:
			memmove(name, name + namecur, (size_t) namecur);
			namelen -= namecur;
			namecur = 0;
			break;
		default:
			if (!isprint(c)) {
				break;
			}
			if (namelen < sizeof name - 1) {
				memmove(name+namecur+1, name+namecur,
						(size_t) (namelen-namecur));
				name[namecur++] = (char) c;
				++namelen;
			}
			break;
		}
		break;

	int *currnum;
#define DATEVAR(enumval, var) \
	case enumval: \
		currnum = &var; \
		goto changenum;
	DATEVAR(SYEAR, syear);
	DATEVAR(SMON, smon);
	DATEVAR(SDAY, sday);
	DATEVAR(SHR, shr);
	DATEVAR(SMIN, smin);
	DATEVAR(SSEC, ssec);

	DATEVAR(EYEAR, eyear);
	DATEVAR(EMON, emon);
	DATEVAR(EDAY, eday);
	DATEVAR(EHR, ehr);
	DATEVAR(EMIN, emin);
	DATEVAR(ESEC, esec);
#undef DATEVAR

	int oldval;
	changenum:
		oldval = *currnum;
		switch (c) {
		case 'j': case KEY_DOWN:
			--*currnum;
			break;
		case 'k': case KEY_UP:
			++*currnum;
			break;
		}
		if (isinvalid(syear, smon, sday, shr, smin, ssec) ||
				isinvalid(eyear, emon, eday, ehr, emin, esec)) {
			*currnum = oldval;
			break;
		}
		break;
	case COPY:
		if (c == '\n' || c == ' ') {
			eyear = syear;
			emon = smon;
			eday = sday;
			ehr = shr;
			emin = smin;
			esec = ssec;
		}
		break;
	case CANCEL:
		if (c == '\n' || c == ' ') {
			*state = VIEWCAL;
		}
		break;
	case GO:;
		struct event newevent;
		newevent.start = convtime(syear, smon, sday, shr, smin, ssec);
		newevent.end = convtime(eyear, emon, eday, ehr, emin, esec);
		name[namelen] = '\0';
		newevent.name = name;
		dateadd(&newevent, &f);

		*state = VIEWCAL;

		return 0;
	case OVERFLOW_BOX:
		return 1;
	}
end:
	return 0;
	/* Oh no! Backwards goto! */
next:
	currbox = (currbox+1) % OVERFLOW_BOX;
	goto end;
prev:
	currbox = (currbox-1 + OVERFLOW_BOX) % OVERFLOW_BOX;
	goto end;
}
