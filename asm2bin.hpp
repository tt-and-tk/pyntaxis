#pragma once

#include <map>
#include <string>

// 各命令の引数情報
typedef struct {
    int arg_num_min;    // とりうる引数の最小の個数(イミディエイトデータがある場合は引数の数はこれプラス1になる)
    bool imm_required;  // イミディエイトデータが必須かどうか(必須な場合，引数の個数はarg_num_minから変動しない)
} command_arg_t;

// 機械語一覧
const std::map<std::string, command_arg_t> commands = {
    // 演算系(P系)
    {"and"  ,  {3, false}},
    {"or"   ,  {3, false}},
    {"xor"  ,  {3, false}},
    {"not"  ,  {2, false}},
    {"nand" ,  {3, false}},
    {"add"  ,  {3, false}},
    {"sub"  ,  {3, false}},
    {"addi" ,  {3, false}},
    {"subi" ,  {3, false}},

    // シフト系(S系)
    {"sll"  ,  {3, false}},
    {"srl"  ,  {3, false}},
    {"sla"  ,  {3, false}},
    {"sra"  ,  {3, false}},

    // 代入系(A系)
    {"mov"  ,  {3, false}},

    // 分岐系(F系)
    {"eq"   ,  {1, true }},
    {"ne"   ,  {1, true }},
    {"lt"   ,  {1, true }},
    {"gt"   ,  {1, true }},
    {"elt"  ,  {1, true }},
    {"egt"  ,  {1, true }},

    // ジャンプ系(J系)
    {"jmp"  ,  {1, false}},

    // メモリ系(M系)
    {"rm"   ,  {3, false}},
    {"wm"   ,  {3, false}},
    {"brm"  ,  {3, false}},
    {"bwm"  ,  {4, false}},
};
