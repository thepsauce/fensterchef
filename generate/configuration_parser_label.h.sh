#!/bin/bash

set -e

marker='typedef enum configuration_parser_label {'
marker_end='} parser_label_t;'

found_marker=false

IFS=
while read -r line ; do
    echo "$line"
    if [ "$line" = "$marker" ] ; then
        found_marker=true

        while read -r line ; do
            if [ "$line" = "$marker_end" ] ; then
                break
            fi
        done

        while read -r line ; do
            if [ "${line:0:1}" != "[" ] ; then
                continue
            fi
            line="${line:1}"
            line="${line%]*}"
            line="${line^^}"
            line="PARSER_LABEL_$line"
            echo "    $line,"
        done <generate/fensterchef.labels

        echo
        echo "    PARSER_LABEL_MAX"

        echo "$marker_end"
    fi
done

if ! $found_marker ; then
    echo "MARKER NOT FOUND - $0"
    exit 1
fi
