#!/bin/sh

echo >&2 "Returns immediately"
(wait)

echo >&2 "Run asynchronous list and wait"
echo a &
wait $!

echo >&2 "Run two asynchronous lists in parallel and wait"
echo a &
p1=$!
echo b &
wait $p1
echo Job 1 exited with status $?
wait $!
echo Job 2 exited with status $?

#echo >&2 "Run asynchronous list, kill it and wait"
#sleep 1000 &
#pid=$!
#kill -kill $pid
#wait $pid
#echo $pid was terminated by a SIG$(kill -l $?) signal.
