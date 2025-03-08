# Include me into your shell with `source <script>`.
# I supply a map called `data_types` that stores the data types the
# configuration parser supports.

declare -A data_types

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
    data_types["$name"]="${c_type}"
    # skip over the comment line
    read -r
done 
} <generate/fensterchef.data_types
