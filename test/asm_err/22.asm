; ラベルの重複定義 → エラーになるべき

.global main

main:
.L0:
    add r1 r2 r3
.L0:          ; .L0 が二重に定義されている
    ret
