#!/bin/bash

set -e

script="$1"

[ -d "$script" ] && exit 1
[ -x "$script" ] || exit 1

file_name="${script##*/}"
file_name="${file_name%.*}"

if [ "${file_name: -1:1}" = "c" ] ; then
    include_file="src/$file_name"
else
    include_file="include/bits/$file_name"
fi

temporary_file="$(mktemp)"

./"$script" <"$include_file" >"$temporary_file"

\mv -f "$temporary_file" "$include_file"
