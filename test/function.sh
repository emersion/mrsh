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

func_a
func_b
func_c
func_c

output=$(func_a)

[ "$output" = "func_a" ]
