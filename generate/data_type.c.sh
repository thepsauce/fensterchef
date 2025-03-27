#!/bin/bash

set -e

source generate/data_types.sh

replace_lines() {
    for d in "${data_types[@]}" ; do
        echo "    [DATA_TYPE_${d^^}] = sizeof(((GenericData*) 0)->$d),"
    done
}

start_marker='    [DATA_TYPE_VOID] = 0,'
end_marker='};'
source generate/replace_markers.sh

while read -r line ; do
    echo "$line"
done
