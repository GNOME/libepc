#!/bin/bash

test -n "$EPC_DEBUG" && echo "$0: running $1..."

EPC_DEBUG="${EPC_DEBUG:-1}" \
G_DEBUG="${G_DEBUG:-fatal-warnings}" \
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
fi

exit $result
