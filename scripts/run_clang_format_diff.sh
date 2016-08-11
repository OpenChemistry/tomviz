#!/bin/bash
DIFF=`git diff -U0 master...$1 | clang-format-diff-3.8 -p1`
if [ -z "$DIFF" ]; then
  exit 0
else
  printf "clang-format-diff detected formatting issues. Please run clang format on your branch.\n\n%s" "$DIFF"
  exit 1
fi
