#!/bin/bash

for src in `ls *.kal`
do
	`./frontend $src`
	assembly=${src%.kal}.s
	`gcc libkaleidoscope.o $assembly -o ${src%.kal}`
done

exit 0
