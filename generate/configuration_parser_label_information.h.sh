#!/bin/bash

set -e

source generate/data_types.sh

IFS=

declare -A labels
declare -A label_special_entries
declare -a labels_in_order

let max_count=1

# Read the `!command` and regular variables.
# This also already writes the special parser function declarations.
# $1 -> label declaration
read_label_variables_and_commands() {
    label="${1:1}"
    special_entry="${label#*\*}"
    label="${label%]*}"
    if [ "${special_entry: -1:1}" != "]" ] ; then
        label_special_entries["$label"]="$special_entry"
        echo "static parser_error_t parse_${label}_$special_entry(Parser *parser);"
    fi

    inner=""
    let count=1
    while read -r line ; do
        if [ -z "$line" ] ; then
            break
        fi

        # ignore the comment
        read -r comment

        case "$line" in
        \**)
            continue
            ;;
        !*)
            ;;
        *)
            let count++
            ;;
        esac

        if [ -n "$inner" ] ; then
            inner+=$'\n'
        fi
        inner+="$line"
    done

    if [ $count -gt $max_count ] ; then
        max_count=$count
    fi

    labels["$label"]="$inner"
    labels_in_order+=( "$label" )
}

# skip the initial notice
read -r notice
echo "$notice"
echo

# First pass: read all data
while read -r line ; do
    if [ "${line:0:1}" != "[" ] ; then
        continue
    fi
    read_label_variables_and_commands "$line"
done <generate/fensterchef.labels

echo

IFS=$'\n'

# Second pass: Write label variables
echo '/* variables in the form <name> <value> */'
echo 'static const struct configuration_parser_label_name {'
echo '    /* the string representation of the label */'
echo '    const char *name;'
echo '    /* special handling for a label */'
echo '    parser_error_t (*special_parser)(Parser *parser);'
echo '    /* the variables that can be defined in the label */'
echo '    struct configuration_parser_label_variable {'
echo '        /* name of the variable */'
echo '        const char *name;'
echo '        /* type of the variable */'
echo '        parser_data_type_t data_type;'
echo '        /* offset within a `struct configuration` */'
echo '        size_t offset;'
echo "    } variables[$max_count];"
echo '} labels[PARSER_LABEL_MAX] = {'

first_time=true
for label in "${labels_in_order[@]}" ; do
    inner="${labels[$label]}"

    if ! $first_time ; then
        echo
    fi
    first_time=false

    special_entry="${label_special_entries[$label]}"
    if [ -z "$special_entry" ] ; then
        special_function="NULL"
    else
        special_function="parse_${label}_$special_entry"
    fi
    echo "    [PARSER_LABEL_${label^^}] = {"
    echo "        \"$label\", $special_function, {"
    for v in $inner ; do
        case "$v" in
        !*)
            # skip this for now, this comes later
            ;;
        *)
            variable_name="${v%% *}"

            variable_type="${v#* }"
            variable_type="${variable_type%% *}"
            data_type="${data_types[$variable_type]}"
            if [ -z "$data_type" ] ; then
                echo "WHAT IS $variable_type - $0" >&2
                exit 1
            fi
            
            data_type_length="${data_type%% *}"
            c_variable_name="${variable_name//-/_}"
            echo "        { \"${variable_name}\", PARSER_DATA_TYPE_${variable_type^^},"
            echo "            offsetof(struct configuration, $label.$c_variable_name) },"
            ;;
        esac
    done
    echo "        /* null terminate the end */"
    echo "        { NULL, 0, 0 } }"
    echo "    },"
done

echo "};"
echo

# Third pass: Write label command function declarations
let max_count=1
for label in "${labels_in_order[@]}" ; do
    inner="${labels[$label]}"

    let count=1
    for v in $inner ; do
        case "$v" in
        !*)
            v="${v:1}"

            echo "static parser_error_t ${v//-/_}_$label(Parser *parser);"

            let count++
            ;;
        esac
    done

    if [ $count -gt $max_count ] ; then
        max_count=$count
    fi
done

echo

# Fourth pass: Write label command array
echo '/* the parser commands are in the form <command> <arguments> */'
echo 'static const struct configuration_parser_command {'
echo '    /* the name of the command */'
echo '    const char *name;'
echo '    /* the procedure to execute (parses and executes the command) */'
echo '    parser_error_t (*procedure)(Parser *parser);'
echo "} commands[PARSER_LABEL_MAX][$max_count] = {"

first_time=true
for label in "${labels_in_order[@]}" ; do
    inner="${labels[$label]}"

    if ! $first_time ; then
        echo
    fi
    first_time=false

    echo "    [PARSER_LABEL_${label^^}] = {"
    for v in $inner ; do
        case "$v" in
        !*)
            v="${v:1}"

            echo "        { \"$v\", ${v//-/_}_$label },"
            ;;
        esac
    done
    echo "        /* null terminate the end */"
    echo "        { NULL, NULL }"
    echo "    },"
done

echo "};"
