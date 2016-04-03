#!/bin/sh

(
while true
do
	echo 0123456789012345678901234567890123456789
	sleep 1
done
) | ./decipede -C -x con 
