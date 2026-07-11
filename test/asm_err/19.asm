; 引数過多のエラーテスト（異常系）
; addは引数3つだが4つ与えている

.global main

main:
    add r1 r2 r3 r4
    ret
