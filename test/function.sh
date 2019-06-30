#!/bin/sh -e
func_a() {
	echo func_a
}

func_b() {
	echo func_b
}

func_b() {
	echo func_b revised
}

func_c() {
	echo func_c

	func_c() {
		echo func_c revised
	}
}

func_d() if true; then echo func_d; fi

func_e() {
	echo $1
}

func_a
func_b
func_a
func_c
func_c
func_d
func_e hello

output=$(func_a)
echo "output is $output"
