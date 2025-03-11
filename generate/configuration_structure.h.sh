#!/bin/bash

set -e

source generate/data_types.sh

IFS=

START_MARKER='/*< START OF CONFIGURATION >*/'
END_MARKER='/*< END OF CONFIGURATION >*/'

found_marker=false

labels=( )

# $1 -> label declaration
write_struct() {
    label="${1:1}"
    special_entry="${label#*\*}"
    label="${label%]*}"
    if [ "${special_entry: -1:1}" != "]" ] ; then
        is_special=true
    else
        is_special=false
    fi
    labels+=( "$label" )

    echo
    echo "/* $label settings */"
    echo "struct configuration_$label {"
    unset next_line
    while : ; do
        if [ -z "${next_line+x}" ] ; then
            read -r line || break
        else
            line="$next_line"
        fi

        # an EMPTY line signals stop
        if [ "$line" = "" ] ; then
            break
        fi

        # read the comment which may span across multiple lines
        comments=()
        while read -r next_line ; do
            if [ "${next_line:0:1}" != " " ] ; then
                break
            fi
            comments+=( "${next_line:3}" )
        done

        case "$line" in
        "*"*)
            line="${line:1}"
            ;;
        "!"*)
            continue
            ;;
        *)
            variable_name="${line%% *}"

            variable_type="${line#* }"
            variable_type="${variable_type%% *}"

            data_type="${data_types[$variable_type]}"
            if [ -z "$data_type" ] ; then
                echo "WHAT IS $variable_type - $0" >&2
                exit 1
            fi
            
            data_type_length="${data_type%% *}"
            variable_name="${variable_name//-/_}"

            line="${data_type#* } "
            case "$data_type_length" in
            1) line+="$variable_name" ;;
            "*") line+="*$variable_name" ;;
            *) line+="$variable_name[$data_type_length]" ;;
            esac
            ;;
        esac

        if [ "${#comments[@]}" -eq 1 ] ; then
            echo "    /* ${comments[0]} */"
        else
            echo "    /* ${comments[0]}"
            for c in "${comments[@]:1}" ; do
                echo "     * $c"
            done
            echo "     */"
        fi
        echo "    $line;"
    done
    echo "};"
}

while read -r line ; do
    echo "$line"
    if [ "$line" = "$START_MARKER" ] ; then
        found_marker=true

        while read -r line ; do
            if [ "$line" = "$END_MARKER" ] ; then
                break
            fi
        done

        while read -r line ; do
            if [ "${line:0:1}" != "[" ] ; then
                continue
            fi
            write_struct "$line"
        done <generate/fensterchef.labels

        echo
        echo "/* configuration settings */"
        echo "struct configuration {"
        for l in "${labels[@]}" ; do
            echo "    /* $l settings */"
            echo "    struct configuration_$l $l;"
        done
        echo "};"

        echo
        echo "$END_MARKER"
    fi
done

if ! $found_marker ; then
    echo "MARKER NOT FOUND - $0" >&2
    exit 1
fi
