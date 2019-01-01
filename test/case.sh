#!/bin/sh
x=hello

case "$x" in
	hello)
		echo pass
		;;
	world)
		echo fail
		;;
esac

case "$x" in
	he*)
		echo pass
		;;
	*)
		echo fail
		;;
esac

case "$x" in
	foo)
		echo fail
		;;
	he??o)
		echo pass
		;;
esac

case "$x" in
	foo)
		echo fail
		;;
	*)
		echo pass
		;;
esac

case "$x" in
	world|hello)
		echo pass
		;;
	*)
		echo fail
		;;
esac

case "$x" in
	hell[a-z])
		echo pass
		;;
	*)
		echo fail
		;;
esac

y=hello

# Expanding patterns
case "$x" in
	$y)
		echo pass
		;;
	*)
		echo fail
		;;
esac

# ;; optional for last item
case hello in
	*)
		echo pass
esac
