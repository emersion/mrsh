#!/bin/sh

echo "basic while loop"
n=asdf
echo start
while [ "$n" != "fdsa" ]; do
	echo "n: $n"
	n="fdsa"
	echo "n: $n"
done
echo stop

echo "continue in while loop should skip the iteration"
n=asdf
echo start
while [ "$n" != fdsa ]; do
	n=fdsa
	continue
	echo "this shouldn't be printed"
done
echo stop

echo "break in while loop should stop the loop"
n=asdf
echo start
while true; do
	if [ "$n" = fdsa ]; then
		break
	fi
	n=fdsa
done
echo stop

# Should exit immediately
while true
do
	exit
	# https://github.com/emersion/mrsh/issues/37
	echo bad
done
