#!/bin/bash

RC=0
for src in `ls *.simple`
do
	echo -n "Compiling $src...  "
	assembly=${src%.simple}.s
	if ! `./tutorial $src > $assembly`; then
		echo "Compile error"
		RC=1
	else
		`gcc libruntime.o $assembly -o ${src%.simple}`
		echo "ok"
		echo "Running $src..."
		echo 'A' | ./${src%.simple} || RC=1
		rm -f "${src%.simple}"
	fi
	rm -f "${assembly}"
done

exit $RC
