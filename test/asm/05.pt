; 同じ関数を2回callするテスト
; main から func1 を2回呼び出す
; 各 call の戻り先が正しく設定されるか確認

.global main, func1

main:
    call func1
    call func1
    ret

func1:
    add r1 r0 r0
    ret
