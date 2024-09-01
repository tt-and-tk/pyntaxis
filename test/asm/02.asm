; 関数呼び出しを含むアセンブリファイル

.global func, main

func:
    ret

main:
    call func
    ret
