#!/bin/bash

# Exit when an error occurs
set -e

# $1 - name of the generate script
# Usually in the form generate/*.sh
script="$1"

# The script must not be a directory and must be executable
[ -d "$script" ] && exit 1
[ -x "$script" ] || exit 1

# Get the name of the file
file_name="${script##*/}"
# Trim of the last file extension (.sh)
file_name="${file_name%.*}"
# Replace all __ with / (a convention we use)
file_name="${file_name//__//}"

# Get the actual file name
case "$file_name" in
*.c)
    file="src/$file_name"
    ;;
*.h)
    file="include/$file_name"
    ;;
*)
    file="man/$file_name"
    ;;
esac

# Write into a temporary file and then use it to replace the real file
temporary_file="$(mktemp)"

./"$script" <"$file" >"$temporary_file"

\mv -f "$temporary_file" "$file"
