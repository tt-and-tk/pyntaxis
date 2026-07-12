; 呼び出し深さ11段が正常にアセンブルできることのテスト（正常系）
; main → f1 → f2 → ... → f11 の合計12段（深さ11）の呼び出し
; 戻り先レジスタの本数はハードウェアの実行時都合であり，
; アセンブラは静的な深さ検査をしないためそのまま変換できる

.global main, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11

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
    call f10
    ret

f10:
    call f11
    ret

f11:
    nop
    ret
