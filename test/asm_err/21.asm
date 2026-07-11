; 未定義ラベルの参照 → エラーになるべき

.global main

main:
    jmp .Lx   ; .Lx はどこにも定義されていない
    ret
