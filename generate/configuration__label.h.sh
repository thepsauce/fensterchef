#!/bin/bash

set -e

replace_lines() {
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
}

start_marker='typedef enum configuration_parser_label {'
end_marker=''
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
