# pyntaxis

このリポジトリは，自作アセンブリ言語Pyntaxis(`.pt`)のアセンブラのソースを置く．アセンブラは，Pyntaxisで書いたプログラムをSystemVerilogのROM(`.sv`)，または自作OS Qosmosの実行ファイルへ変換する．

このリポジトリは，PYNQ-Z2(Zynq-7000)上に実装する自作CPUと，それを動かすソフトウェア群(アセンブラ・コンパイラ・OS)からなる自作パソコンのプロジェクトPynthesisの一部である．

## 文書

文書は，仕様書のリポジトリ`specification`に置く．このリポジトリにかかわる文書は次のとおり．

- [README.md](https://github.com/tt-and-tk/specification/blob/master/README.md): 仕様書全体の構成
- [terms.md](https://github.com/tt-and-tk/specification/blob/master/terms.md): Pynthesisの構成要素の名称と，Pynthesis全体で共通に使う用語
- [pyntaxis/](https://github.com/tt-and-tk/specification/tree/master/pyntaxis): Pyntaxisの言語仕様，アセンブラの使い方，アセンブラの開発の手順
