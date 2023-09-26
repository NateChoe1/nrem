CFLAGS = -O2 -g -DNREM_TESTS
OUT = build/nrem
INSTALLDIR = /usr/bin

LIBS = ncurses
_CFLAGS = $(CFLAGS) -Isrc/include $(shell pkg-config --cflags $(LIBS))
__CFLAGS = $(_CFLAGS) -Wall -Wpedantic -Wshadow
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

clean: $(wildcard work/*) $(OUT)
	rm $<

install: $(OUT)

.PHONY: clean install
