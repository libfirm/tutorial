#!/bin/bash

for src in `ls *.simple`
do
	echo -n "Compiling $src...  "
	assembly=${src%.simple}.s
	if ! `./tutorial $src`; then
		echo "Compile error"
	else
		`gcc libsimple.o $assembly -o ${src%.simple}`
		echo "ok"
		echo "Running $src..."
		./${src%.simple} < /dev/null
		rm -f "${src%.simple}"
	fi
	rm -f "${assembly}"
done

exit 0
