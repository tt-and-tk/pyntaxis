; ret無し関数のエラーテスト（異常系）
; 全関数にretが1つ以上必要だが，nofuncはretを持たない

.global main, nofunc

main:
    ret

nofunc:
    nop
