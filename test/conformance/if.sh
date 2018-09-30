#!/bin/sh
# Reference stdout:
# pass
# pass
# pass
# pass
# pass
# pass

echo >&2 "-> if..fi with true condition"
if true
then
	echo pass
fi

echo >&2 "-> if..fi with false condition"
if false
then
	echo >&2 "fail: This branch should not have run" && exit 1
fi
echo pass

echo >&2 "-> if..else..fi with true condition"
if true
then
	echo pass
else
	echo >&2 "fail: This branch should not have run" && exit 1
fi

echo >&2 "-> if..else..fi with false condition"
if false
then
	echo >&2 "fail: This branch should not have run" && exit 1
else
	echo pass
fi

echo >&2 "-> if..else..fi with true condition"
if true
then
	echo pass
else
	echo >&2 "fail: This branch should not have run" && exit 1
fi

echo >&2 "-> if..else..elif..fi with false condition"
if false
then
	echo >&2 "fail: This branch should not have run" && exit 1
elif true
then
	echo pass
else
	echo >&2 "fail: This branch should not have run" && exit 1
fi

echo >&2 "-> test exit status"
if true
then
	( exit 10 )
else
	( exit 20 )
fi
[ $# -eq 10 ] || { echo >&2 "fail: Expected status code = 10" && exit 1; }
if false
then
	( exit 10 )
else
	( exit 20 )
fi
[ $# -eq 20 ] || { echo >&2 "fail: Expected status code = 20" && exit 1; }

echo >&2 "-> test alternate syntax"
# These tests are only expected to parse, they do not make assertions
if true; then true; fi
if true; then true; else true; fi
if true; then true; elif true; then true; else true; fi
