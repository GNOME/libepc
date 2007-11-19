#!/bin/bash

test -n "$EPC_DEBUG" && echo "$0: running $1..."
"$@" 2> "$1.err" > "$1.out"
result=$?

if test -n "$EPC_DEBUG"
then
  echo "===== BEGIN OF STDOUT ====="
  cat "$1.out"
  echo "===== END OF STDOUT ====="

  echo "===== BEGIN OF STDERR ====="
  cat "$1.out"
  echo "===== END OF STDERR ====="
elif test $result -ne 0
then
  cat "$1.err"
fi

exit $result
