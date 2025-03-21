#!/bin/bash

set -e

start_marker='.SH ACTION'
end_marker='.'

action_h_start_marker='\'
action_h_end_marker=''

IFS=

found_marker=false
while read -r line ; do
    echo "$line"
    if [ "$line" = "$start_marker" ] ; then
        found_marker=true

        while read -r line ; do
            if [ "$line" = "$end_marker" ] ; then
                break
            fi
        done

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

        echo "$end_marker"
    fi
done

if ! $found_marker ; then
    echo "MARKER NOT FOUND - $0"
    exit 1
fi
