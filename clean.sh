#!/bin/bash

for src in `ls *.kal`
do
	`rm -f ${src%.kal} ${src%.kal}.s`
done

exit 0
