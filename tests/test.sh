#!/usr/bin/env bash

export passed=0
export total=0

# We can't use *.in because if there are no inputs the program breaks
for infile in $(ls | grep '.*\.in$') ; do
	export DATEFILE=./test.date
	export outfile=$(echo "$infile" | sed "s/\.in$/.out/")
	if sh "$infile" ; then
		passed=$(expr $passed + 1)
	else
		echo "TEST $infile FAILED!" > /dev/stderr
	fi
	rm ./test.date > /dev/null 2>&1
	total=$(expr $total + 1)
done
echo "$passed/$total"
