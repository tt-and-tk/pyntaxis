; 関数呼び出しを含むアセンブリファイル

.global func, main

main:
    call func
    ret

func:
    ret
