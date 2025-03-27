# Utility script that is included within other script using `source`.
#
# This reads lines from stdin and outputs them to stdout.
# However when a line matches `start_marker`, it skips all lines until the
# `end_marke`.
#
# After skipping the lines `replace_lines()` is called. This function must be
# defined before sourcing this script.

_found_marker=false

# preserve all whitespace
IFS=

while read -r line ; do
    echo "$line"
    if [ "$line" = "$start_marker" ] ; then

        while read -r line ; do
            if [ "$line" = "$end_marker" ] ; then
                _found_marker=true
                break
            fi
        done

        replace_lines

        echo "$end_marker"

        break
    fi
done

if ! $_found_marker ; then
    echo "MARKER '$start_marker' OR '$end_marker' NOT FOUND - $0"
    exit 1
fi

