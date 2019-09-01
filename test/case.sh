#!/bin/sh

x=hello

echo "exact matching with variable expansion"
case "$x" in
	hello)
		echo pass
		;;
	world)
		echo fail
		;;
esac

echo "* pattern"
case "$x" in
	he*)
		echo pass
		;;
	*)
		echo fail
		;;
esac

echo "? pattern"
case "$x" in
	foo)
		echo fail
		;;
	he??o)
		echo pass
		;;
esac

echo "default pattern"
case "$x" in
	foo)
		echo fail
		;;
	*)
		echo pass
		;;
esac

echo "| pattern"
case "$x" in
	world|hello)
		echo pass
		;;
	*)
		echo fail
		;;
esac

echo "[] pattern"
case "$x" in
	hell[a-z])
		echo pass
		;;
	*)
		echo fail
		;;
esac

y=hello

echo "expanding patterns"
case "$x" in
	$y)
		echo pass
		;;
	*)
		echo fail
		;;
esac

echo "quoted strings in patterns"
case "$x" in
	"$y"'')
		echo pass
		;;
	*)
		echo fail
		;;
esac

echo ";; optional for last item"
case hello in
	*)
		echo pass
esac
