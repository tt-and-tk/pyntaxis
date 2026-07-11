#!/bin/bash
# testerエージェント：ソースファイルへの書き込みを禁止する
input=$(cat)
file_path=$(echo "$input" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('file_path',''))" 2>/dev/null)
for src in "asm2bin.cpp" "asm2bin.hpp" "util.hpp"; do
    if [[ "$file_path" == *"$src"* ]]; then
        echo "testerエージェントはソースファイルの編集は禁止されています: $file_path" >&2
        exit 1
    fi
done
exit 0
