#!/bin/bash

for src in `ls *.simple`
do
	echo "Compiling and running $src..."
	`./tutorial $src`
	assembly=${src%.simple}.s
	`gcc libsimple.o $assembly -o ${src%.simple}`
	rm -f "${assembler}"
done

exit 0
