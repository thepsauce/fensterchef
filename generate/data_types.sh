# Include me into your shell with `source <script>`.
# I supply a map called `data_types` that stores the data types the
# configuration parser supports.

declare -a data_types
declare -A data_type_c
declare -A data_type_comments

{
# skip the inital notice
while read -r line ; do
    if [ -z "$line" ] ; then
        break
    fi
done

while read -r line ; do
    name="${line%% *}"
    c_type="${line#* }"
    data_types+=("$name")
    data_type_c["$name"]="$c_type"
    read -r comment
    data_type_comments["$name"]="$comment"
done 
} <generate/fensterchef.data_types
