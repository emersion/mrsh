#!/bin/sh

echo >&2 "Pipeline with 1 command"
echo "a b c d"

echo >&2 "Pipeline with 2 commands"
echo "a b c d" | sed s/b/B/

echo >&2 "Pipeline with 3 commands"
echo "a b c d" | sed s/b/B/ | sed s/c/C/

echo >&2 "Pipeline with subshell"
(echo "a b"; echo "c d") | sed s/c/C/

#echo >&2 "Pipeline with brace group"
#{ echo "a b"; echo "c d"; } | sed s/c/C/

#echo >&2 "Pipeline with early close"
#(
#	i=0
#	while [ $i -lt 8096 ]
#	do
#		echo "Line $i"
#		i=$((i+1))
#	done
#) | head -n 1
