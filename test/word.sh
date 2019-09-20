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
echo ${a+GOOD} ${idontexist+BAD} ${null-GOOD} ${null+}
echo ${#hello} ${#null} ${#idontexist}

echo ""
echo "Command Substitution"
echo $(echo asdf)
echo `echo asdf`

# Field Splitting
# Pathname Expansion
# Quote Removal
