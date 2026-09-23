#include <string.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "asm2sv.hpp"
#include "asm2sv_main.hpp"
#include "util.hpp"

// コマンドライン引数情報
typedef struct {
    std::string asm_file_name;    // アセンブリファイル名
    std::string sv_file_name;     // 出力ファイル名
} args_t;

// 関数
// (assemble_asm_to_sv以外はこのファイル内でしか使わないため，pn2sv.exeへのリンク時に
//  コンパイラ側の同名シンボルと衝突しないようすべてstaticにする)
static void get_args(int argc, char **argv, args_t &args);                // コマンドライン引数を取得
static void asm2sv(std::ifstream &asm_file, std::ofstream &sv_file);      // アセンブリをSystemVerilogに変換する
static void output_header(std::ofstream &sv_file);                        // svファイルのヘッダーを出力する
static void output_body(std::ifstream &asm_file, std::ofstream &sv_file); // 機械語化した命令部分を出力する
static std::string read_global_line(std::ifstream &asm_file);            // .global行まで読み飛ばし，コメントを除いて返す
static void get_function_names(                                          // プログラムに存在する関数の名前を取得する
    std::map<std::string, std::size_t> &functions, std::string line
);
static void assemble_body(                                               // 本体をアセンブルしfunctions/local_labels/instructionsを埋める
    std::ifstream &asm_file, std::map<std::string, std::size_t> &functions,
    std::map<std::string, std::size_t> &local_labels,
    std::vector<std::string> &instructions
);
static void output_instruction_line(                                             // アセンブリ一行を機械語化しinstructionsへ追加
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &functions, std::string line
);
static std::vector<std::string> split_args(std::string line);            // コメントを除いた引数部分を空白区切りで取り出す
static const command_form_t &select_form(                                // 書かれた引数に合う引数形式を選ぶ
    const std::map<std::string, std::size_t> &functions,
    const std::vector<command_form_t> &forms, const std::vector<std::string> &args,
    const std::string &command
);
static bool matches_form(                                                // 書かれた引数がその形式の書き方に合うか
    const std::map<std::string, std::size_t> &functions,
    const command_form_t &form, const std::vector<std::string> &args
);
static int written_arg_num(const command_form_t &form);                  // 形式がアセンブリ上で取る引数の個数
static std::string form_usage(                                           // 引数形式の書き方(エラーメッセージ用)
    const std::string &command, const command_form_t &form
);
static std::string arg_type_name(const arg_t arg_type);                  // 引数の種類の名前(エラーメッセージ用)
static std::string get_bit_length_of_command(const arg_t arg_type);      // 引数タイプごとのビット数を返す
static std::string convert_arg(                                          // 機械語関数の引数を加工して返す
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const arg_t arg_type, const std::string &command
);
static std::string get_machine_function_name(const std::string &command); // machine.svh側の関数名へ変換する
static bool is_digits_of_base(const std::string &digits, const int base); // 全ての桁がその基数で表せるか
static bool is_number_notation(const std::string &word);                 // 数値表記(基数接尾辞を含む)として妥当か
static bool is_negative_notation(const std::string &word);               // 負の数値表記('-'+10進)として妥当か
static bool is_register_notation(const std::string &word);               // レジスタ表記('r'+数値表記)として妥当か
static void throw_if_tab(const std::string &line);                       // タブ文字があればエラーにする
static void resolve_labels(                                              // 局所ラベル参照を絶対index/相対オフセットに解決する
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &local_labels
);
static std::string offset2imm(const long offset);                        // 相対オフセットをイミディエイト表記にする（負は32bit2の補数）
static std::string join_instructions(                                    // 命令を結合する（末尾カンマ無し）
    const std::vector<std::string> &instructions
);
static std::string function_name2line_num(                                // 関数参照を行番号に置換する
    const std::map<std::string, std::size_t> &functions, const std::string &body
);
static void output_footer(std::ofstream &sv_file);                       // svファイルのフッターを出力する

// 出力されるアセンブリプログラムの最大命令数(空行・コメント・ラベルは数えない)
// ROM自体に固定容量は無く(ROM_SIZEはプログラムの命令数から自動算出する)，プログラムカウンタの
// ビット幅(14ビット)がちょうど表現できる範囲として設定したハードウェア側と揃える上限
const int MAX_LINE_NUM = 16384;
const char FUNC_REF_DELIM = '@';                  // 出力本体で関数参照を囲む区切り文字（命令名や数値との衝突を防ぐ）

// 局所ラベル参照の仮文字列（プレースホルダ）
// jmp/F系の飛び先ラベルは，いったんこの仮文字列で囲んで出力本体に埋め込み，
// 全命令の変換後に resolve_labels が実値（jmp=絶対index，F系=相対オフセット）へ置換する
const std::string LABEL_REF_ABS = "<<ABS:";       // jmp用ラベル参照の開始（絶対indexに解決される）
const std::string LABEL_REF_REL = "<<REL:";       // F系用ラベル参照の開始（相対オフセットに解決される）
const std::string LABEL_REF_CLOSE = ">>";         // ラベル参照の終端

// メイン関数: assemble_asm_to_svをそのまま呼ぶだけ
// pn2sv.cppに直接組み込むビルド(ASM2SV_NO_MAIN定義時)ではmain多重定義を避けるため除外する
#ifndef ASM2SV_NO_MAIN
int main(int argc, char **argv) {
    return assemble_asm_to_sv(argc, argv);
}
#endif

// アセンブリをSystemVerilog ROMに変換する本処理
// 処理に成功したら0，失敗したら1を返り値にする
int assemble_asm_to_sv(int argc, char **argv) {
    args_t args;              // コマンドライン引数
    std::ifstream asm_file;   // アセンブリファイル
    std::ofstream sv_file;    // 出力ファイル

    // コマンドライン引数を取得
    get_args(argc, argv, args);

    // コマンドライン引数の取得に失敗していれば
    if (
        // アセンブリファイル名
        args.asm_file_name.empty()
        // 出力ファイル名
        || args.sv_file_name.empty()
    ) {
        std::cout << "fail args" << std::endl;
        return 1;
    }

    // アセンブリファイルを開く
    asm_file.open(args.asm_file_name);
    if (!asm_file) {
        std::cout << "cannot open asm file: " << args.asm_file_name << std::endl;
        return 1;
    }

    // 出力ファイルを開く（テキストモードによる改行コード変換(LF→CRLF)を避けるためバイナリモードで開く）
    sv_file.open(args.sv_file_name, std::ios::binary);
    if (!sv_file) {
        std::cout << "cannot open sv file: " << args.sv_file_name << std::endl;
        return 1;
    }

    // アセンブリ言語をSystemVerilogに変換する
    try {
        asm2sv(asm_file, sv_file);

        // 正常終了を報告
        std::cout << "assembled: " << args.sv_file_name << std::endl;

        // ファイルを閉じる
        asm_file.close();
        sv_file.close();

        return 0;
    }
    catch (std::string msg) {
        std::cout << msg << std::endl;

        return 1;
    }
}

// コマンドライン引数を取得
// -pt: 必須引数．アセンブリファイル名．
// -sv: 出力ファイル名．省略した場合，アセンブリファイル名の拡張子を変更して同階層に出力される．
// 何も指定せずに引数を置いた場合，アセンブリファイル名と解釈される．
void get_args(int argc, char **argv, args_t &args) {
    // 全ての引数でループ(コマンド名は飛ばす)
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];    // 引数一つ

        // 指定子なら
        if (arg[0] == '-') {
            std::string kind = arg;   // 指定を保存

            // インクリメントして次のパラメータを取得
            i++;
            if (i >= argc) break;
            arg = argv[i];

            // 指定されたパラメータを保存
            if      (kind == "-pt")  args.asm_file_name = argv[i];
            else if (kind == "-sv")  args.sv_file_name  = argv[i];
        }
        // 指定子の直後ではないなら
        else {
            args.asm_file_name = argv[i];
        }
    }

    // アセンブリファイル名が .pt で終わっているか（短い名前での範囲外アクセスを防ぐ）
    const bool asm_name_ok =
        args.asm_file_name.length() >= 3
        && args.asm_file_name.substr(args.asm_file_name.length() - 3) == ".pt";

    // 出力ファイル名が指定されていないなら，アセンブリ名の拡張子を .sv にして使う
    if (asm_name_ok && args.sv_file_name.empty()) {
        // いったんアセンブリファイル名を入れる
        args.sv_file_name = args.asm_file_name;

        // 拡張子を更新
        args.sv_file_name.replace(
            args.sv_file_name.length() - 2,  // 置換するのは後ろから二文字
            2,      // 置換する文字数
            "sv"    // 拡張子は「.sv」にする
        );
    }

    // 出力ファイル名が .sv で終わっているか
    const bool sv_name_ok =
        args.sv_file_name.length() >= 3
        && args.sv_file_name.substr(args.sv_file_name.length() - 3) == ".sv";

    // コマンドライン引数が不正ではないことをチェック
    if (!asm_name_ok || !sv_name_ok) {
        // メッセージ出力
        std::cout << "args fail" << std::endl
                  << "-pt: asm file name. e.g. ~~.pt" << std::endl
                  << "    actual: " << args.asm_file_name << std::endl
                  << "-sv: output file name. e.g. ~~.sv" << std::endl
                  << "    actual: " << args.sv_file_name << std::endl;

        // 後の処理でエラーになるよう，コマンドライン引数をクリア
        args.asm_file_name.clear();
        args.sv_file_name.clear();
    }
}

// アセンブリをSystemVerilogに変換する
void asm2sv(std::ifstream &asm_file, std::ofstream &sv_file) {
    // ヘッダーを出力する
    output_header(sv_file);

    // 機械語化した命令部分を出力する
    output_body(asm_file, sv_file);

    // フッターを出力する
    output_footer(sv_file);

    // バッファに溜まっている分を出力
    sv_file.flush();
}

// svファイルのヘッダーを出力する
void output_header(std::ofstream &sv_file) {
    sv_file << "`include \"rom.svh\"\n"
            << "`include \"machine.svh\"\n"
            << "\n"
            << "module rom_sv(\n"
            << "    input logic clk,\n"
            << "    rom_read_if.slave rom_read\n"
            << "    );\n"
            << "    import machine_p::*;\n"
            << "\n";
}

// 機械語化した命令部分を出力する
void output_body(std::ifstream &asm_file, std::ofstream &sv_file) {
    std::map<std::string, std::size_t> functions;     // 関数とその開始pc
    std::map<std::string, std::size_t> local_labels;  // 局所ラベルとその位置（直後の命令のindex）
    std::vector<std::string> instructions;            // 機械語にした命令一覧（1要素=1命令）

    // .global 行を取得し，宣言された関数名を読み込む
    std::string global_line = read_global_line(asm_file);
    get_function_names(functions, global_line);

    // main関数が指定されていなければ
    if (functions.find("main") == functions.end()) {
        throw std::string("asm syntax error: main function not found");
    }

    // 本体をアセンブルする（functions/local_labels のpc確定 + instructions 生成）
    assemble_body(asm_file, functions, local_labels, instructions);

    // .global で宣言された関数がすべて定義されているか確認する
    // pcがnposのまま残っていれば，宣言だけで定義(ラベル)がない関数
    for (const auto &function : functions) {
        if (function.second == std::string::npos) {
            throw "asm syntax error: declared but not defined function '" + function.first + "'";
        }
    }

    // 局所ラベル参照を解決する（絶対index/相対オフセット）
    resolve_labels(instructions, local_labels);

    // 命令数をlocalparam，machine_t配列として出力する
    const std::string body = function_name2line_num(functions, join_instructions(instructions));
    sv_file << "    localparam integer ROM_SIZE = " << instructions.size() << ";\n\n";
    sv_file << "    (* rom_style = \"block\" *) machine_t machines[0:ROM_SIZE - 1] = {\n";
    sv_file << body;
    sv_file << "    };\n";
}

// .global行まで読み飛ばし，コメントを除いて返す
// .global より前は空行とコメント行(;)のみ許可し，それ以外はエラーにする
std::string read_global_line(std::ifstream &asm_file) {
    std::string line;
    while (getline(asm_file, line)) {
        // .global 行が見つかったら（タブ非対応を確認して）返す
        if (strncmp(".global ", line.c_str(), strlen(".global ")) == 0) {
            throw_if_tab(line);
            return strip_comment(line);
        }

        // 空行でもコメント行でもなければ，.global より前のコードとしてエラー
        if (!ltrim(strip_comment(line)).empty()) {
            throw "asm syntax error: code before .global '" + line + "'";
        }
    }

    // .global 宣言が無いままEOFに達した
    throw std::string("asm syntax error: .global not found");
}

// プログラムに存在する関数の名前を取得する
void get_function_names(
    std::map<std::string, std::size_t> &functions, std::string line
) {
    // 関数指定が正しくなければ
    if (strncmp(".global ", line.c_str(), strlen(".global ")) != 0) {
        throw std::string("asm syntax error: .global fail");
    }

    // 関数の羅列部分を取得
    // lineを上書きしないのは，エラーメッセージで.globalを含む行全体を示すため
    const std::string names = line.substr(strlen(".global "));

    // 関数名一覧を取得(カンマで区切った各要素を関数名とする)
    std::size_t begin = 0;      // 現在の要素の先頭位置
    while (true) {
        // 次のカンマまで(なければ末尾まで)を1要素として切り出す
        const std::size_t comma_index = names.find(',', begin);
        std::string function_name = ltrim(names.substr(begin, comma_index - begin));

        // 末尾の空白を除去（カンマの前に空白がある場合に備える）
        while (!function_name.empty() && function_name.back() == ' ') {
            function_name.pop_back();
        }

        // 空の要素なら(カンマの前後や.globalの後に関数名がない)
        // 空文字列を関数名として登録すると，無関係な箇所のエラーや `:` のみの行の受理につながる
        if (function_name.empty()) {
            throw "asm syntax error: empty function name in .global '" + line + "'";
        }

        // 数値表記・レジスタ表記と同じ綴りなら
        // 即値・レジスタの位置に書いたときどちらとも解釈できるため，宣言の時点で受け付けない
        if (
            is_number_notation(function_name) || is_negative_notation(function_name)
            || is_register_notation(function_name)
        ) {
            throw "asm syntax error: function name conflicts with number or register notation '" + function_name + "'";
        }

        // すでにその名前の関数が登録されていれば
        if (functions.find(function_name) != functions.end()) {
            throw "asm syntax error: function name fail '" + function_name + "'";
        }
        functions[function_name] = std::string::npos;   // いったんnposを入れる

        // 最後の要素なら終了し，そうでなければカンマの次から続ける
        if (comma_index == std::string::npos) break;
        begin = comma_index + 1;
    }
}

// 本体をアセンブルしfunctionsとinstructionsを埋める
// 命令のpcは instructions のインデックスに対応する
void assemble_body(
    std::ifstream &asm_file, std::map<std::string, std::size_t> &functions,
    std::map<std::string, std::size_t> &local_labels,
    std::vector<std::string> &instructions
) {
    std::string line;                       // アセンブリファイルの一文
    std::string current_function;           // 現在変換中の関数名
    bool current_has_ret = false;           // 現在の関数が ret を含むか

    while (getline(asm_file, line)) {
        // コメントを除いた部分(コメント内のコロンをラベルと誤認しないため)
        const std::string code = strip_comment(line);

        // 空行・空白のみの行・コメント行はスキップ(main宣言前のコードとして扱わないため)
        const std::string trimmed = ltrim(code);
        if (trimmed.empty()) continue;

        // ラベル宣言なら
        std::size_t colon_index = code.find_first_of(':');
        if (colon_index != std::string::npos) {
            std::string label_name = code.substr(0, colon_index);

            // 局所ラベル（先頭が '.'）なら，関数とは別に位置だけ記録する
            // 命令は生成せず，.global 照合・main先頭チェック・ret追跡の対象外
            if (!label_name.empty() && label_name[0] == '.') {
                // すでに定義済みなら（ラベルはプログラム全体で一意）
                if (local_labels.find(label_name) != local_labels.end()) {
                    throw "asm syntax error: label overlapping definition '" + label_name + "'";
                }
                // ラベル位置（直後の命令のindex）を記録する
                local_labels[label_name] = instructions.size();
                continue;
            }

            // 以降は関数ラベルの処理
            std::string function_name = label_name;

            // 関数一覧にないなら
            if (functions.find(function_name) == functions.end()) {
                throw "asm syntax error: not defined function '" + function_name + "'";
            }
            // すでにセット済みなら
            if (functions[function_name] != std::string::npos) {
                throw "asm syntax error: function overlapping definition '" + function_name + "'";
            }
            // 最初に宣言された関数がmainではない
            if (instructions.empty() && function_name != "main") {
                throw "asm syntax error: first function is not main '" + function_name + "'";
            }
            // 直前の関数が ret を1つも持たないならエラー
            if (!current_function.empty() && !current_has_ret) {
                throw "asm syntax error: function without ret '" + current_function + "'";
            }

            // 関数の先頭pcを記録し，現在の関数を更新する
            functions[function_name] = instructions.size();
            current_function = function_name;
            current_has_ret = false;
            continue;
        }

        // main関数の宣言前にコードがある
        if (functions["main"] == std::string::npos) {
            throw std::string("asm syntax error: not found main function");
        }

        // 現在の関数が ret を含むか記録する
        std::string command = trimmed.substr(0, str_find_first_of(trimmed, ' '));
        if (command == "ret") current_has_ret = true;

        // アセンブリを機械語にしてinstructionsに追加する
        output_instruction_line(instructions, functions, code);

        // mainはCALLできず戻り先が無いため，main内のretはすべてプログラムの終了を表す
        // 自分自身へのjmp(無限ループ)に置き換えて，どの経路でmainを抜けても同じ終了状態にする
        // 先頭から最初に現れるretだけを置き換える方式は，早期returnがあると末尾のretが残るため採用しない
        if (current_function == "main" && command == "ret") {
            const std::size_t pc = instructions.size() - 1;
            instructions[pc] = "jmp(0, 33'h1_0000_0000 + " + std::to_string(pc) + ")";
        }

        // 最大命令数を超えた
        if (static_cast<int>(instructions.size()) > MAX_LINE_NUM) {
            throw std::string("asm syntax error: instructions more than ") + std::to_string(MAX_LINE_NUM);
        }
    }

    // 最後の関数が ret を1つも持たないならエラー
    if (!current_function.empty() && !current_has_ret) {
        throw "asm syntax error: function without ret '" + current_function + "'";
    }
}

// アセンブリ一行を機械語化しinstructionsへ追加する
// lineはコメントを除いた，空白以外の文字を含む行とする
void output_instruction_line(
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &functions, std::string line
) {
    // タブ文字は非対応
    throw_if_tab(line);

    // 先頭のスペースを除去する
    line = ltrim(line);

    // 命令を取得
    std::string command = line.substr(0, str_find_first_of(line, ' '));

    // 命令が不正なら
    if (commands.find(command) == commands.end()) {
        throw "asm syntax error: fail command '" + command + "'";
    }

    // 書かれた引数を取り出し，それに合う引数形式を選ぶ
    const std::vector<std::string> args = split_args(
        line.substr(std::min(command.length() + 1, line.length()))  // 引数がなかった時のためstd::min
    );
    const command_form_t &form = select_form(functions, commands.at(command), args, command);

    // 命令本体を組み立てる
    std::string instr = get_machine_function_name(command) + "(";

    // 機械語の引数を形式の順に並べる
    // ZEROは0を出し，それ以外は書かれた引数を先頭から順に受け取る
    int arg_num = 0;
    for (std::size_t i = 0; i < form.size(); i++) {
        if (i != 0) instr += ", ";

        if (form[i] == arg_t::ZERO) {
            instr += "0";
            continue;
        }

        instr += convert_arg(functions, args[arg_num], form[i], command);
        arg_num++;
    }

    // 命令を閉じて追加する
    instr += ")";
    instructions.push_back(instr);
}

// コメントを除いたアセンブリ一行の引数部分を空白区切りで取り出す
std::vector<std::string> split_args(std::string line) {
    std::vector<std::string> args;    // 書かれた引数一覧

    while (true) {
        // 引数前のスペースを除去し，引数が尽きたら終わる
        line = ltrim(line);
        if (line.empty()) break;

        // 次のスペースまでを引数一つとして取り出す
        const int first_space = str_find_first_of(line, ' ');
        args.push_back(line.substr(0, first_space));
        line = line.substr(first_space);
    }

    return args;
}

// 書かれた引数に合う引数形式を選ぶ
// まず引数の個数で絞り，同数の形式が複数あるなら書き方で決める
const command_form_t &select_form(
    const std::map<std::string, std::size_t> &functions,
    const std::vector<command_form_t> &forms, const std::vector<std::string> &args,
    const std::string &command
) {
    // エラーメッセージ用に，書かれた命令一行を組み立てておく
    std::string written = command;
    for (const std::string &arg : args) written += " " + arg;

    // 引数の個数が合う形式を集める
    std::vector<const command_form_t *> candidates;
    for (const command_form_t &form : forms) {
        if (written_arg_num(form) == static_cast<int>(args.size())) candidates.push_back(&form);
    }

    // 個数の合う形式がなければ，引数の個数の誤り
    // 何個書けるかが分かるよう，その命令の全ての形式の書き方を示す
    if (candidates.empty()) {
        std::string expected;
        for (const command_form_t &form : forms) {
            if (!expected.empty()) expected += " or ";
            expected += "'" + form_usage(command, form) + "'";
        }
        throw "asm syntax error: fail argument count '" + written + "' (expected " + expected + ")";
    }

    // 個数の合う形式が一つなら，引数の中身の誤りはconvert_argが種類ごとに報告する
    if (candidates.size() == 1) return *candidates[0];

    // 複数あるなら書き方で選ぶ
    const command_form_t *matched = nullptr;    // 書き方の合った形式
    for (const command_form_t *form : candidates) {
        if (!matches_form(functions, *form, args)) continue;

        // 二つ以上合うのは，個数でも書き方でも区別できない形式を並べた表の誤り
        // 先に合った方を黙って選ばず，表を直せるようエラーにする
        if (matched != nullptr) {
            throw "asm syntax error: ambiguous argument form '" + written + "'";
        }

        matched = form;
    }

    // 合う形式があればそれを使う
    if (matched != nullptr) return *matched;

    // どの形式にも合わない(callの呼び出し先が関数名でもレジスタでもない場合など)
    // 何を書けるかが分かるよう，個数の合う形式の書き方を並べて示す
    std::string expected;
    for (const command_form_t *form : candidates) {
        if (!expected.empty()) expected += " or ";
        expected += "'" + form_usage(command, *form) + "'";
    }
    throw "asm syntax error: fail arguments '" + written + "' (expected " + expected + ")";
}

// 引数形式の書き方を「命令 <引数の種類>…」の形で返す(エラーメッセージ用)
std::string form_usage(const std::string &command, const command_form_t &form) {
    std::string usage = command;

    for (const arg_t arg_type : form) {
        // ZEROは書かれた引数を取らないため，書き方には現れない
        if (arg_type == arg_t::ZERO) continue;

        usage += " <" + arg_type_name(arg_type) + ">";
    }

    return usage;
}

// 引数の種類の名前を返す(エラーメッセージ用)
std::string arg_type_name(const arg_t arg_type) {
    switch (arg_type) {
        case arg_t::REGISTER:  return "register";
        case arg_t::RAW_DATA:  return "immediate";
        case arg_t::FUNC_NAME: return "function name";
        case arg_t::LABEL:     return "local label";
        case arg_t::MASK:      return "mask";

        default:
            // 起きないはずのエラーなのでエラーメッセージは適当
            throw std::string("asm syntax error: arg type is fail");
    }
}

// 書かれた引数がその形式の書き方に合うかを返す
// ここでは綴りが解決できるかだけを見て，値の妥当性の検証はconvert_argに任せる
bool matches_form(
    const std::map<std::string, std::size_t> &functions,
    const command_form_t &form, const std::vector<std::string> &args
) {
    int arg_num = 0;    // 照合中の引数の番号

    for (const arg_t arg_type : form) {
        // ZEROは書かれた引数を取らない
        if (arg_type == arg_t::ZERO) continue;

        const std::string &arg = args[arg_num];
        arg_num++;

        switch (arg_type) {
            // 関数名は関数表にあるもののみ
            case arg_t::FUNC_NAME:
                if (functions.find(arg) == functions.end()) return false;
                break;

            // レジスタは'r'に数値表記が続く形のみ
            case arg_t::REGISTER:
                if (!is_register_notation(arg)) return false;
                break;

            // 局所ラベルは先頭が'.'
            case arg_t::LABEL:
                if (arg.empty() || arg[0] != '.') return false;
                break;

            // 即値は数値表記か，先頭indexに解決される関数名
            case arg_t::RAW_DATA:
                if (
                    !is_number_notation(arg) && !is_negative_notation(arg)
                    && functions.find(arg) == functions.end()
                ) return false;
                break;

            // マスクは数値表記のみ
            default:
                if (!is_number_notation(arg)) return false;
                break;
        }
    }

    return true;
}

// 形式がアセンブリ上で取る引数の個数を返す(ZEROは機械語側だけの引数なので数えない)
int written_arg_num(const command_form_t &form) {
    int num = 0;

    for (const arg_t arg_type : form) {
        if (arg_type != arg_t::ZERO) num++;
    }

    return num;
}

// ニーモニックをmachine.svh側の関数名に変換する
// SystemVerilog予約語と衝突するand/or/xor/not/nandは末尾に_を付ける
std::string get_machine_function_name(const std::string &command) {
    if (command == "and") return "and_";
    if (command == "or") return "or_";
    if (command == "xor") return "xor_";
    if (command == "not") return "not_";
    if (command == "nand") return "nand_";

    return command;
}

// 全ての桁がその基数で表せるかを返す(桁が一つもなければ数値ではないとする)
bool is_digits_of_base(const std::string &digits, const int base) {
    if (digits.empty()) return false;

    for (const char digit : digits) {
        // 16進数ではa〜f(A〜F)も桁として使える
        const int value = ('0' <= digit && digit <= '9') ? digit - '0'
                        : ('a' <= digit && digit <= 'f') ? digit - 'a' + 10
                        : ('A' <= digit && digit <= 'F') ? digit - 'A' + 10
                        : -1;  // どの基数の桁でもない文字．次の判定で弾くための値

        if (value < 0 || value >= base) return false;
    }

    return true;
}

// 数値表記として妥当かを返す
// 末尾のb/o/hがあればその基数，なければ10進として，残りの桁がその基数で表せるかを見る
bool is_number_notation(const std::string &word) {
    if (word.empty()) return false;

    switch (word[word.length() - 1]) {
        case 'b': return is_digits_of_base(word.substr(0, word.length() - 1), 2);
        case 'o': return is_digits_of_base(word.substr(0, word.length() - 1), 8);
        case 'h': return is_digits_of_base(word.substr(0, word.length() - 1), 16);
        default:  return is_digits_of_base(word, 10);
    }
}

// 負の数値表記として妥当かを返す('-'に10進の桁が続く形．基数接尾辞は付けられない)
bool is_negative_notation(const std::string &word) {
    return !word.empty() && word[0] == '-' && is_digits_of_base(word.substr(1), 10);
}

// レジスタ表記として妥当かを返す('r'に数値表記が続く形)
bool is_register_notation(const std::string &word) {
    return !word.empty() && word[0] == 'r' && is_number_notation(word.substr(1));
}

// 引数タイプごとのビット数を返す
// 数値表記を書ける引数のみが対象(関数名・局所ラベルは表を引いて解決するため数値の桁数を持たない)
std::string get_bit_length_of_command(const arg_t arg_type) {
    switch (arg_type) {
        case arg_t::REGISTER:
            return std::to_string(6);

        case arg_t::RAW_DATA:
            return std::to_string(32);

        case arg_t::MASK:
            return std::to_string(4);

        default:
            // 起きないはずのエラーなのでエラーメッセージは適当
            throw std::string("asm syntax error: arg type is fail");
    }
}

// 機械語関数の引数を加工して返す
std::string convert_arg(
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const arg_t arg_type, const std::string &command
) {
    std::string converted_arg = arg;   // 引数は加工できないので，加工用の変数を用意

    // 引数が局所ラベルなら (jmp/F系の飛び先)
    // 飛び先は局所ラベルのみ．ここではプレースホルダを埋め，resolve_labelsで実値に解決する
    if (arg_type == arg_t::LABEL) {
        // ラベルは先頭が '.'
        if (converted_arg.empty() || converted_arg[0] != '.') {
            throw "asm syntax error: jump target must be a local label '" + arg + "'";
        }

        // jmpは絶対index，F系は相対オフセットに解決する（命令名で区別）
        // 後で resolve_labels が置換する仮文字列で囲んで埋め込む
        const std::string open = (command == "jmp") ? LABEL_REF_ABS : LABEL_REF_REL;
        return "33'h1_0000_0000 + " + open + converted_arg + LABEL_REF_CLOSE;
    }

    // 引数が関数名なら (callの呼び出し先，または即値の位置に書いた関数の先頭index)
    // 関数名として解決するのは関数名・即値の位置だけ．レジスタ・マスクの位置では解決せず，
    // 通常の引数として検証するため，関数名は後続の種類・数値表記の検証でエラーになる
    if (
        (arg_type == arg_t::FUNC_NAME || arg_type == arg_t::RAW_DATA)
        && functions.find(converted_arg) != functions.end()
    ) {
        // mainはプログラムの開始点で，呼び出しても戻り先へ復帰できない(main内のretは自分自身へのjmpになる)
        // 間接呼び出しも防ぐため，callの呼び出し先だけでなく番地の取得もエラーにする
        if (converted_arg == "main") {
            if (arg_type == arg_t::FUNC_NAME) throw std::string("asm syntax error: cannot call 'main'");
            throw std::string("asm syntax error: cannot take the address of 'main'");
        }

        // 関数名を区切り文字で囲み，先頭indexを即値として渡すため即値使用フラグを立てて返す
        // function_name2line_num が囲まれたトークンだけを行番号へ置換するため，
        // 関数名が命令名や数値の一部と一致して誤置換されることを防げる
        return std::string("33'h1_0000_0000 + ") + FUNC_REF_DELIM + converted_arg + FUNC_REF_DELIM;
    }

    // 引数がレジスタなら
    if (converted_arg[0] == 'r') {
        // 引数タイプが違うなら
        if (arg_type != arg_t::REGISTER) {
            throw "asm syntax error: arg register address fail '" + arg + "'";
        }

        converted_arg = arg.substr(1);
    }
    else {
        // 引数タイプがマスクまたは生の値ではないなら
        if (arg_type != arg_t::MASK && arg_type != arg_t::RAW_DATA) {
            throw "asm syntax error: arg mask or raw data fail '" + arg + "'";
        }
    }

    // 加工後に空文字列なら不正な引数（例: 番号のない "r"）
    if (converted_arg.empty()) {
        throw "asm syntax error: fail arg '" + arg + "'";
    }

    // 負の値は即値だけが取り，基数接尾辞を持たない10進表記に限る
    // (接尾辞付きの負数は基数の書き直しが符号を巻き込み 32'h-4 のような不正な出力になるため)
    if (converted_arg[0] == '-') {
        if (arg_type != arg_t::RAW_DATA || !is_negative_notation(converted_arg)) {
            throw "asm syntax error: fail number notation '" + arg + "'";
        }
    }
    // 負でないなら，基数接尾辞と桁が数値表記として妥当か検証する
    // (末尾の一文字だけを見ると，関数名でも数値でもない綴りが数値として素通りする)
    else if (!is_number_notation(converted_arg)) {
        throw "asm syntax error: fail number notation '" + arg + "'";
    }

    // 引数が十進数表記ではない(末尾が基数接尾辞)なら，Verilogでの表記に書き直す
    const char last = converted_arg[converted_arg.length() - 1];
    if (last == 'b' || last == 'o' || last == 'h') {
        converted_arg = get_bit_length_of_command(arg_type)
                        + '\'' + last
                        + converted_arg.substr(0, converted_arg.length() - 1);
    }

    // イミディエイトデータを使用するなら
    if (arg_type == arg_t::RAW_DATA) {
        // 負の10進数はそのまま足すと符号拡張により33bit目の即値使用フラグが消えるため，
        // 32bit2の補数のhexにしてから足す
        if (!converted_arg.empty() && converted_arg[0] == '-') {
            converted_arg = offset2imm(std::stol(converted_arg));
        }
        converted_arg = "33'h1_0000_0000 + " + converted_arg;
    }

    // 加工した引数を返す
    return converted_arg;
}

// タブ文字があればエラーにする
void throw_if_tab(const std::string &line) {
    if (line.find('\t') != std::string::npos) {
        throw "asm syntax error: tab character is not supported '" + line + "'";
    }
}

// 局所ラベル参照を絶対index/相対オフセットに解決する
// 各命令のインデックスがそのまま自命令のpcになるため，ループのpcを使って計算できる
// jmp（絶対）はラベルのindex，F系（相対）は「ラベルのindex − 自命令pc」に置換する
void resolve_labels(
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &local_labels
) {
    for (std::size_t pc = 0; pc < instructions.size(); pc++) {
        std::string &instr = instructions[pc];

        // 開きタグを探し，ラベル参照の有無と種別（絶対/相対）を判定する
        bool is_abs = true;
        std::size_t open_pos = instr.find(LABEL_REF_ABS);
        if (open_pos == std::string::npos) {
            open_pos = instr.find(LABEL_REF_REL);
            is_abs = false;
        }

        // ラベル参照を持たない命令は何もしない（大多数はここで抜ける）
        if (open_pos == std::string::npos) continue;

        // 開きタグと終端タグの間からラベル名を取り出す
        const std::size_t name_start =
            open_pos + (is_abs ? LABEL_REF_ABS : LABEL_REF_REL).length();
        const std::size_t close_pos = instr.find(LABEL_REF_CLOSE, name_start);
        const std::string label_name = instr.substr(name_start, close_pos - name_start);

        // 参照先ラベルが定義されているか
        auto label = local_labels.find(label_name);
        if (label == local_labels.end()) {
            throw "asm syntax error: undefined label reference '" + label_name + "'";
        }

        // jmp（絶対）はラベルのindex，F系（相対）は「ラベルのindex − 自命令pc」に解決する
        std::string value;
        if (is_abs) {
            value = std::to_string(label->second);
        }
        else {
            const long offset = static_cast<long>(label->second) - static_cast<long>(pc);
            value = offset2imm(offset);
        }

        // 仮文字列（開きタグ〜終端タグ）を実値に置換する
        instr.replace(open_pos, close_pos + LABEL_REF_CLOSE.length() - open_pos, value);
    }
}

// 相対オフセットをイミディエイト表記にする
// 負のオフセットは32bit2の補数のhexにする（33bit目の即値使用フラグを落とさないため）
std::string offset2imm(const long offset) {
    // 0以上ならそのまま10進で出力する
    if (offset >= 0) return std::to_string(offset);

    // 負なら32bit2の補数（例: -4 → 32'hfffffffc）にする
    char buf[16];
    snprintf(buf, sizeof(buf), "32'h%08x", static_cast<unsigned int>(offset));
    return std::string(buf);
}

// 命令を結合する（各命令を8スペースインデントし，カンマ区切りで並べる）
// SystemVerilogの配列初期化子では末尾カンマが構文エラーになるため，末尾要素にはカンマを付けない
std::string join_instructions(const std::vector<std::string> &instructions) {
    std::string body;
    for (std::size_t i = 0; i < instructions.size(); i++) {
        body += "        " + instructions[i];
        if (i + 1 < instructions.size()) body += ",";
        body += "\n";
    }
    return body;
}

// 関数参照を行番号に置換する
std::string function_name2line_num(
    const std::map<std::string, std::size_t> &functions, const std::string &body
) {
    std::string rtn = body;  // 引数は加工できないので，加工用の変数を用意

    // 区切り文字で囲まれた関数参照（@func@ など）を対応する行番号に置換する
    // 区切り文字で囲んでいるため，f1 と f11 のような接頭辞の衝突や，
    // 関数名が命令名・数値の一部に一致することによる誤置換が起きない
    for (const auto &function : functions) {
        replace(
            rtn,
            FUNC_REF_DELIM + function.first + FUNC_REF_DELIM,
            std::to_string(function.second)
        );
    }

    return rtn;
}

// svファイルのフッターを出力する
void output_footer(std::ofstream &sv_file) {
    sv_file << "\n"
            << "    always_ff @(posedge clk) begin\n"
            << "        rom_read.valid <= (rom_read.pc < ROM_SIZE);\n"
            << "\n"
            << "        if (rom_read.pc < ROM_SIZE) begin\n"
            << "            rom_read.machine <= machines[rom_read.pc];\n"
            << "        end else begin\n"
            << "            rom_read.machine <= nop();\n"
            << "        end\n"
            << "    end\n"
            << "\n"
            << "endmodule\n";
}
