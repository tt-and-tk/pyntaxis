; 未定義関数の呼び出しエラーテスト（異常系）
; .global に宣言されていない関数を call するとエラーになるべき

.global main

main:
    call undefined_func
    ret
