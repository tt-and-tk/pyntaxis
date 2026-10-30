#pragma once

// アセンブラの処理のうち，コンパイラのPynesisソースからの一貫変換(pn2mc.exe)で使う関数

#include <string>

// アセンブリをSystemVerilog ROMまたは実行ファイルに変換する本処理．mainと同じ引数(argc, argv)を受け取る
// 処理に成功したら0，失敗したら1を返す
int assemble_asm_to_mc(int argc, char **argv);

// Qosmosの実行ファイル名として使えるかを返す．ディレクトリを除いたファイル名の部分を受け取る
// 基本名だけの1〜8文字で，FATの短い名前に使えない文字を含まないものに限る
bool is_executable_name(const std::string &name);
