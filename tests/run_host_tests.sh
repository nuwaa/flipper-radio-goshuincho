#!/bin/sh
set -eu

test_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(dirname -- "$test_dir")
output_file="${TMPDIR:-/tmp}/radio_goshuincho_stamp_test"

cc -std=c11 -Wall -Wextra -Werror \
    -I"$test_dir/include" -I"$repo_dir" \
    -x c "$test_dir/stamp_host_test.c.txt" \
    "$repo_dir/goshuin_stamp.c" "$repo_dir/goshuin_multiband.c" \
    -o "$output_file"
"$output_file"
