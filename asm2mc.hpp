#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// 命令に与えられる引数の種類
enum class arg_t {
    REGISTER,    // レジスタの番地
    RAW_DATA,    // イミディエイトデータ (イミディエイトデータの使用フラグも含む)
    FUNC_NAME,   // 関数名
    LABEL,       // 局所ラベル名 (jmpは絶対PC，F系は相対オフセットに解決される)
    MASK,        // ビットマスク
    ZERO,        // アセンブリ上の引数を取らず，機械語の引数を0で埋める
};

// 機械語の引数を格納するフィールド
enum class field_t {
    MASK,        // マスク(4bit)
    RS1,         // 演算に使用するレジスタの番地ひとつめ(6bit)
    RS2,         // 演算に使用するレジスタの番地ふたつめ(6bit)
    RD,          // 演算結果を出力するレジスタの番地(6bit)
    IMM,         // 即値使用フラグ(1bit)とイミディエイトデータ(32bit)
};

// 一つの引数形式
// 機械語の引数の並びを表し，ZERO以外の要素がアセンブリに書かれた引数を順に受け取る
typedef std::vector<arg_t> command_form_t;

// 一つの命令の定義
typedef struct {
    unsigned int type;                    // 命令タイプ(m_type)
    unsigned int func;                    // 機械語の種類(func)
    std::vector<field_t> fields;          // 機械語の引数を格納するフィールド(machine.svhの関数の引数順)
    std::vector<command_form_t> forms;    // 取りうる引数形式(いずれもfieldsと同じ個数の機械語の引数を持つ)
} command_t;

// 機械語の命令一覧(命令ごとに，m_type・func・引数の格納先・取りうる引数形式を並べる)
// 同じ命令の形式どうしは引数の個数か書き方で区別でき，両方に合う書き方は存在しない
const std::map<std::string, command_t> commands = {
    // 処理を実行しない(N系)
    {"nop"  , {0, 0x00, {},
               {{                                                                              }}}},

    // 演算系(P系)
    {"and"  , {1, 0x00, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"or"   , {1, 0x01, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"xor"  , {1, 0x02, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"not"  , {1, 0x03, {field_t::RS1, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER                                              }}}},
    {"nand" , {1, 0x04, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"add"  , {1, 0x05, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"sub"  , {1, 0x06, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    {"mul"  , {1, 0x07, {field_t::RS1, field_t::RS2, field_t::RD},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}}},
    // 商のみ必要なら即値を省略し，余りも必要ならその格納先のレジスタ番地を即値で指定する
    {"div"  , {1, 0x08, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"divu" , {1, 0x09, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},

    // シフト系(S系)
    // シフト量はrs2で指定するか，即値で指定する
    {"sll"  , {2, 0x00, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"srl"  , {2, 0x01, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"sla"  , {2, 0x02, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"sra"  , {2, 0x03, {field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},

    // 代入系(A系)
    // MASKはCPU側で未実装(値に関わらず動作は変わらず，常に全バイトへ書き込まれる)
    {"mov"  , {3, 0x00, {field_t::MASK, field_t::RS1, field_t::RD, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},

    // 分岐系(F系)
    // 飛び先は局所ラベルのみ(相対オフセットに解決される)
    {"eq"   , {4, 0x00, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"ne"   , {4, 0x01, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"lt"   , {4, 0x02, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"gt"   , {4, 0x03, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"elt"  , {4, 0x04, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"egt"  , {4, 0x05, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"ltu"  , {4, 0x06, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"gtu"  , {4, 0x07, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"eltu" , {4, 0x08, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},
    {"egtu" , {4, 0x09, {field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}}},

    // ジャンプ系(J系)
    // jmpの飛び先は局所ラベルのみ(絶対PCに解決される)．レジスタ・数値による飛び先指定は持たない
    {"jmp"  , {5, 0x00, {field_t::RS1, field_t::IMM},
               {{arg_t::ZERO    , arg_t::LABEL                                                 }}}},
    // callの呼び出し先は，関数名なら先頭PCを即値で渡し，レジスタならその値をそのままPCとする
    {"call" , {5, 0x01, {field_t::RS1, field_t::IMM},
               {{arg_t::ZERO    , arg_t::FUNC_NAME                                             },
                {arg_t::REGISTER, arg_t::ZERO                                                  }}}},
    {"ret"  , {5, 0x02, {},
               {{                                                                              }}}},

    // メモリ系(M系)
    // rm/wmは番地をrs1で指定するか，即値で指定する
    {"rm"   , {6, 0x00, {field_t::MASK, field_t::RS1, field_t::RD, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"wm"   , {6, 0x01, {field_t::MASK, field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
                {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"brm"  , {6, 0x02, {field_t::MASK, field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO    },
                {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}}}},
    {"bwm"  , {6, 0x03, {field_t::MASK, field_t::RS1, field_t::RS2, field_t::RD, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO    },
                {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}}}},
    // RMR/WMRはrs1とimmを足した番地を読み書きするため，即値を省略した形を持たない
    {"rmr"  , {6, 0x04, {field_t::MASK, field_t::RS1, field_t::RD, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},
    {"wmr"  , {6, 0x05, {field_t::MASK, field_t::RS1, field_t::RS2, field_t::IMM},
               {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}}},

    // 標準入出力系(IO系)
    // printは出力する値をrs1で指定するか，即値で指定する
    {"scan" , {7, 0x00, {field_t::RD},
               {{arg_t::REGISTER                                                               }}}},
    {"print", {7, 0x01, {field_t::RS1, field_t::IMM},
               {{arg_t::REGISTER, arg_t::ZERO                                                  },
                {arg_t::REGISTER, arg_t::RAW_DATA                                              }}}},
};

// 機械語の引数を後から解決する参照の種類
enum class ref_t {
    NONE,        // 参照を持たない(値が確定している)
    FUNCTION,    // 関数の先頭PC
    LABEL_ABS,   // 局所ラベルのPC(jmpの飛び先)
    LABEL_REL,   // 局所ラベルまでの相対オフセット(F系の飛び先)
};

// 機械語の引数一つ
typedef struct {
    std::string sv;               // SystemVerilog上の表記(参照は解決時に埋める)
    std::uint64_t value;          // 値(immは即値使用フラグを含む．参照は解決時に埋める)
    ref_t ref;                    // 後から解決する参照の種類
    std::string name;             // 参照先の関数名・局所ラベル名
} operand_t;

// アセンブルした命令一つ
typedef struct {
    std::string command;              // ニーモニック
    std::vector<operand_t> operands;  // 機械語の引数(machine.svhの関数の引数順)
    std::string function;             // 命令が属する関数名
} instruction_t;
