#!/bin/bash
# bin-ans-creatorエージェント：test/bin_ans/ 以外への書き込みを禁止する
input=$(cat)
file_path=$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null)
if [[ ! "$file_path" =~ bin_ans ]]; then
    echo "bin-ans-creatorエージェントはtest/bin_ans/以外への書き込みは禁止されています: $file_path" >&2
    exit 1
fi
exit 0
