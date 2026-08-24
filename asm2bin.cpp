#include <string.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

#include "asm2bin.hpp"
#include "asm2bin_main.hpp"
#include "util.hpp"

// コマンドライン引数情報
typedef struct {
    std::string asm_file_name;    // アセンブリファイル名
    std::string sv_file_name;     // 出力ファイル名
} args_t;

// 関数
// (assemble_asm_to_sv以外はこのファイル内でしか使わないため，c2bin.exeへのリンク時に
//  コンパイラ側の同名シンボルと衝突しないようすべてstaticにする)
static void get_args(int argc, char **argv, args_t &args);               // コマンドライン引数を取得
static void asm2bin(std::ifstream &asm_file, std::ofstream &sv_file);    // アセンブリをバイナリに変換する
static void output_header(std::ofstream &sv_file);                       // svファイルのヘッダーを出力する
static void output_bin(std::ifstream &asm_file, std::ofstream &sv_file); // バイナリ部分を出力する
static std::string read_global_line(std::ifstream &asm_file);            // .global行まで読み飛ばして返す
static void get_function_names(                                          // プログラムに存在する関数の名前を取得する
    std::map<std::string, std::size_t> &functions, std::string line
);
static void assemble_body(                                               // 本体をアセンブルしfunctions/local_labels/instructionsを埋める
    std::ifstream &asm_file, std::map<std::string, std::size_t> &functions,
    std::map<std::string, std::size_t> &local_labels,
    std::vector<std::string> &instructions
);
static void output_bin_line(                                             // アセンブリ一行を機械語化しinstructionsへ追加
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &functions, std::string line
);
static std::string convert_arg(                                          // 機械語関数の引数を加工して返す
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const command_arg_t &command_arg, const int arg_num,
    const std::string &command
);
static void validate_arg_count(                                          // 引数の個数が命令の仕様に合うか検証する
    const command_arg_t &command_arg, const int arg_num, const std::string &command
);
static std::string get_machine_function_name(const std::string &command); // machine.svh側の関数名へ変換する
static void throw_if_tab(const std::string &line);                       // タブ文字があればエラーにする
static void resolve_labels(                                              // 局所ラベル参照を絶対index/相対オフセットに解決する
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &local_labels
);
static std::string offset2imm(const long offset);                        // 相対オフセットをイミディエイト表記にする（負は32bit2の補数）
static void apply_main_self_loop(                                        // mainが到達する最初のretを自己ループに置換する
    std::vector<std::string> &instructions
);
static std::string join_instructions(                                    // 命令を結合する（末尾カンマ無し）
    const std::vector<std::string> &instructions
);
static std::string function_name2line_num(                                // 関数参照を行番号に置換する
    const std::map<std::string, std::size_t> &functions, const std::string &bin
);
static void output_footer(std::ofstream &sv_file);                       // svファイルのフッターを出力する

// 出力されるアセンブリプログラムの最大行数
// ROM自体に固定容量は無く(ROM_SIZEはこの行数から自動算出する)，プログラムカウンタの
// ビット幅(12ビット)がちょうど表現できる範囲として設定したハードウェア側と揃える上限(詳細は../specification/limitations.md)
const int MAX_LINE_NUM = 4096;
const char FUNC_REF_DELIM = '@';                  // 出力本体で関数参照を囲む区切り文字（命令名や数値との衝突を防ぐ）

// 局所ラベル参照の仮文字列（プレースホルダ）
// jmp/F系の飛び先ラベルは，いったんこの仮文字列で囲んで出力本体に埋め込み，
// 全命令の変換後に resolve_labels が実値（jmp=絶対index，F系=相対オフセット）へ置換する
const std::string LABEL_REF_ABS = "<<ABS:";       // jmp用ラベル参照の開始（絶対indexに解決される）
const std::string LABEL_REF_REL = "<<REL:";       // F系用ラベル参照の開始（相対オフセットに解決される）
const std::string LABEL_REF_CLOSE = ">>";         // ラベル参照の終端

// メイン関数: assemble_asm_to_svをそのまま呼ぶだけ
// c2bin.cppに直接組み込むビルド(ASM2BIN_NO_MAIN定義時)ではmain多重定義を避けるため除外する
#ifndef ASM2BIN_NO_MAIN
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

    // アセンブリ言語をバイナリに変換する
    try {
        asm2bin(asm_file, sv_file);

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
// -bin: 出力ファイル名．省略した場合，アセンブリファイル名の拡張子を変更して同階層に出力される．
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
            else if (kind == "-bin") args.sv_file_name  = argv[i];
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
                  << "-bin: output file name. e.g. ~~.sv" << std::endl
                  << "    actual: " << args.sv_file_name << std::endl;

        // 後の処理でエラーになるよう，コマンドライン引数をクリア
        args.asm_file_name.clear();
        args.sv_file_name.clear();
    }
}

// アセンブリをバイナリに変換する
void asm2bin(std::ifstream &asm_file, std::ofstream &sv_file) {
    // ヘッダーを出力する
    output_header(sv_file);

    // バイナリ部分を出力する
    output_bin(asm_file, sv_file);

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

// バイナリ部分を出力する
void output_bin(std::ifstream &asm_file, std::ofstream &sv_file) {
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

    // mainが到達する最初のretを自己ループに置き換える
    apply_main_self_loop(instructions);

    // 命令数をlocalparam，machine_t配列として出力する
    const std::string body = function_name2line_num(functions, join_instructions(instructions));
    sv_file << "    localparam integer ROM_SIZE = " << instructions.size() << ";\n\n";
    sv_file << "    (* rom_style = \"block\" *) machine_t machines[0:ROM_SIZE - 1] = {\n";
    sv_file << body;
    sv_file << "    };\n";
}

// .global行まで読み飛ばして返す
// .global より前は空行とコメント行(;)のみ許可し，それ以外はエラーにする
std::string read_global_line(std::ifstream &asm_file) {
    std::string line;
    while (getline(asm_file, line)) {
        // .global 行が見つかったら（タブ非対応を確認して）返す
        if (strncmp(".global ", line.c_str(), strlen(".global ")) == 0) {
            throw_if_tab(line);
            return line;
        }

        // 空行でもコメント行でもなければ，.global より前のコードとしてエラー
        std::string trimmed = ltrim(line);
        if (!trimmed.empty() && trimmed[0] != ';') {
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
    line = line.substr(strlen(".global "));

    // 関数名一覧を取得
    for (int i = 0; i < static_cast<int>(line.length()); i++) {
        // スペースならスキップ
        if (line[i] == ' ') continue;

        // スペース以外なら，カンマまでを関数名として記録
        std::string word = line.substr(i);        // 厳密には一単語ではないが便宜上wordと呼ぶ
        int last_index = str_find_first_of(word, ',');
        std::string function_name = word.substr(0, last_index);

        // 末尾の空白を除去（カンマの前に空白がある場合に備える）
        while (!function_name.empty() && function_name.back() == ' ') {
            function_name.pop_back();
        }

        // すでにその名前の関数が登録されていれば
        if (functions.find(function_name) != functions.end()) {
            throw "asm syntax error: function name fail '" + function_name + "'";
        }
        functions[function_name] = std::string::npos;   // いったんnposを入れる

        // 関数名の長さぶんiに加算
        i += last_index;
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
        // 空行はスキップ
        if (line == "") continue;

        // 空白のみの行・コメント行はスキップ（コメント内のコロンをラベルと誤認しないため）
        std::string trimmed = ltrim(line);
        if (trimmed.empty() || trimmed[0] == ';') continue;

        // ラベル宣言なら
        std::size_t colon_index = line.find_first_of(':');
        if (colon_index != std::string::npos) {
            std::string label_name = line.substr(0, colon_index);

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
        output_bin_line(instructions, functions, line);

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
void output_bin_line(
    std::vector<std::string> &instructions,
    const std::map<std::string, std::size_t> &functions, std::string line
) {
    // タブ文字は非対応
    throw_if_tab(line);

    // 先頭のスペースを除去する
    line = ltrim(line);

    // セミコロンなら
    if (line[0] == ';') return;

    // 命令を取得
    std::string command = line.substr(0, str_find_first_of(line, ' '));

    // 命令が不正なら
    if (commands.find(command) == commands.end()) {
        throw "asm syntax error: fail command '" + command + "'";
    }

    // 命令がcall/jmpなら
    // どちらも「rs1=0 + 即値ターゲット」という特殊な出力形のため，汎用経路に乗らない
    // call: 呼び出し先pcを即値で渡す（戻り先保存やSP更新はCPU側が行う）
    // jmp : 飛び先（局所ラベルの絶対index）を即値で渡す
    if (command == "call" || command == "jmp") {
        // 引数前のスペースを除去してターゲット（呼び出し先関数名／飛び先ラベル）を取得
        line = ltrim(line.substr(std::min(command.length() + 1, line.length())));
        const int first_space = str_find_first_of(line, ' ');
        const std::string target = convert_arg(
            functions, line.substr(0, first_space), commands.at(command), 0, command
        );

        // 引数は1つだけ．ターゲットの後に（コメント以外の）余分な引数があればエラー
        const std::string rest = ltrim(line.substr(first_space));
        if (!rest.empty() && rest[0] != ';') {
            throw "asm syntax error: too many arguments '" + command + "'";
        }

        // callは関数参照(@func@)に即値プレフィックスを付ける
        // jmpはconvert_argが既にプレフィックス付きのラベル参照を返す
        if (command == "call") {
            instructions.push_back("call(0, 33'h1_0000_0000 + " + target + ")");
        }
        else {
            instructions.push_back("jmp(0, " + target + ")");
        }
        return;
    }

    // 命令の引数仕様
    const command_arg_t &command_arg = commands.at(command);

    // 命令本体を組み立てる
    std::string instr = get_machine_function_name(command) + "(";

    // 引数を取得
    int arg_num = 0;
    line = line.substr(std::min(command.length() + 1, line.length()));  // 引数がなかった時のためstd::min
    while (!line.empty() && line[0] != ';') {
        // スペースを飛ばす
        if (line[0] == ' ') {
            line = line.substr(1);
            continue;
        }

        // 引数が引数仕様の個数より多い（arg_typesの範囲外アクセスを防ぐ）
        if (arg_num >= static_cast<int>(command_arg.arg_types.size())) {
            throw "asm syntax error: too many arguments '" + command + "'";
        }

        // 引数を追加
        if (arg_num != 0) instr += ", ";
        int first_space = str_find_first_of(line, ' ');
        instr += convert_arg(
            functions, line.substr(0, first_space), command_arg, arg_num, command
        );

        // 次のループの準備
        line = line.substr(first_space);  // 引数直後のスペースは飛ばさない．最後の引数である可能性があるため
        arg_num++;
    }

    // 引数の数があっているか確認
    validate_arg_count(command_arg, arg_num, command);

    // immあり・未出力なら，0にしておく
    if (command_arg.has_imm && !command_arg.imm_required && (arg_num == command_arg.arg_num_min)) {
        instr += ", 0";
    }

    // 命令を閉じて追加する
    instr += ")";
    instructions.push_back(instr);
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

// 機械語関数の引数を加工して返す
std::string convert_arg(
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const command_arg_t &command_arg, const int arg_num,
    const std::string &command
) {
    std::string converted_arg = arg;   // 引数は加工できないので，加工用の変数を用意

    // 引数が局所ラベルなら (jmp/F系の飛び先)
    // 飛び先は局所ラベルのみ．ここではプレースホルダを埋め，resolve_labelsで実値に解決する
    if (command_arg.arg_types[arg_num] == arg_t::LABEL) {
        // ラベルは先頭が '.'
        if (converted_arg.empty() || converted_arg[0] != '.') {
            throw "asm syntax error: jump target must be a local label '" + arg + "'";
        }

        // jmpは絶対index，F系は相対オフセットに解決する（命令名で区別）
        // 後で resolve_labels が置換する仮文字列で囲んで埋め込む
        const std::string open = (command == "jmp") ? LABEL_REF_ABS : LABEL_REF_REL;
        return "33'h1_0000_0000 + " + open + converted_arg + LABEL_REF_CLOSE;
    }

    // 引数が関数名なら (命令がcallの場合は関数名が引数になる)
    if (functions.find(converted_arg) != functions.end()) {
        // 関数名を区切り文字で囲んで返す
        // function_name2line_num が囲まれたトークンだけを行番号へ置換するため，
        // 関数名が命令名や数値の一部と一致して誤置換されることを防げる
        return FUNC_REF_DELIM + converted_arg + FUNC_REF_DELIM;
    }

    // 引数がレジスタなら
    if (converted_arg[0] == 'r') {
        // 引数タイプが違うなら
        if (command_arg.arg_types[arg_num] != arg_t::REGISTER) {
            throw "asm syntax error: arg register address fail '" + arg + "'";
        }

        converted_arg = arg.substr(1);
    }
    else {
        // 引数タイプがマスクまたは生の値ではないなら
        if (command_arg.arg_types[arg_num] != arg_t::MASK && command_arg.arg_types[arg_num] != arg_t::RAW_DATA) {
            throw "asm syntax error: arg mask or raw data fail '" + arg + "'";
        }
    }

    // 加工後に空文字列なら不正な引数（例: 番号のない "r"）
    if (converted_arg.empty()) {
        throw "asm syntax error: fail arg '" + arg + "'";
    }

    // 引数が十進数表記ではないなら
    const char last = converted_arg[converted_arg.length() - 1];
    if (last < '0' || last > '9') {
        switch (last) {
            case 'b': case 'o': case 'h': // 2進数，8進数，16進数
                // Verilogでの表記に書き直す
                converted_arg = get_bit_length_of_command(command_arg.arg_types[arg_num])
                                + '\'' + last
                                + converted_arg.substr(0, converted_arg.length() - 1);
                break;
            default:
                throw std::string("asm syntax error: fail base number '") + last + "'";
        }
    }

    // イミディエイトデータを使用するなら
    if (command_arg.arg_types[arg_num] == arg_t::RAW_DATA) {
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

// 引数の個数が命令の仕様に合うか検証する
void validate_arg_count(
    const command_arg_t &command_arg, const int arg_num, const std::string &command
) {
    if (
        // イミディエイトデータが必須で，引数の個数が違う
        (command_arg.imm_required && arg_num != command_arg.arg_num_min)
        // immあり・省略可で，引数の個数が違う（arg_num_min または arg_num_min+1 が有効）
        || (!command_arg.imm_required && command_arg.has_imm && arg_num != command_arg.arg_num_min && arg_num != command_arg.arg_num_min + 1)
        // immなしで，引数の個数が違う（arg_num_min のみ有効）
        || (!command_arg.imm_required && !command_arg.has_imm && arg_num != command_arg.arg_num_min)
    ) {
        throw "asm syntax error: fail program " + command + " " + std::to_string(arg_num);
    }
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

// mainが到達する最初のretを自己ループに置換する
// プログラムはmain(pc=0)から実行されるため，先頭から線形に見て最初に現れるretが
// mainがCALLされずに到達するret＝戻り先の無いretになる．これを自分自身へのjmp(無限ループ)に
// 置き換えてmainを停止させる（mainは必ずretを持つので必ず見つかる）
void apply_main_self_loop(std::vector<std::string> &instructions) {
    for (std::size_t pc = 0; pc < instructions.size(); pc++) {
        if (instructions[pc] == "ret()") {
            instructions[pc] =
                "jmp(0, 33'h1_0000_0000 + " + std::to_string(pc) + ")";
            return;  // 最初の1つだけ置換する
        }
    }
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
    const std::map<std::string, std::size_t> &functions, const std::string &bin
) {
    std::string rtn = bin;  // 引数は加工できないので，加工用の変数を用意

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
