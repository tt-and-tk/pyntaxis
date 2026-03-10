# assembler

## 当面の目標
`call`命令と`ret`命令を完成させること．

## このプロジェクトの目的

このプロジェクトは、自作CPU向けのアセンブリ言語を機械語へ変換し、最終的に System Verilog で記述された ROM 初期化コードとして出力するコンパイラを作るためのものです。

入力は `.asm`、出力は `machine.svh` の `machine_p` パッケージを使う `.sv` ファイルです。生成物は CPU 側の `rom_sv.sv` のような命令 ROM と同じ形式を想定しています。

## CPU に関する簡単な仕様

- 対象 CPU は PYNQ-Z2 上で動かす自作 CPU。
- 命令は 64bit 幅で、上位 32bit が命令本体、下位 32bit が即値領域。
- 命令フォーマットは `[type:3 | func:6 | mask:4 | rs1:6 | rs2:6 | rd:6 | imm:33]`。イミディエイトデータの上位1bitはイミディエイトデータ32bitを使用するかどうかのフラグ．
- レジスタアドレスは 6bit。
- 汎用レジスタのほか、`SP`、`FLG`、`PC`、入出力用レジスタ、AXI Stream 接続の stdin/stdout 用レジスタがある。
- 命令種別は `N/P/S/A/F/J/M/IO` に分かれる。

主な外部仕様の保管場所:

- CPU 全体概要: `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\CLAUDE.md`
- 命令定義: `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\sources_1\new\machine.svh`
- 仕様書: `C:\D\program\xilinx\pynq-z2\pc\specification\*.md`

## コンパイラの概要

現状のコンパイラは C++ 製の単機能アセンブラで、次の流れで動きます。

1. コマンドライン引数から入力 `.asm` と出力 `.sv` を決定する。
2. ソース中の `.global` 宣言を読み、関数名一覧を集める。
3. 各関数ラベルの行番号を収集する。
4. 命令行を `machine::xxx(...)` 形式の System Verilog コードへ変換する。
5. 関数名参照を最終的に行番号へ置換する。
6. `module rom` 形式の `.sv` を出力する。

`call` と `ret` は CPU の正式命令ではなく、アセンブラが `jmp` と復帰先アドレス管理へ展開する疑似命令として扱います．

## 命令セットアーキテクチャの概要

### 実装済みとして扱われている命令

アセンブラの命令テーブルに登録されているのは次の命令です。

- `nop`
- 論理演算/算術: `and`, `or`, `xor`, `not`, `nand`, `add`, `sub`, `mul`, `div`
- シフト: `sll`, `srl`, `sla`, `sra`
- 転送: `mov`
- 比較分岐: `eq`, `ne`, `lt`, `gt`, `elt`, `egt`
- ジャンプ: `jmp`
- メモリアクセス: `rm`, `wm`, `brm`, `bwm`

加えて、アセンブラ内部で特別扱いされる疑似命令があります。

- `call`
- `ret`

ただし注意点として， `and`, `or`, `xor`, `not`, `nand` は入力ニーモニックと `machine.svh` 側の関数名が一致しないため、出力時に `and_`, `or_`, `xor_`, `not_`, `nand_` へ名前変換する必要がある。

### 即値と引数の扱い

- レジスタは `r10` のように `r` プレフィックス付きで記述する実装。
- 数値は基本 10 進で、末尾に `b`, `o`, `h` を付けると 2/8/16 進数としてそのまま Verilog リテラルへ変換する実装。
- `RAW_DATA` 扱いの引数は `33'h1_0000_0000 + ...` の形で出力しており、即値使用フラグを 33bit の最上位ビットで表す前提。

## 今の進捗

### できていること

- `.asm` から `.sv` を生成する最小限のアセンブラ本体がある。
- `.global` 宣言の読取りと関数名管理がある。
- 関数ラベルを命令アドレスへ解決できる。
- `main` 関数必須、`main` が先頭関数であること、関数重複定義禁止などの構文チェックがある。
- `call` / `ret` を `jmp` に落として関数呼び出し風の制御を表現できる。
- 簡易テストがある。
- 出力は `machine.svh` を include する `module rom` 形式で生成できる。

### まだ未完成または不整合なこと

- `output_call_function()` と `output_ret_function()` は空のまま。
- `wm` の引数仕様が CPU 側 `machine.svh` とアセンブラ定義で一致していない。CPU 側は `wm(mask, rs1, rs2, imm)` だが、アセンブラ側は第3引数をレジスタとして取り、そのまま `rd` 的に扱っている。
- `assembler.md` に書かれている文法例と、実装の引数順・命令名が一部一致していない。
- `rom_sv.sv` のような実運用 ROM モジュール形式とは I/F が異なる。現状出力は `pc`, `machine`, `imm` を直接持つ単純な `module rom`。
- テストが少なく、未対応命令や異常系、数値表記、メモリ命令の妥当性が未検証。

### 現時点の評価

「アセンブラの土台と、関数ラベル解決付きの最小構成 ROM 生成器まではできているが、ISA 全体を正しくカバーする段階にはまだ達していない」という進捗です。

## プロジェクト内ファイルの説明

### ルート

- `AGENTS.md`
  - このプロジェクトの現状把握メモ。
- `asm2bin.cpp`
  - アセンブラ本体。引数処理、`.global` 解析、ラベル解決、命令変換、SystemVerilog 出力を持つ。
- `asm2bin.hpp`
  - 命令テーブルと引数種別定義。アセンブラが受け付ける命令の一覧はここにある。
- `util.hpp`
  - 文字列置換や補助関数。
- `asm2bin.exe`
  - ビルド済み実行ファイル。

### `.vscode`

- `.vscode/settings.json`
  - エディタ設定。

### `test`

- `test/test.py`
  - `test/asm/*.asm` をまとめて `asm2bin.exe` に通す簡易テストスクリプト。
- `test/test.bat`
  - Windows 用テスト起動バッチ。実体は `python test.py` 呼び出し。
- `test/asm/`
  - テスト用アセンブリ入力。
- `test/bin/`
  - 現行アセンブラが出力した `.sv`。
- `test/bin_ans/`
  - 過去の想定出力の一部。現在の `test/bin` と完全一致している保証はない。

### テスト内容の傾向

- `01.asm`
  - `mov`, `add`, `ret` の基本ケース。
- `02.asm`
  - `call`, `ret` の基本関数呼び出し。
- `03.asm`
  - 多段 `call`。
- `04.asm`
  - 関数定義順が `.global` 順と異なるケース。

## 関連する外部ファイル

このリポジトリ外だが、実装確認に重要だったファイル:

- `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\CLAUDE.md`
- `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\sources_1\new\machine.svh`
- `C:\D\program\xilinx\pynq-z2\pc\mypc\mypc.srcs\sources_1\new\rom_sv.sv`
- `C:\D\program\xilinx\pynq-z2\pc\specification\assembler.md`
- `C:\D\program\xilinx\pynq-z2\pc\specification\isa.md`
- `C:\D\program\xilinx\pynq-z2\pc\specification\register.md`
- `C:\D\program\xilinx\pynq-z2\pc\specification\memory.md`
- `C:\D\program\xilinx\pynq-z2\pc\specification\rom.md`

## 次に着手するなら

- `machine.svh` と整合する命令名へ出力を合わせる。
- `mul`, `div`, `scan`, `print` を追加する。
- `addi` / `subi` を ISA 上どう表現するか再確認する。
- `wm` を含むメモリ命令の引数順を仕様に合わせて修正する。
- `rom_sv.sv` と接続できる出力形式へ寄せるか、現行の `module rom` を開発用フォーマットとして明記する。
- 数値変換や Verilog リテラル表現について、既存の `test/bin_ans` を正解として固定せず、必要なら期待値側を見直す。

## 実装メモ

- `convert_arg()` で関数名を一旦文字列のまま残し、最後に `function_name2line_num()` で実アドレスへ置換する設計自体は想定どおり。
- 問題は `call` 展開時だけ、未解決の関数名引数を `atoi()` で数値化して復帰先スタックに積んでいる点にある。
- したがって修正対象は `convert_arg()` や `function_name2line_num()` の後置換方針そのものではなく、`call` / `ret` の復帰先管理ロジックである。
- `jmp` は通常命令としてはレジスタまたは即値を取り得るが、`call` / `ret` 展開時はイミディエイトデータのみを使う想定。
- `call` / `ret` は各呼び出し箇所で大きく展開せず、ROM 末尾に共通ディスパッチャを 1 回だけ生成する方針へ変更した。
- `call` は「呼び出し先 pc」と「復帰先 pc」を一時レジスタへ保存してから共通 `call` ディスパッチャへ飛ぶ。
- 共通 `call` ディスパッチャは `SP(6'h10)` を見て、`6'h11`～`6'h1a` のどの戻り先レジスタへ復帰先 pc を保存するかを決める。
- `ret` は共通 `ret` ディスパッチャへ飛び、`SP` に応じた戻り先レジスタへジャンプする。
- `SP` の初期値はアセンブラが生成するブートストラップ命令で `6'h10` に初期化する。
- 再帰呼び出しは未対応とし、call グラフに循環がある場合はコンパイルエラーにする。
- 静的な呼び出し深さが戻り先レジスタ本数を超える場合もコンパイルエラーにする。
