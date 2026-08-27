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
};

// 各命令の引数情報
typedef struct {
    const int arg_num_min;    // とりうる引数の最小の個数(イミディエイトデータがある場合は引数の数はこれプラス1になる)
    const std::vector<arg_t> arg_types;  // それぞれの引数の種類
    const bool imm_required;  // 機械語側でイミディエイトデータが必須かどうか(必須な場合，引数の個数はarg_num_minから変動しない)
    const bool has_imm;       // machine.svh関数のimmパラメータを持つか(falseの場合，immは出力しない)
} command_arg_t;

// 機械語の命令一覧
const std::map<std::string, command_arg_t> commands = {
    // 処理を実行しない(N系)
    {"nop"  ,  {0, {                                                                  }, false, false}},

    // 演算系(P系)
    {"and"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"or"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"xor"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"not"  ,  {2, {arg_t::REGISTER, arg_t::REGISTER,                                 }, false, false}},
    {"nand" ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"add"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"sub"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"mul"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER                 }, false, false}},
    {"div"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},

    // シフト系(S系)
    {"sll"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"srl"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"sla"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"sra"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},

    // 代入系(A系)
    {"mov"  ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},  // MASKはCPU側で未実装(値に関わらず動作は変わらず，常に全バイトへ書き込まれる)

    // 分岐系(F系)
    // 飛び先は局所ラベルのみ(相対オフセットに解決される)
    {"eq"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},
    {"ne"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},
    {"lt"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},
    {"gt"   ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},
    {"elt"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},
    {"egt"  ,  {3, {arg_t::REGISTER, arg_t::REGISTER, arg_t::LABEL                    }, true , true }},

    // ジャンプ系(J系)
    // 飛び先は局所ラベルのみ(絶対indexに解決される)．レジスタ・数値による飛び先指定は持たない
    {"jmp"  ,  {1, {arg_t::LABEL                                                      }, true , true }},
    {"call" ,  {1, {arg_t::FUNC_NAME                                                  }, false, false}},  // 引数は呼び出し先関数名。出力は output_instruction_line で特別に組み立てる(rs1=0 + 即値ターゲット)
    {"ret"  ,  {0, {                                                                  }, false, false}},  // 引数なし。汎用経路が machine::ret() を生成する

    // メモリ系(M系)
    {"rm"   ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"wm"   ,  {3, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"brm"  ,  {4, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},
    {"bwm"  ,  {4, {arg_t::MASK    , arg_t::REGISTER, arg_t::REGISTER, arg_t::REGISTER, arg_t::RAW_DATA}, false, true }},

    // 標準入出力系(IO系)
    {"scan" ,  {1, {                                                   arg_t::REGISTER}, false, false}},
    {"print",  {1, {arg_t::REGISTER,                                   arg_t::RAW_DATA}, false, true }},
};

// 引数タイプごとのビット数を返す
std::string get_bit_length_of_command(const arg_t arg) {
    switch (arg) {
        case arg_t::REGISTER:
            return std::to_string(6);

        case arg_t::RAW_DATA:
            return std::to_string(32);

        case arg_t::FUNC_NAME:
            return std::to_string(6);

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
