#!/bin/sh
dir=$(dirname "$0")
testcase="$1"
stdout="${testcase%%.sh}.stdout"

# Set TEST_SHELL to quickly compare the conformance of different shells
mrsh=${TEST_SHELL:-$MRSH}

actual_out=$("$mrsh" "$testcase")
actual_ret=$?

if [ -f "$stdout" ]
then
	stdout="$(cat "$stdout")"
	if [ "$stdout" != "$actual_out" ]
	then
		exit 1
	fi
fi

exit $actual_ret
