# アセンブラの内部実装

`asm2bin.cpp`がアセンブリ言語Pyntaxis(`.pt`)をどのように処理してSystemVerilog ROM(`.sv`)へ変換しているかを説明する。言語仕様・命令フォーマットそのものは`../specification/assembler.md`・`../specification/isa.md`を参照(このファイルには記載しない)。

## ファイル構成

- `asm2bin.cpp` — アセンブラ本体。引数処理、`.global` 解析、ラベル解決、命令変換、SV 出力
- `asm2bin.hpp` — 命令テーブル (`commands`) と引数種別 (`arg_t`) の定義
- `util.hpp` — 文字列補助関数(`str_find_first_of`, `ltrim`, `b2d`, `o2d`, `h2d`, `replace`)
- `asm2bin_main.hpp` — `assemble_asm_to_sv()` を宣言する公開エントリポイント。pynesis(コンパイラ)側が本リポジトリのソースをincludeして直接呼び出すために使う(`ASM2BIN_NO_MAIN`マクロで`asm2bin.cpp`側の`main`定義と衝突しないようにする)

## アセンブル処理の流れ

1. コマンドライン引数から `.pt` と出力 `.sv` パスを決定(`get_args`)
2. `read_global_line` で `.global` 行まで読み飛ばし、前置きの妥当性検証
3. `get_function_names` で関数名一覧を収集(アドレスは `npos` で初期化)
4. `assemble_body` で命令を 1 行ずつ変換。関数ラベル出現時にアドレスを確定(main を先頭 pc=0 から配置)、局所ラベル(`.` 始まり)は別表 `local_labels` に位置を記録。全関数の ret 有無も追跡
5. 宣言済み全関数の定義を確認(`npos` が残っていればエラー)
6. `resolve_labels` で局所ラベル参照を解決(jmp は絶対 index、F系は相対オフセット)。未定義ラベル参照はエラー
7. `apply_main_self_loop` で先頭から線形探索し、最初の `ret` を自己ループ `jmp` に置換
8. `join_instructions` で命令を連結し、`function_name2line_num` で関数参照を行番号に置換して出力

## call/ret/jmp の実装方針

`call`・`ret`・`jmp` は CPU の正式命令(J系)。アセンブラはそれぞれ機械語1命令に直接変換する。

- `call func` → `call(0, 33'h1_0000_0000 + <func の行番号>)`(呼び出し先 pc を即値で渡す)
- `jmp .L0` → `jmp(0, 33'h1_0000_0000 + <.L0 の絶対 index>)`(飛び先は局所ラベルのみ。`rs1=0` 固定)
- `ret` → `ret()`(引数なし)
- `call` と `jmp` は「`rs1=0` +即値ターゲット」という同じ特殊な出力形のため、`output_bin_line()` 内でまとめて特別に組み立てる(汎用経路には乗らない)。`ret` は汎用経路で `ret()` を生成する
- 戻り先アドレスの保存・SP の更新・戻り先レジスタの選択はすべて CPU(CALL/RET 命令)が行う(具体的なレジスタ番地は`../specification/register.md`を参照)
- アセンブラは呼び出しネストの深さを静的検査しない(`test/asm/09.pt`は11段の呼び出しが正常にアセンブルできることを確認している)。ハードウェア上の戻り先レジスタ本数による実行時の上限は`../specification/limitations.md`を参照

## main の自己ループ置換

main は CALL で呼ばれないため戻り先が無い。プログラムは pc=0 から実行されるため、先頭から線形に探索して**最初に現れる `ret`** が「main が CALL されずに到達する戻り先の無い ret」になる。アセンブラはこの ret を自分自身の pc への `jmp`(無限ループ)に自動置換する。ret の位置・個数は問わない(ソース上に ret が存在しさえすれば置換できる)。

## 関数名解決の仕組み

`convert_arg()` で関数名を `@func@` のように区切り文字 `@` で囲んで出力に残し、全命令の変換が終わった後で `function_name2line_num()` が `@func@` を実アドレス(行番号)に一括置換する。区切り文字で囲むことで、関数名が命令名や数値の部分文字列と一致しても誤置換が起きない。

## 局所ラベル解決の仕組み

`jmp`・F系の飛び先は局所ラベル(`.L0` など、`.` 始まり)のみで指定する。`assemble_body` で `.` 始まりのラベルを `local_labels` に位置(直後の命令の index)として記録する(関数とは別の名前空間。`.global` 宣言不要、プログラム全体で一意)。

参照側は `convert_arg()` で仮文字列(プレースホルダ)に変換しておき、全命令の変換後に `resolve_labels()` が実値へ置換する。

- `jmp`(絶対): `33'h1_0000_0000 + <<ABS:.L0>>` → ラベルの**絶対 index** に置換(`PC = imm`)
- F系(相対): `33'h1_0000_0000 + <<REL:.L0>>` → 「ラベルの index − その命令自身の pc」(**相対オフセット**、`PC += imm`)に置換
- 命令の vector 上のインデックスがそのまま pc なので、`resolve_labels` はループの index を自命令 pc として相対オフセットを計算する
- 後方ジャンプは相対オフセットが負になる。負値は 33bit 目の即値使用フラグを落とさないよう **32bit 2の補数の hex**(例 `32'hfffffffc`)で出力する(`offset2imm`)
- プレースホルダ(`<<ABS:`/`<<REL:`/`>>`)の文字列定数は `asm2bin.cpp` 冒頭で定義。置換されずに残った参照は未定義ラベルとしてエラー

## 出力フォーマット

```systemverilog
`include "rom.svh"
`include "machine.svh"

module rom_sv(
    rom_read_if.slave rom_read
    );
    import machine_p::*;

    localparam integer ROM_SIZE = 4;

    machine_t machines[0:ROM_SIZE - 1] = {
        call(0, 33'h1_0000_0000 + 3),
        call(0, 33'h1_0000_0000 + 3),
        jmp(0, 33'h1_0000_0000 + 2),
        ret()
    };

    always_comb begin
        if (rom_read.pc >= ROM_SIZE) begin
            rom_read.machine = nop();
        end else begin
            rom_read.machine = machines[rom_read.pc];
        end
    end

endmodule
```

命令は 64bit(`machine_t` = `logic [63:0]`)。`ROM_SIZE` は実際の命令数。末尾カンマなし(SV の配列初期化子では末尾カンマが構文エラーになるため)。`import machine_p::*` により `machine::` プレフィックスなしで各命令関数を呼び出せる。
