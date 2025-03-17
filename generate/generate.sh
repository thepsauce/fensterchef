#!/bin/bash

set -e

script="$1"

[ -d "$script" ] && exit 1
[ -x "$script" ] || exit 1

file_name="${script##*/}"
file_name="${file_name%.*}"

case "$file_name" in
*.c)
    file="src/$file_name"
    ;;
*.h)
    file="include/bits/$file_name"
    ;;
*)
    file="man/$file_name"
    ;;
esac

temporary_file="$(mktemp)"

./"$script" <"$file" >"$temporary_file"

\mv -f "$temporary_file" "$file"
