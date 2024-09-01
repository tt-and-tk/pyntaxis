#pragma once

#include <map>
#include <string>
#include <vector>

// 引数の種類
enum class arg_t {
    REGISTER,
    RAW_DATA,
    FUNC_NAME,
    MASK,
};

// 各命令の引数情報
typedef struct {
    const int arg_num_min;    // とりうる引数の最小の個数(イミディエイトデータがある場合は引数の数はこれプラス1になる)
    const std::vector<arg_t> args;  // 取る引数の数
    const bool imm_required;  // イミディエイトデータが必須かどうか(必須な場合，引数の個数はarg_num_minから変動しない)
} command_arg_t;

// 機械語一覧
const std::map<std::string, command_arg_t> commands = {
    // 演算系(P系)
    {"and"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"or"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"xor"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"not"  ,  {2, {arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA                 }, false}},
    {"nand" ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"add"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"sub"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"addi" ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"subi" ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},

    // シフト系(S系)
    {"sll"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"srl"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"sla"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"sra"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},

    // 代入系(A系)
    {"mov"  ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},

    // 分岐系(F系)
    {"eq"   ,  {1, {arg_t::RAW_DATA                                                   }, true }},
    {"ne"   ,  {1, {arg_t::RAW_DATA                                                   }, true }},
    {"lt"   ,  {1, {arg_t::RAW_DATA                                                   }, true }},
    {"gt"   ,  {1, {arg_t::RAW_DATA                                                   }, true }},
    {"elt"  ,  {1, {arg_t::RAW_DATA                                                   }, true }},
    {"egt"  ,  {1, {arg_t::RAW_DATA                                                   }, true }},

    // ジャンプ系(J系)
    {"jmp"  ,  {1, {arg_t::REGISTER, arg_t::RAW_DATA                                  }, false}},

    // メモリ系(M系)
    {"rm"   ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"wm"   ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"brm"  ,  {4, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
    {"bwm"  ,  {4, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false}},
};
