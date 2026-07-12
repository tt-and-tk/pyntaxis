; 最大呼び出し深さ10段の境界値テスト（正常系）
; main → f1 → f2 → ... → f9 の合計10段呼び出し
; 呼び出し深さの上限が10であり，これが正常系の最大

.global main, f1, f2, f3, f4, f5, f6, f7, f8, f9

main:
    call f1
    ret

f1:
    call f2
    ret

f2:
    call f3
    ret

f3:
    call f4
    ret

f4:
    call f5
    ret

f5:
    call f6
    ret

f6:
    call f7
    ret

f7:
    call f8
    ret

f8:
    call f9
    ret

f9:
    nop
    ret
