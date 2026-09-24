# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## このリポジトリについて

GitHubリポジトリ名: `tt-and-tk/pyntaxis`．

PYNQ-Z2 (Zynq-7000) 上に実装する自作CPUと，それを動かすソフトウェア群(アセンブラ・コンパイラ・OS)からなる自作PCプロジェクトの一部．プロジェクト全体は以下の独立したGitHubリポジトリで構成される．

| リポジトリ(GitHub) | ディレクトリ(`pc/`配下) | 役割 |
|:-|:-|:-|
| `specification` | `specification/` | CPUアーキテクチャ・ISA・アセンブリ言語・コンパイラ・Qosmosの仕様のドキュメント(唯一の一次情報源) |
| `pyntaxis`(本リポジトリ) | `assembler/` | 自作アセンブリ言語Pyntaxis(`.pt`) → SystemVerilog ROM(`.sv`)へのアセンブラ |
| `pynesis` | `compiler/` | 自作プログラミング言語Pynesis(`.pn`) → アセンブリ言語Pyntaxisへのコンパイラ．本リポジトリのソースファイルをincludeして使用し，`.sv`まで一貫変換も可能 |
| `qurge` | `mypc/` | CPU・メモリ・ROM等のハードウェア全体のVivadoプロジェクト(SystemVerilog + PS側C++)と，ROM上で動く自作OS Qosmos(シェルやファイルシステムなど．Pynesisで記述．仕様は`specification`の`qosmos.md`) |
| `for-pynthesis-skills` | `for-pynthesis-skills/` | 上記各リポジトリで共有するissue起票・対応支援スキルを提供する．特定のリポジトリが主担当と判断できない，全リポジトリに影響するissueの起票先(受け皿)でもある |

```
入力(.pn) → [pynesisのコンパイラ] → アセンブリ(.pt) → [pyntaxis(本リポジトリ)のアセンブラ] → SystemVerilog ROM(.sv) → [Vivado] → PYNQ-Z2上のハードウェア(qurge)
```

## ビルドとテスト

**ビルド:**
```
g++ -o asm2sv.exe asm2sv.cpp
```

**テスト(`test/` ディレクトリで実行):**
```
cd test
python test.py        # 正常系: test/asm/*.pt を全て変換し test/sv/ へ出力
python test_err.py    # 異常系: test/asm_err/*.pt が全てエラーになることを確認
python test_bin.py    # 実行ファイル: test/asm_bin/*.pt を test/bin/ へ出力し，test/asm_bin_err/*.pt と引数の誤りがエラーになることを確認
```
期待値は `test/sv_ans/`・`test/bin_ans/`(実行ファイルと同じ形式のバイナリ) にある。

**単体実行:**
```
asm2sv.exe input.pt -sv output.sv
rem または(-sv 省略時は input.sv が生成される)
asm2sv.exe input.pt
rem 実行ファイルを出力する場合(拡張子の無い8文字以内の名前を指定する．-sv とは同時に指定できない)
asm2sv.exe input.pt -bin OUTPUT
```

## Issue対応の徹底

ファイルを修正する場合は，必ず対応するGitHub issueを起票し，そのissue用のブランチ(`fix/issue-<番号>-<内容を表す短い語句>`)を作成してから行う．デフォルトブランチを直接編集しない．

## 次の作業候補

- テストケースをさらに拡充する(数値表記の妥当性・メモリ命令の境界値など)