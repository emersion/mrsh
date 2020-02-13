#!/bin/sh

echo "Pipeline with 1 command"
echo "a b c d"

echo "Pipeline with 2 commands"
echo "a b c d" | sed s/b/B/

echo "Pipeline with 3 commands"
echo "a b c d" | sed s/b/B/ | sed s/c/C/

echo "Pipeline with bang"
! false
echo $?

echo "Pipeline with unknown command"
idontexist
echo $? # 127

# https://github.com/emersion/mrsh/issues/100
echo "Pipeline with subshell"
(echo "a b"; echo "c d") | sed s/c/C/

# https://github.com/emersion/mrsh/issues/96
echo "Pipeline with brace group"
{ echo "a b"; echo "c d"; } | sed s/c/C/

# https://github.com/emersion/mrsh/issues/95
#echo "Pipeline with early close"
#(
#	i=0
#	while [ $i -lt 8096 ]
#	do
#		echo "Line $i"
#		i=$((i+1))
#	done
#) | head -n 1
