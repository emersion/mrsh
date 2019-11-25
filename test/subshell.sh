#!/bin/sh

echo "Simple subshell"
(echo hi)

echo "Subshell assignment"
a=a
(a=b)
echo $a

echo "Subshell status"
(exit 1) && echo "shouldn't happen"
true # to clear the last status

echo "Multi-line subshell"
(
	echo a
	echo b
)
