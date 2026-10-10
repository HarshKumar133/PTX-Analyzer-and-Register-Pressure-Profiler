#!/bin/sh
set -eu
cmake --preset debug-sanitizers
cmake --build --preset debug-sanitizers --parallel 2
ctest --preset debug-sanitizers
