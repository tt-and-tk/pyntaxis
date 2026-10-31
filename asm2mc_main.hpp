#pragma once

// アセンブラの処理のうち，コンパイラのPynesisソースからの一貫変換(pn2mc.exe)で使う関数

#include <string>

// アセンブリをSystemVerilog ROMまたは実行ファイルに変換する本処理．mainと同じ引数(argc, argv)を受け取る
// 処理に成功したら0，失敗したら1を返す
int assemble_asm_to_mc(int argc, char **argv);

// 出力先のファイル名(ディレクトリを含んでよい)が，Qosmosの実行ファイル名として使えるかを返す
// ディレクトリを除いた名前が，基本名だけの1〜8文字で，FATの短い名前に使える文字だけから成るものに限る
bool is_executable_name(const std::string &file_name);
