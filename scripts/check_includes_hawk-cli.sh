#!/bin/bash

git diff --staged --name-only --diff-filter=ACMR | \
grep -E '^hawk-cli/.*\.(cpp|hpp)$' | \
while read src; do \
    output=$(iwyu \
        -std=c++20 \
        -Xiwyu --mapping_file=libhawk/iwyu.imp \
        -Xiwyu --no_fwd_decls \
        -Ilibhawk/include \
        -Ihawk-cli/src \
        -Ibuild/generated \
        $src 2>&1)

    if ! echo $output | grep -q "$src has correct #includes/fwd-decls"; then
        echo "$output"
        exit 1
    fi
done
