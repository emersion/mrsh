#!/bin/sh

dir=$(dirname "$0")

mrsh="$1"
ref="$2"
testcase="$dir/$3"

mrsh_out=$("$mrsh" "$testcase")
mrsh_ret=$?
ref_out=$("$ref" "$testcase")
ref_ret=$?
if [ "$mrsh_ret" != "$ref_ret" ] || [ "$mrsh_out" != "$ref_out" ] ; then
	echo >&2 "$testcase: mismatch"
	echo >&2 ""
	echo >&2 "mrsh: $mrsh_ret"
	echo >&2 "$mrsh_out"
	echo >&2 ""
	echo >&2 "ref: $ref_ret"
	echo >&2 "$ref_out"
	echo >&2 ""
	exit 1
fi
