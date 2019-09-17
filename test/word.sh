#!/bin/sh

echo ""
echo "Tilde Expansion"
echo ~ ~/stuff # ~"/stuff"
echo ~root
#a=~/stuff
#echo $a

echo ""
echo "Parameter Expansion"
a=a
b=B
hello=hello
null=""
echo $a ${b} ">$a<"
echo \$a '$a'
echo ${a:-BAD} ${idontexist:-GOOD} ${null:-GOOD}
echo ${a-BAD} ${idontexist-GOOD} ${null-BAD}
#echo ${#hello}

echo ""
echo "Command Substitution"
echo $(echo asdf)
echo `echo asdf`

echo ""
echo "Arithmetic Expansion"
#echo $((1+2))

# Field Splitting
# Pathname Expansion
# Quote Removal
