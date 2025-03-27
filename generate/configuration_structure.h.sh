#!/bin/bash

set -e

source generate/data_types.sh

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

        # an empty line signals stop
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

            c_type="${data_type_c[$variable_type]}"
            if [ -z "$c_type" ] ; then
                echo "WHAT IS $variable_type - $0" >&2
                exit 1
            fi
            
            c_type_length="${c_type%% *}"
            variable_name="${variable_name//-/_}"

            line="${c_type#* } "
            case "$c_type_length" in
            1) line+="$variable_name" ;;
            "*") line+="*$variable_name" ;;
            *) line+="$variable_name[$c_type_length]" ;;
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

replace_lines() {
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
}

start_marker='/*< START OF CONFIGURATION >*/'
end_marker='/*< END OF CONFIGURATION >*/'
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
