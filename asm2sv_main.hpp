#pragma once

// アセンブリをSystemVerilog ROMに変換する本処理．mainと同じ引数(argc, argv)を受け取る
// 処理に成功したら0，失敗したら1を返す
int assemble_asm_to_sv(int argc, char **argv);
