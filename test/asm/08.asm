; 再帰呼び出しが正常にアセンブルできることのテスト（正常系）
; ハードウェアのCALL/RETが戻り先レジスタ(最大10階層)で再帰を実行時サポートするため，
; アセンブラは再帰呼び出しをそのまま機械語へ変換できる
; func1 が自分自身を再帰呼び出しする

.global main, func1

main:
    call func1
    ret

func1:
    call func1
    ret
