#!/bin/sh

echo "Simple for loop"
for i in 1 2 3; do
	echo $i
done

echo "Word expansion in for loop"
two=2
for i in 1 $two $(echo 3); do
	echo $i
done
echo $i

echo "No-op for loop"
for i in; do
	echo invalid
done
echo $i

echo "Field splitting in for loop, expanded from parameter"
asdf='a s d f'
for c in $asdf; do
	echo $c
done

echo "Field splitting in for loop, expanded from command substitution"
for c in $(echo a s d f); do
	echo $c
done

echo "Field splitting in for loop, with IFS set"
(
	IFS=':'
	asdf='a:s:d:f'
	for c in $asdf; do
		echo $c
	done
)
