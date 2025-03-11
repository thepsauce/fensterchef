#!/bin/bash

set -e

source generate/data_types.sh

marker='static const struct configuration default_configuration = {'
marker_end='};'

found_marker=false

IFS=

# $1 -> data type
# $2 -> data value
data_type_to_string() {
    case "$1" in
    boolean|integer|quad)
        echo "$2"
        ;;
    string)
        echo "someone forgot to change this - $0" >&2
        return 1
        ;;
    color)
        echo "0x${2:1}"
        ;;
    modifiers)
        IFS='+'
        local first_time=true
        for m in $2 ; do
            if ! $first_time ; then
                echo -n " | "
            fi
            first_time=false

            case "$m" in
            Mod*)
                m="${m:3}"
                ;;
            esac
            echo -n "XCB_MOD_MASK_${m^^}"
        done
        IFS=
        ;;
    *)
        echo "WHAT IS $1 - $0" >&2
        return 1
    esac
}

# $1 -> label declaration
write_default_label_options() {
    local label
    local variable_count
    local count
    local line

    label="${1:1}"
    label="${label%]*}"

    echo "    .$label = {"

    variable_count=0
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

        # skip the comment which may span across multiple lines
        while read -r next_line ; do
            if [ "${next_line:0:1}" != " " ] ; then
                break
            fi
        done

        case "$line" in
        "*"*|!*)
            continue
            ;;
        esac

        let variable_count++ || true

        variable_name="${line%% *}"

        variable_type="${line#* }"
        variable_type="${variable_type%% *}"

        data_type="${data_types[$variable_type]}"
        if [ -z "$data_type" ] ; then
            echo "WHAT IS $variable_type - $0" >&2
            return 1
        fi
        
        data_type_length="${data_type%% *}"
        variable_name="${variable_name//-/_}"

        variable_value="${line#* }"
        variable_value="${variable_value#* }"

        line="$variable_name = "
        case "$data_type_length" in
        1)
            line+="$(data_type_to_string "$variable_type" "$variable_value")"
            ;;
        "*")
            line+="(${data_type##* }*) \"$variable_value\""
            ;;
        *)
            line+="{ "
            count=0
            IFS=' '
            for v in $variable_value ; do
                if let count++ ; then
                    line+=", "
                fi

                line+="$(data_type_to_string "$variable_type" "$v")"
            done
            IFS=

            if [ $count -ne $data_type_length ] ; then
                echo $count >&2
                echo "amount mismatch at $label.$variable_name - $0" >&2
                return 1
            fi

            line+=" }"
            ;;
        esac

        echo "        .$line,"
    done

    if [ $variable_count -eq 0 ] ; then
        echo "        0"
    fi
    echo "    },"
}

# go through all lines of the sources file and find the right section
while read -r line ; do
    echo "$line"
    if [ "$line" = "$marker" ] ; then
        found_marker=true

        # consume the lines
        while read -r line ; do
            if [ "$line" = "$marker_end" ] ; then
                break
            fi
        done

        first_time=true
        while read -r line ; do
            if [ "${line:0:1}" != "[" ] ; then
                continue
            fi

            if ! $first_time ; then
                echo
            fi
            first_time=false

            write_default_label_options "$line"
        done <generate/fensterchef.labels

        echo "$marker_end"
    fi
done

if ! $found_marker ; then
    echo "MARKER NOT FOUND - $0"
    exit 1
fi
