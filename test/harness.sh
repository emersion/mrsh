#!/bin/sh
dir=$(dirname "$0")
testcase="$dir/$1"

echo >&2 "Running with mrsh"
mrsh_out=$("$MRSH" "$testcase")
mrsh_ret=$?
echo >&2 "Running with reference shell"
ref_out=$("$REF_SH" "$testcase")
ref_ret=$?
if [ $mrsh_ret -ne $ref_ret ] || [ "$mrsh_out" != "$ref_out" ]
then
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
