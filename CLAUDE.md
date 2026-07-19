# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## このリポジトリについて

GitHubリポジトリ名: `tt-and-tk/pyntaxis`．

PYNQ-Z2 (Zynq-7000) 上に実装する自作CPUと，それを動かすソフトウェア群(アセンブラ・コンパイラ)からなる自作PCプロジェクトの一部．プロジェクト全体は以下の独立したGitHubリポジトリで構成される．

| リポジトリ(GitHub) | ディレクトリ(`pc/`配下) | 役割 |
|:-|:-|:-|
| `specification` | `specification/` | CPUアーキテクチャ・ISA・アセンブリ言語・コンパイラ仕様のドキュメント(唯一の一次情報源) |
| `pyntaxis`(本リポジトリ) | `assembler/` | 自作アセンブリ言語Pyntaxis(`.pt`) → SystemVerilog ROM(`.sv`)へのアセンブラ |
| `pynesis` | `compiler/` | 自作プログラミング言語Pynesis(`.pn`) → アセンブリ言語Pyntaxisへのコンパイラ．本リポジトリのソースファイルをincludeして使用し，`.sv`まで一貫変換も可能 |
| `qurge` | `mypc/` | CPU・メモリ・ROM等のハードウェア全体のVivadoプロジェクト(SystemVerilog + PS側C++) |
| `for-pynthesis-skills` | `for-pynthesis-skills/` | 上記各リポジトリで共有するissue起票・対応支援スキルを提供する．特定のリポジトリが主担当と判断できない，全リポジトリに影響するissueの起票先(受け皿)でもある |

```
入力(.pn) → [pynesisのコンパイラ] → アセンブリ(.pt) → [pyntaxis(本リポジトリ)のアセンブラ] → SystemVerilog ROM(.sv) → [Vivado] → PYNQ-Z2上のハードウェア(qurge)
```

## ビルドとテスト

**ビルド:**
```
g++ -o asm2bin.exe asm2bin.cpp
```

**テスト(`test/` ディレクトリで実行):**
```
cd test
python test.py        # 正常系: test/asm/*.pt を全て変換し test/bin/ へ出力
python test_err.py    # 異常系: test/asm_err/*.pt が全てエラーになることを確認
```
期待値は `test/bin_ans/` にある。

**単体実行:**
```
asm2bin.exe input.pt -bin output.sv
rem または(-bin 省略時は input.sv が生成される)
asm2bin.exe input.pt
```

## コードアーキテクチャ

### ファイル構成

- `asm2bin.cpp` — アセンブラ本体。引数処理、`.global` 解析、ラベル解決、命令変換、SV 出力
- `asm2bin.hpp` — 命令テーブル (`commands`) と引数種別 (`arg_t`) の定義
- `util.hpp` — 文字列補助関数(`str_find_first_of`, `ltrim`, `b2d`, `o2d`, `h2d`, `replace`)

### アセンブル処理の流れ

1. コマンドライン引数から `.pt` と出力 `.sv` パスを決定(`get_args`)
2. `read_global_line` で `.global` 行まで読み飛ばし、前置きの妥当性検証
3. `get_function_names` で関数名一覧を収集(アドレスは `npos` で初期化)
4. `assemble_body` で命令を 1 行ずつ変換。関数ラベル出現時にアドレスを確定(main を先頭 pc=0 から配置)、局所ラベル(`.` 始まり)は別表 `local_labels` に位置を記録。全関数の ret 有無も追跡
5. 宣言済み全関数の定義を確認(`npos` が残っていればエラー)
6. `resolve_labels` で局所ラベル参照を解決(jmp は絶対 index、F系は相対オフセット)。未定義ラベル参照はエラー
7. `apply_main_self_loop` で先頭から線形探索し、最初の `ret` を自己ループ `jmp` に置換
8. `join_instructions` で命令を連結し、`function_name2line_num` で関数参照を行番号に置換して出力

### call/ret/jmp の実装方針

`call`・`ret`・`jmp` は CPU の正式命令(J系)。アセンブラはそれぞれ機械語1命令に直接変換する。

- `call func` → `call(0, 33'h1_0000_0000 + <func の行番号>)`(呼び出し先 pc を即値で渡す)
- `jmp .L0` → `jmp(0, 33'h1_0000_0000 + <.L0 の絶対 index>)`(飛び先は局所ラベルのみ。`rs1=0` 固定)
- `ret` → `ret()`(引数なし)
- `call` と `jmp` は「`rs1=0` +即値ターゲット」という同じ特殊な出力形のため、`output_bin_line()` 内でまとめて特別に組み立てる(汎用経路には乗らない)。`ret` は汎用経路で `ret()` を生成する
- 戻り先アドレスの保存・SP の更新・戻り先レジスタの選択はすべて CPU(CALL/RET 命令)が行う
- `SP` レジスタ = `6'h10`(リセット時にハードウェアが初期化)、戻り先 PC レジスタ = `6'h11`〜`6'h1a`(最大呼び出し深さ = 10)

### main の自己ループ置換

main は CALL で呼ばれないため戻り先が無い。プログラムは pc=0 から実行されるため、先頭から線形に探索して**最初に現れる `ret`** が「main が CALL されずに到達する戻り先の無い ret」になる。アセンブラはこの ret を自分自身の pc への `jmp`(無限ループ)に自動置換する。ret の位置・個数は問わない(ソース上に ret が存在しさえすれば置換できる)。

### 関数名解決の仕組み

`convert_arg()` で関数名を `@func@` のように区切り文字 `@` で囲んで出力に残し、全命令の変換が終わった後で `function_name2line_num()` が `@func@` を実アドレス(行番号)に一括置換する。区切り文字で囲むことで、関数名が命令名や数値の部分文字列と一致しても誤置換が起きない。

### 局所ラベル解決の仕組み

`jmp`・F系の飛び先は局所ラベル(`.L0` など、`.` 始まり)のみで指定する。`assemble_body` で `.` 始まりのラベルを `local_labels` に位置(直後の命令の index)として記録する(関数とは別の名前空間。`.global` 宣言不要、プログラム全体で一意)。

参照側は `convert_arg()` で仮文字列(プレースホルダ)に変換しておき、全命令の変換後に `resolve_labels()` が実値へ置換する。

- `jmp`(絶対): `33'h1_0000_0000 + <<ABS:.L0>>` → ラベルの**絶対 index** に置換(`PC = imm`)
- F系(相対): `33'h1_0000_0000 + <<REL:.L0>>` → 「ラベルの index − その命令自身の pc」(**相対オフセット**、`PC += imm`)に置換
- 命令の vector 上のインデックスがそのまま pc なので、`resolve_labels` はループの index を自命令 pc として相対オフセットを計算する
- 後方ジャンプは相対オフセットが負になる。負値は 33bit 目の即値使用フラグを落とさないよう **32bit 2の補数の hex**(例 `32'hfffffffc`)で出力する(`offset2imm`)
- プレースホルダ(`<<ABS:`/`<<REL:`/`>>`)の文字列定数は `asm2bin.cpp` 冒頭で定義。置換されずに残った参照は未定義ラベルとしてエラー

### アセンブリ言語の文法

```asm
.global func1, func2, main   ; 使用する全関数を宣言(必須)

main:                        ; 最初に定義する関数は main でなければならない(pc=0 から配置)
    mov fh r2h r0 15         ; レジスタは rN プレフィックス
    add r34 r32 r33          ; 数値はデフォルト10進。末尾 b/o/h で2/8/16進
    eq r1 r2 .L0             ; F系の飛び先は局所ラベルのみ(相対オフセットに解決)
    jmp .L0                  ; jmp の飛び先も局所ラベルのみ(絶対 index に解決)
.L0:                         ; 局所ラベル(`.` 始まり、`.global` 宣言不要)
    call func1               ; CALL 命令(呼び出し先 pc を即値で渡す)
    ret                      ; RET 命令(引数なし)

func1:
    ret
```

- コメント: `;`
- `.global` より前は空行とコメント行のみ許可(それ以外はエラー)
- タブ文字は非対応(`.global` 行・命令行にタブがあるとエラー)
- 全関数に `ret` が1つ以上必須(位置・個数は問わない)
- `jmp`・F系(`eq`/`ne`/`lt`/`gt`/`elt`/`egt`)の飛び先は**局所ラベルのみ**(数値・レジスタ指定は不可)。`jmp` は絶対 index、F系は相対オフセットに解決される
- 局所ラベルは `.` 始まりで `<名前>:` を単独行に書く。`.global` 宣言不要、プログラム全体で一意、未定義参照はエラー
- `and`, `or`, `xor`, `not`, `nand` は出力時に `and_`, `or_`, `xor_`, `not_`, `nand_` へ変換される(SystemVerilog の予約語と衝突するため)
- `RAW_DATA` 引数は `33'h1_0000_0000 + 値` の形で出力(33bit 目がイミディエイト使用フラグ)

### 出力フォーマット

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

## CPU の命令フォーマット

- 命令幅: 64bit(上位 32bit が命令本体、下位 32bit が即値領域)
- 命令フォーマット: `[type:3 | func:6 | mask:4 | rs1:6 | rs2:6 | rd:6 | imm:33]`
- イミディエイトの最上位 1bit は「即値 32bit を使用するか」のフラグ
- レジスタアドレス: 6bit
- 命令種別: `N`(nop)/ `P`(演算)/ `S`(シフト)/ `A`(転送)/ `F`(比較分岐)/ `J`(ジャンプ)/ `M`(メモリ)/ `IO`(標準入出力)
- 汎用レジスタのほか、`SP`・`FLG`・`PC`・入出力用レジスタ・AXI Stream 接続の stdin/stdout 用レジスタがある

## 命令一覧

`asm2bin.hpp` の命令テーブル (`commands`) に登録されている命令。`jmp`・F系の飛び先引数は引数種別 `arg_t::LABEL`(局所ラベル)。

| カテゴリ | 命令 |
|---|---|
| N系(nop) | `nop` |
| P系(演算) | `and`, `or`, `xor`, `not`, `nand`, `add`, `sub`, `mul`, `div` |
| S系(シフト) | `sll`, `srl`, `sla`, `sra` |
| A系(転送) | `mov` |
| F系(比較分岐) | `eq`, `ne`, `lt`, `gt`, `elt`, `egt` |
| J系(ジャンプ) | `jmp`, `call`, `ret` |
| M系(メモリ) | `rm`, `wm`, `brm`, `bwm` |
| IO系 | `scan`, `print` |

## テストケース内容

`test/asm/`(`01.pt`〜`13.pt`)にある正常系テストケースを`test/test.py`で一括実行できる。call/retの多段・再帰呼び出し，P/S/F/M/IO各系命令，局所ラベルの前方/後方ジャンプなど，本CLAUDE.mdに記載の仕様を一通り網羅する。各ファイルの内容はソース(コメント)を参照。

`test/asm_err/`(`13.pt`〜`23.pt`)にある異常系テストケースを`test/test_err.py`で一括実行できる(エラーが出ることを確認する)。未定義関数・main欠如・タブ文字・引数過多・未定義ラベル等，「アセンブリ言語の文法」節に記載の各種制約違反を網羅する。各ファイルの内容はソース(コメント)を参照。

## Issue対応の徹底

ファイルを修正する場合は，必ず対応するGitHub issueを起票し，そのissue用のブランチ(`fix/issue-<番号>-<内容を表す短い語句>`)を作成してから行う．デフォルトブランチを直接編集しない．

**例外:** `CLAUDE.md`や`.claude/skills/`配下のスキル定義ファイルの修正は，ソースコードの変更ではないためissue起票は不要．ただしブランチ作成は必要(デフォルトブランチを直接編集しない)．作業中の既存ブランチがあれば，新たにブランチを切らずそれに乗せてよい．

## 次の作業候補

- テストケースをさらに拡充する(数値表記の妥当性・メモリ命令の境界値など)