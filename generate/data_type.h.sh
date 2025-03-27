#!/bin/bash

set -e

source generate/data_types.sh

replace_lines() {
    for d in "${data_types[@]}" ; do
        comment="${data_type_comments[$d]}"
        echo "    /* $comment */"
        echo "    DATA_TYPE_${d^^},"
    done
}

start_marker='    DATA_TYPE_VOID,'
end_marker=''
source generate/replace_markers.sh

unset replace_lines
replace_lines() {
    for d in "${data_types[@]}" ; do
        c_type="${data_type_c[$d]}"
        comment="${data_type_comments[$d]}"
        name="${c_type%% *}"

        variable_type="${line#* }"
        variable_type="${d%% *}"

        c_type="${data_type_c[$d]}"
        c_type_length="${c_type%% *}"

        line="${c_type#* }"
        if [ "${line: -1}" != "*" ] ; then
            line+=" "
        fi
        case "$c_type_length" in
        1) line+="$d" ;;
        "*") line+="*$d" ;;
        *) line+="$d[$c_type_length]" ;;
        esac

        echo "    /* $comment */"
        echo "    $line;"
    done
}

start_marker='typedef union data_type_value {'
end_marker='} GenericData;'
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
