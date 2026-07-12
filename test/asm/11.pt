; S系・F系・J系命令の動作確認（前方/後方ジャンプ含む）

.global main

main:
.L1:                ; index 0（後方ジャンプの飛び先）
    sll r1 r2 r3
    srl r1 r2 r3
    sla r1 r2 r3
    sra r1 r2 r3
    eq r1 r2 .L0    ; 前方 → .L0(12) - 4 = 8
    ne r1 r2 .L0    ; 前方 → 12 - 5 = 7
    lt r1 r2 .L0    ; 前方 → 12 - 6 = 6
    gt r1 r2 .L0    ; 前方 → 12 - 7 = 5
    elt r1 r2 .L0   ; 前方 → 12 - 8 = 4
    egt r1 r2 .L1   ; 後方 → .L1(0) - 9 = -9（負オフセット）
    jmp .L0         ; 絶対 → 12
    jmp .L1         ; 絶対 → 0
.L0:
    ret
