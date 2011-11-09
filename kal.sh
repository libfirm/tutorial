#!/bin/bash

for src in `ls *.kal`
do
	`./tutorial $src`
	assembly=${src%.kal}.s
	`gcc libkaleidoscope.o $assembly -o ${src%.kal}`
done

exit 0
