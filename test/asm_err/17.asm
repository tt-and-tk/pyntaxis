; .global宣言のみで定義がない関数のエラーテスト（異常系）
; func1はglobalに宣言されているがラベル定義がない

.global main, func1

main:
    call func1
    ret
