; call引数過多のエラーテスト（異常系）
; callの引数は呼び出し先関数名1つのみ

.global main, func

main:
    call func func
    ret

func:
    ret
