#!/bin/bash -e
# Reshape 5 number summaries to a row of summary.csv

file="$1"
dir="$(dirname "$file")"
type="$(basename "$dir" | tr - ,)"

dir="$(dirname "$dir")"
host="$(basename "$dir")"
echo -n "$host,$type,"

grep -F : "$file" | grep -v "NA's" | sed 's/\s\+[0-9]*[a-zA-Z. ]\+\s*//g;s/:/ /g' | datamash -W transpose | xargs | tr ' ' ,
