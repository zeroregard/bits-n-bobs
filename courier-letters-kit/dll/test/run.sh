#!/bin/bash
# Tests the letter files' parser and conditions on Linux, against stand-ins for CommonLib (PCH.h here): no game needed.
#   dll/test/run.sh
set -e
cd "$(dirname "$0")"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cp ../src/Main.cpp PCH.h test.cpp "$T"/
${CXX:-clang++} -std=c++23 -Wall -Wno-unused-parameter -Wno-unused-function -DWORK="\"$T\"" -DEXAMPLES="\"$(cd ../../examples && pwd)\"" "$T/test.cpp" -o "$T/test"
"$T/test"
