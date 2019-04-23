#!/bin/sh

lla () {
	ls -la
}

alias ll="ls -l"

command -v if
echo "exists if $?"
command -v cd
echo "exits cd $?"
command -v ls
echo "exists ls $?"
command -v ll
echo "exists ll $?"
command -v lla
echo "exists lla $?"
command -v idontexists
if [ $? -ne 0 ]
then
	echo "ok"
else
	echo "ko"
fi
