#pragma once

#include <map>
#include <string>
#include <vector>

// 命令に与えられる引数の種類
enum class arg_t {
    REGISTER,    // レジスタの番地
    RAW_DATA,    // イミディエイトデータ (イミディエイトデータの使用フラグも含む)
    FUNC_NAME,   // 関数名
    LABEL,       // 局所ラベル名 (jmpは絶対index，F系は相対オフセットに解決される)
    MASK,        // ビットマスク
    ZERO,        // アセンブリ上の引数を取らず，機械語の引数を0で埋める
};

// 一つの引数形式
// 機械語の引数の並びを表し，ZERO以外の要素がアセンブリに書かれた引数を順に受け取る
typedef std::vector<arg_t> command_form_t;

// 機械語の命令一覧(命令ごとに，取りうる引数形式を並べる)
// 同じ命令の形式どうしは引数の個数か書き方で区別でき，両方に合う書き方は存在しない
const std::map<std::string, std::vector<command_form_t>> commands = {
    // 処理を実行しない(N系)
    {"nop"  , {{                                                                              }}},

    // 演算系(P系)
    {"and"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"or"   , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"xor"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"not"  , {{arg_t::REGISTER, arg_t::REGISTER                                              }}},
    {"nand" , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"add"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"sub"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    {"mul"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                             }}},
    // 商のみ必要なら即値を省略し，余りも必要ならその格納先のレジスタ番地を即値で指定する
    {"div"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"divu" , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},

    // シフト系(S系)
    // シフト量はrs2で指定するか，即値で指定する
    {"sll"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"srl"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"sla"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"sra"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},

    // 代入系(A系)
    // MASKはCPU側で未実装(値に関わらず動作は変わらず，常に全バイトへ書き込まれる)
    {"mov"  , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},

    // 分岐系(F系)
    // 飛び先は局所ラベルのみ(相対オフセットに解決される)
    {"eq"   , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"ne"   , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"lt"   , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"gt"   , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"elt"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"egt"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"ltu"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"gtu"  , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"eltu" , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},
    {"egtu" , {{arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                                }}},

    // ジャンプ系(J系)
    // jmpの飛び先は局所ラベルのみ(絶対indexに解決される)．レジスタ・数値による飛び先指定は持たない
    {"jmp"  , {{arg_t::ZERO    , arg_t::LABEL                                                 }}},
    // callの呼び出し先は，関数名なら先頭indexを即値で渡し，レジスタならその値をそのままPCとする
    {"call" , {{arg_t::ZERO    , arg_t::FUNC_NAME                                             },
               {arg_t::REGISTER, arg_t::ZERO                                                  }}},
    {"ret"  , {{                                                                              }}},

    // メモリ系(M系)
    // rm/wmは番地をrs1で指定するか，即値で指定する
    {"rm"   , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"wm"   , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO                },
               {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"brm"  , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO    },
               {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}}},
    {"bwm"  , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::ZERO    },
               {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}}},
    // RMR/WMRはrs1とimmを足した番地を読み書きするため，即値を省略した形を持たない
    {"rmr"  , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},
    {"wmr"  , {{arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA            }}},

    // 標準入出力系(IO系)
    // printは出力する値をrs1で指定するか，即値で指定する
    {"scan" , {{arg_t::REGISTER                                                               }}},
    {"print", {{arg_t::REGISTER, arg_t::ZERO                                                  },
               {arg_t::REGISTER, arg_t::RAW_DATA                                              }}},
};

// 引数タイプごとのビット数を返す
// 数値表記を書ける引数のみが対象(関数名・局所ラベルは表を引いて解決するため数値の桁数を持たない)
std::string get_bit_length_of_command(const arg_t arg) {
    switch (arg) {
        case arg_t::REGISTER:
            return std::to_string(6);

        case arg_t::RAW_DATA:
            return std::to_string(32);

        case arg_t::LABEL:
            return std::to_string(32);

        case arg_t::MASK:
            return std::to_string(4);

        default:
            // 起きないはずのエラーなのでエラーメッセージは適当
            throw std::string("asm syntax error: arg type is fail");
            return "";
    }
}
