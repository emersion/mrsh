#!/bin/sh
dir=$(dirname "$0")
testcase="$1"

echo "Running with mrsh"
mrsh_out=$("$MRSH" "$testcase")
mrsh_ret=$?
echo "Running with reference shell ($REF_SH)"
ref_out=$("$REF_SH" "$testcase")
ref_ret=$?
if [ $mrsh_ret -ne $ref_ret ] || [ "$mrsh_out" != "$ref_out" ]
then
	echo >&2 "$testcase: mismatch"
	echo >&2 ""
	echo >&2 "mrsh: $mrsh_ret"
	echo >&2 "$mrsh_out"
	echo >&2 ""
	echo >&2 "ref ($REF_SH): $ref_ret"
	echo >&2 "$ref_out"
	echo >&2 ""
	exit 1
fi
