#!/bin/sh

set -e

mrsh_limits=`ulimit`
[ $mrsh_limits == "unlimited" ]
if [ -e /proc/self/limits ]
then
	grep "Max file size" /proc/self/limits | grep "unlimited"
fi

ulimit -f 100

mrsh_limits=`ulimit`
[ $mrsh_limits -eq 100 ]
if [ -e /proc/self/limits ]
then
	grep "Max file size" /proc/self/limits | grep 51200
fi
