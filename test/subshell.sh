#!/bin/sh

echo "Subshell assignment"
a=a
(a=b)
echo $a

echo "Subshell status"
(exit 1) && echo "shouldn't happen"
true # to clear the last status
