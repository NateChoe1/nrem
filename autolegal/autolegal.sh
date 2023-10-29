#!/bin/sh

if [ $# -lt 1 ] ; then
	echo "Usage: $0 [source dir]" > /dev/stderr
	exit 1
fi

getver() {
	if [ $# -lt 1 ] ; then
		echo "getver called without arguments" > /dev/stderr
		exit 1
	fi
	cat "$1" | grep "@LEGAL_HEAD" | sed "s/.*@LEGAL_HEAD \(\[.*\]\).*/\1/"
}

find "$1" -type f | while read FILE ; do
	EXTENSION="$(echo "$FILE" | sed "s/.*\(\..*$\)/\1/")"
	LF="$(dirname "$0")/license$EXTENSION"
	if [ "$FILE" = "src/main.c" ] ; then
	if [ "$(getver "$LF")" != "$(getver "$FILE")" ] ; then
		TMP="$(cat "$LF" <(cat "$FILE" | sed '/@LEGAL_HEAD/,/@LEGAL_TAIL/d'))"
		echo "$TMP" > "$FILE"
	fi
	fi
done
