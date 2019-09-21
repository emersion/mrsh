#!/bin/sh

echo ""
echo "Tilde Expansion"
echo ~ ~/stuff ~/"stuff" "~/stuff"
echo '~/stuff' ~"/stuff" "/"~ "a"~"a"
echo ~root
a=~/stuff
echo $a
a=~/foo:~/bar:~/baz
echo $a

echo ""
echo "Parameter Expansion"
a=a
b=B
hello=hello
null=""
echo $a ${b} ">$a<"
echo \$a '$a'
echo ${a:-BAD} ${idontexist:-GOOD} ${null:-GOOD} ${idontexist:-}
echo ${a-BAD} ${idontexist-GOOD} ${null-BAD} ${null-}
echo ${c:=GOOD} $c; echo ${c:=BAD} $c; c=""; echo ${c:=GOOD} $c; unset c
echo ${c=GOOD} $c; echo ${c=BAD} $c; c=""; echo ${c=BAD} $c; unset c
echo ${a:+GOOD} ${idontexist:+BAD} ${null:+BAD} ${idontexist:+}
echo ${a+GOOD} ${idontexist+BAD} ${null+GOOD} ${null+}
echo ${#hello} ${#null} ${#idontexist}

# Examples from the spec
# ${parameter}: dash and busybox choke on this
#a=1
#set 2
#echo ${a}b-$ab-${1}0-${10}-$10
# ${parameter-word}
foo=asdf
echo ${foo-bar}xyz}
foo=
echo ${foo-bar}xyz}
unset foo
echo ${foo-bar}xyz}
# ${parameter:-word}
#unset x
#echo ${x:-$(echo >&2 GOOD)} 2>&1
#x=x
#echo ${x:-$(echo >&2 BAD)} 2>&1
# ${parameter:=word}
unset X
echo ${X:=abc}
# ${parameter:?word}
#unset posix
#echo ${posix:?}
# ${parameter:+word}
set a b c
echo ${3:+posix}
# ${#parameter}
posix=/usr/posix
echo ${#posix}
# ${parameter%word}
x=file.c
echo ${x%.c}.o
# ${parameter%%word}
x=posix/src/std
echo ${x%%/*}
# ${parameter#word}
x=$HOME/src/cmd
echo ${x#$HOME}
# ${parameter##word}
x=/one/two/three
echo ${x##*/}

echo ""
echo "Command Substitution"
echo $(echo asdf)
echo `echo asdf`

# Field Splitting
# Pathname Expansion
# Quote Removal
