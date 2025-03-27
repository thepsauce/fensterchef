#!/bin/bash

set -e

action_h_start_marker='\'
action_h_end_marker=''

replace_lines() {
    while read -r line ; do
        if [ "$line" = "$action_h_start_marker" ] ; then
            while read -r comment ; do
                if [ "$comment" = "$action_h_end_marker" ] ; then
                    break
                fi

                read -r X

                comment="${comment#*/\* }"
                comment="${comment% \*/*}"
                X="${X#*ACTION_}"
                name="${X%%,*}"
                name="${name//_/-}"
                name="${name,,}"

                optional="${X#*, }"
                optional="${optional%%,*}"

                data_type="${X##*, }"
                data_type="${data_type%)*}"
                data_type="${data_type#*TYPE_}"
                data_type="${data_type,,}"

                echo ".PP"
                echo ".B $name"
                if [ "$data_type" != "void" ] ; then
                    echo ".I $data_type"
                fi
                if "$optional" ; then
                    echo ".B ?"
                fi
                echo "    ${comment^}."

            done
        fi
    done <include/action.h
}

start_marker='.SH ACTION'
end_marker='.'
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
