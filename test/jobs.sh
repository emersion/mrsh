#!/bin/sh
set -me

sleep 10&
echo "$(jobs)"
echo "$(jobs -p %1)"
[ -z "$(jobs %1)" ]
jobs %1 | grep "Running"
kill -STOP $(jobs -p %1)
jobs %1 | grep "SIGSTOP"
kill $(jobs -p %1)
[ -z "$(jobs %1)" ]

sleep 10&
kill -STOP $(jobs -p %1)
kill -CONT $(jobs -p %1)
jobs %1 | grep "Running"
kill $(jobs -p %1)
[ -z "$(jobs %1)" ]
