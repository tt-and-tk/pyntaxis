; 二重に関数を呼び出す

.global func1, func2, main

func2:
    ret

func1:
    call func2
    ret

main:
    call func1
    ret
