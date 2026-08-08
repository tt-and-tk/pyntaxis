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

## 詳細ドキュメント

言語仕様・命令フォーマット等は`../specification/index.md`を参照(唯一の一次情報源．このリポジトリ内には転記しない)。アセンブラ内部の実装・テストケースの内容はソースファイル自体を参照。

## Issue対応の徹底

ファイルを修正する場合は，必ず対応するGitHub issueを起票し，そのissue用のブランチ(`fix/issue-<番号>-<内容を表す短い語句>`)を作成してから行う．デフォルトブランチを直接編集しない．

**例外:** `CLAUDE.md`や`.claude/skills/`配下のスキル定義ファイルの修正は，ソースコードの変更ではないためissue起票は不要．ただしブランチ作成は必要(デフォルトブランチを直接編集しない)．作業中の既存ブランチがあれば，新たにブランチを切らずそれに乗せてよい．

## 次の作業候補

- テストケースをさらに拡充する(数値表記の妥当性・メモリ命令の境界値など)