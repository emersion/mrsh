#!/bin/sh -eu


printf | read a
echo $?

i=0

printf "a\nb\nc\n" | while read line; do
  printf "%s\n" "${line:-blank}"

  i=$((i+1))

  [ $i -gt 10 ] && break
done

[ $i = 3 ] && echo "correct!"
