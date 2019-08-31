#!/bin/sh -eu

echo "1 =" $((1))
echo "2*5 =" $((2*5))
echo "2/5 =" $((2/5))
echo "2%5 =" $((2%5))
echo "2+5 =" $((2+5))
echo "2-5 =" $((2-5))
echo "2<<5 =" $((2<<5))
echo "2>>5 =" $((2>>5))
echo "2<5 =" $((2<5))
# https://github.com/emersion/mrsh/issues/86
#echo "2<=5 =" $((2<=5))
echo "2>5 =" $((2>5))
#echo "2>=5 =" $((2>=5))
echo "2==5 =" $((2==5))
echo "2!=5 =" $((2!=5))
echo "2&5 =" $((2&5))
echo "2^5 =" $((2^5))
echo "2|5 =" $((2|5))
#echo "2&&5 =" $((2&&5))
#echo "2||5 =" $((2||5))

# Associativity
# https://github.com/emersion/mrsh/issues/53
echo "1+2+3 =" $((1+2+3))
#echo "5-1-2 =" $((5-1-2))
echo "1+2*3 =" $((1+2*3))
echo "2*3+1 =" $((2*3+1))
#echo "2*(3+1) =" $((2*(3+1)))
