; .global より前にコードがあるエラーテスト（異常系）
; .global 宣言より前は空行とコメント行のみ許可される
; 下の add 命令のようにコメント以外の記述があるとエラーになるべき

    add r0 r0 r0

.global main

main:
    ret
