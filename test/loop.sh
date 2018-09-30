#!/bin/sh

n=asdf
echo start
while [ "$n" != "fdsa" ]; do
	echo "n: $n"
	n="fdsa"
	echo "n: $n"
done
echo stop

# Should exit immediately
while true
do
	exit
done
