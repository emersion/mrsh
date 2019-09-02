#!/bin/sh -e
func_a() {
	echo func a
	return
	echo func a post-return
}

func_b() {
	echo func b
	return 1
	echo func b post-return
}

func_c() {
	echo func c
	while :
	do
		echo func c loop
		return
		echo func c loop post-return
	done
	echo func c post loop
}

func_a

if func_b
then
	exit 1
fi

func_c
