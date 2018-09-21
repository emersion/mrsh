#!/bin/sh

if true; then echo 1a; fi
if true; then echo 2a; else echo 2b; fi
if false; then echo 3a; else echo 3b; fi
if false; then echo 4a; elif true; then echo 4b; else echo 4c; fi

if false; then
	echo 5a
fi
