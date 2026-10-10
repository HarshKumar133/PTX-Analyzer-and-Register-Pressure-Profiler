#!/bin/sh
set -eu
build_dir=${1:-build-default}
cmake --build "$build_dir" --parallel 2
ctest --test-dir "$build_dir" --output-on-failure
