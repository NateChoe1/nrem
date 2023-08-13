CFLAGS = -O2 -g
OUT = build/nrem
INSTALLDIR = /usr/bin

_CFLAGS = $(CFLAGS) -Isrc/include
__CFLAGS = $(_CFLAGS) -Wall -Wpedantic
LDFLAGS =
__LDFLAGS = $(LDFLAGS)
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
