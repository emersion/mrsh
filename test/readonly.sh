#!/bin/sh

echo "Print read-only parameters after setting one"
readonly mrsh_readonly_param=b
readonly -p | grep mrsh_readonly_param | wc -l

#echo "Try setting a read-only parameter"
#(mrsh_readonly_param=c) 2>/dev/null
#echo $?
