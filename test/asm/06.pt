; 複数の異なる関数を順次callするテスト
; main から func1 → func2 → func3 の順に呼び出す

.global main, func1, func2, func3

main:
    call func1
    call func2
    call func3
    ret

func1:
    add r1 r0 r0
    ret

func2:
    add r2 r0 r0
    ret

func3:
    add r3 r0 r0
    ret
