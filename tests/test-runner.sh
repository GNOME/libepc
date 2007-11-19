#!/bin/bash

"$@" 2> "$1.err" > "$1.out"
result=$?

test $result -ne 0 && cat "$1.err"
exit $result
