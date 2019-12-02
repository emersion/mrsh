#!/bin/sh

echo "to stdout"
uname

echo >&2 "stdout to stderr"
uname >&2

echo 2>&1 "stderr to stdout"
uname 2>&1
#(echo >&2 asdf) 2>&1
