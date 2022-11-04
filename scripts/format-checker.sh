#!/bin/sh

check_cf=${CHECK_CF:-clang-format-15}

check_format_file () {
    $check_cf -style=GNU $1 | diff $1 - > /dev/null
    return $?
}

check_format_file $1
