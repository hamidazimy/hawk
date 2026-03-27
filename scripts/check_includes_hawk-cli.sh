#!/bin/bash

find hawk-cli \( -name "*.cpp" -o -name "*.hpp" \) | \
while read src; do \
    output=$(iwyu \
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
