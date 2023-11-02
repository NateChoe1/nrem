CFLAGS = -g -DNREM_TESTS
OUT = build/nrem
INSTALLDIR = /usr/bin

LIBS = ncurses
_CFLAGS = $(CFLAGS) -Isrc/include $(shell pkg-config --cflags $(LIBS))
__CFLAGS = $(_CFLAGS) -Wall -Wpedantic -Wshadow -Wconversion -Wimplicit-fallthrough=4
LDFLAGS =
_LDFLAGS = $(LDFLAGS) $(shell pkg-config --libs $(LIBS))
__LDFLAGS = $(_LDFLAGS)
HEADERS = $(wildcard src/include/*.h)
CSRC = $(wildcard src/*.c)
OBJS = $(subst .c,.o,$(subst src,work,$(CSRC)))

$(OUT): $(OBJS)
	$(CC) $(__LDFLAGS) $(OBJS) -o $@

work/%.o: src/%.c $(HEADERS)
	$(CC) $(__CFLAGS) -c $< -o $@
work/dates.o: src/dates.c src/filestruct.h $(HEADERS)
	$(CC) $(__CFLAGS) -c $< -o $@

clean: $(wildcard work/*) $(OUT)
	rm $<

install: $(OUT)

.PHONY: clean install
