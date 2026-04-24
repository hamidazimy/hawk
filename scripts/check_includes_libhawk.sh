#!/bin/bash

find libhawk \( -name "*.cpp" -o -name "*.hpp" \) \
    ! -name "hawk.hpp" \
    ! -name "file_mapping_windows.cpp" | \
while read src; do \
    output=$(iwyu \
        -std=c++20 \
        -Ilibhawk/include \
        -Ihawk-cli/src \
        $src 2>&1)

    if ! echo $output | grep -q "$src has correct #includes/fwd-decls"; then
        echo "$output"
        exit 1
    fi
done
