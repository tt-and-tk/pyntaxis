# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

PYNQ-Z2 上で動かす自作 CPU 向けのアセンブラ。`.asm` ファイルを読み込み、`machine.svh` の `machine_p` パッケージを使った SystemVerilog ROM 初期化コード (`.sv`) を出力する。

## ビルドとテスト

**ビルド:**
```
g++ -o asm2bin.exe asm2bin.cpp
```

**テスト（`test/` ディレクトリで実行）:**
```
cd test && test.bat
```
`test/asm/*.asm` を全て変換し `test/bin/` へ出力する。期待値は `test/bin_ans/` にある（全ファイル分はない）。

**単体実行:**
```
asm2bin.exe input.asm -o output.sv
rem または（-o 省略時は input.sv が生成される）
asm2bin.exe input.asm
```

## コードアーキテクチャ

### ファイル構成

- `asm2bin.cpp` — アセンブラ本体。引数処理、`.global` 解析、ラベル解決、命令変換、SV 出力
- `asm2bin.hpp` — 命令テーブル (`commands`) と引数種別 (`arg_t`) の定義
- `util.hpp` — 文字列補助関数（`str_find_first_of`, `b2d`, `o2d`, `h2d`, `replace`）

### アセンブル処理の流れ

1. コマンドライン引数から `.asm` と出力 `.sv` パスを決定
2. `.global` 宣言を読み取り、関数名一覧を収集（アドレスは `npos` で初期化）
3. 命令を 1 行ずつ変換しながら関数ラベルが出現したらアドレスを確定
4. call グラフを構築し、再帰・呼び出し深さオーバーを静的検査
5. ROM 末尾に call/ret 共通ディスパッチャを 1 回だけ出力
6. 関数名文字列を実行アドレス (行番号) に一括置換して `.sv` へ書き出す

### call/ret の実装方針

`call` と `ret` は CPU の正式命令ではなくアセンブラ疑似命令。各 `call` 呼び出し箇所では小さなコード片（3 命令）に展開し、実際の SP 判定と戻り先レジスタへの保存は ROM 末尾の共通ディスパッチャで行う。

- `SP` レジスタ = `6'h10`（初期値はブートストラップ命令で設定）
- 戻り先 PC レジスタ = `6'h11`〜`6'h1a`（最大呼び出し深さ = 10）
- 再帰呼び出しは禁止（コンパイルエラー）
- 呼び出し深さが戻り先レジスタ数を超えた場合もコンパイルエラー

### アセンブリ言語の文法

```asm
.global func1, func2, main   ; 使用する全関数を宣言（必須）

main:                        ; 最初に定義する関数は main でなければならない
    mov fh r2h r0 15         ; レジスタは rN プレフィックス
    add r34 r32 r33          ; 数値はデフォルト10進。末尾 b/o/h で2/8/16進
    call func1               ; 疑似命令（jmp に展開）
    ret                      ; 疑似命令（共通 ret ディスパッチャへ jmp）

func1:
    ret
```

- コメント: `;`
- `and`, `or`, `xor`, `not`, `nand` は出力時に `and_`, `or_`, `xor_`, `not_`, `nand_` へ変換される（`machine.svh` 側の命名に合わせるため）
- `RAW_DATA` 引数は `33'h1_0000_0000 + 値` の形で出力（33bit 目がイミディエイト使用フラグ）

### 出力フォーマット

```systemverilog
`include "machine.svh"
module rom(
    input  logic clk, resetn,
    input  logic [31:0] pc,
    output logic [31:0] machine,
    output logic [31:0] imm
    );
    logic [63:0] machines[0:255] = { ... };
    always_comb begin ... end
endmodule
```

命令は 64bit。上位 32bit が命令本体、下位 32bit が即値。最大命令数は 255。

## 関連する外部ファイル（このリポジトリ外）

- `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\CLAUDE.md` — CPU 全体概要
- `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\sources_1\new\machine.svh` — 命令定義
- `C:\D\program\xilinx\pynq-z2\pc\specification\isa.md` — ISA 仕様
- `C:\D\program\xilinx\pynq-z2\pc\specification\assembler.md` — アセンブラ文法仕様

## 既知の未完成事項（AGENTS.md より）

- `wm` の引数仕様がアセンブラ定義と CPU 側 `machine.svh` で不一致
- `mul`, `div`, `scan`, `print` の動作が未検証
- テストケースが少なく、異常系・メモリ命令の妥当性が未検証
