; movの負の即値（RAW_DATA）が33bit目の即値使用フラグを保持することを確認する

.global main

main:
    mov fh r0 r0 -1     ; -1 → 32bit2の補数で32'hffffffff
    mov fh r0 r1 -5     ; -5 → 32bit2の補数で32'hfffffffb
    mov fh r0 r2 0      ; 境界値(0)は10進のまま
    ret
