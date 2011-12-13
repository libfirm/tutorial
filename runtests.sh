#!/bin/bash

RC=0
for src in `ls *.simple`
do
	echo -n "Compiling $src...  "
	assembly=${src%.simple}.s
	if ! `./tutorial $src`; then
		echo "Compile error"
		RC=1
	else
		`gcc -m32 libsimple.o $assembly -o ${src%.simple}`
		echo "ok"
		echo "Running $src..."
		./${src%.simple} < /dev/null || RC=1
		rm -f "${src%.simple}"
	fi
	rm -f "${assembly}"
done

exit $RC
