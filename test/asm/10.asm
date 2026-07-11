; P系命令の動作確認 (add は01.asmで確認済み)

.global main

main:
    and r1 r2 r3
    or r1 r2 r3
    xor r1 r2 r3
    not r1 r2
    nand r1 r2 r3
    sub r1 r2 r3
    mul r1 r2 r3
    div r1 r2 r3
    ret
