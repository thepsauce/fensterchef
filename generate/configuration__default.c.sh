#!/bin/bash

set -e

source generate/data_types.sh

is_first_struct_item=true

# $1 -> data type
# $2 -> data value
data_type_to_string() {
    case "$1" in
    quad)
        echo "$2"
        ;;
    integer)
        result=""
        case "$2" in
        [0-9]*|true|false)
            result="$2"
            ;;
        '#'*)
            result="0xff${2:1}"
            ;;
        Shift*|Lock*|Control*|Mod*)
            IFS='+'
            local first_time=true
            for m in $2 ; do
                if ! $first_time ; then
                    result+=" | "
                fi
                first_time=false

                case "$m" in
                Mod*)
                    m="Mod${m:3}Mask"
                    ;;
                *)
                    m="${m}Mask"
                    ;;
                esac
                result+="$m"
            done
            IFS=
            ;;
        *)
            echo "HMMM??? $1" >&2
            return 1
            ;;
        esac
        echo "$result"
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
    local next_line

    label="${1:1}"
    label="${label%]*}"

    variable_count=0
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

        if ! let variable_count++ ; then
            if ! $is_first_struct_item ; then
                echo
            fi
            echo "    .$label = {"
        fi

        variable_name="${line%% *}"

        variable_type="${line#* }"
        variable_type="${variable_type%% *}"

        c_type="${data_type_c[$variable_type]}"
        if [ -z "$c_type" ] ; then
            echo "WHAT IS $variable_type - $0" >&2
            return 1
        fi
        
        data_type_length="${c_type%% *}"
        variable_name="${variable_name//-/_}"

        variable_value="${line#* }"
        variable_value="${variable_value#* }"

        line="$variable_name = "
        case "$data_type_length" in
        1)
            line+="$(data_type_to_string "$variable_type" "$variable_value")"
            ;;
        "*")
            line+="\"$variable_value\""
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

    if [ $variable_count -gt 0 ] ; then
        echo "    },"
        is_first_struct_item=false
    fi
}

replace_lines() {
    while read -r line ; do
        if [ "${line:0:1}" != "[" ] ; then
            continue
        fi

        write_default_label_options "$line"
    done <generate/fensterchef.labels
}

start_marker='const struct configuration default_configuration = {'
end_marker='};'
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
