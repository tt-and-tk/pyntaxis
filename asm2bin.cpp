#include <string.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "asm2bin.hpp"
#include "util.hpp"

// コマンドライン引数情報
typedef struct {
    std::string asm_file_name;    // アセンブリファイル名
    std::string sv_file_name;     // 出力ファイル名
} args_t;

// 関数
void get_args(int argc, char **argv, args_t &args);               // コマンドライン引数を取得
void asm2bin(std::ifstream &asm_file, std::ofstream &sv_file);    // アセンブリをバイナリに変換する
void output_header(std::ofstream &sv_file);                       // svファイルのヘッダーを出力する
void output_bin(std::ifstream &asm_file, std::ofstream &sv_file); // バイナリ部分を出力する
void validate_call_graph(                                         // callグラフを検証する
    const std::map<std::string, std::vector<std::string>> &call_graph
);
int get_max_call_depth(                                           // callグラフから関数呼び出しの最大深さを求める
    const std::map<std::string, std::vector<std::string>> &call_graph,
    const std::string &function_name,
    std::map<std::string, int> &memo,
    std::set<std::string> &visiting
);
void get_function_names(                                          // プログラムに存在する関数の名前を取得する
    std::map<std::string, std::size_t> &functions, std::string line
);
void output_bin_line(                                             // アセンブリ一行からバイナリを取得
    std::stringstream &bin, const std::map<std::string, std::size_t> &functions,
    std::string &line, int &line_num
);
std::string convert_arg(                                          // 機械語関数の引数を加工して返す
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const command_arg_t &command_arg, const int arg_num
);
void output_call_function(                                        // call 疑似命令を出力する
    std::stringstream &bin, const std::string &target, int &line_num
);
void output_ret_function(std::stringstream &bin, int &line_num);  // ret疑似命令を出力する
void output_call_ret_dispatchers(std::stringstream &bin, int &line_num); // call/ret 共通ディスパッチャを出力する
std::string get_machine_function_name(const std::string &command); // machine.svh側の関数名へ変換する
std::string function_name2line_num(                                // 関数名を行番号に置換する
    const std::map<std::string, std::size_t> &functions, const std::string &bin
);
void output_footer(std::ofstream &sv_file);                       // svファイルのフッターを出力する

const int MAX_LINE_NUM = 255;                     // 出力されるアセンブリプログラムの最大行数
const int BOOTSTRAP_PC = 0;                       // ブートストラップ命令のpc
const int MAIN_ENTRY_PC = 1;                      // main関数の先頭pc（ブートストラップの次）
const int STACK_POINTER_REGISTER = 0x10;          // スタックポインタレジスタの番地
const int RETURN_PC_REGISTER_BEGIN = 0x11;        // 戻り先pcレジスタの先頭番地
const int RETURN_PC_REGISTER_END = 0x1a;          // 戻り先pcレジスタの末尾番地
const int DISPATCH_COMPARE_TEMP_REGISTER = 0x1b;  // ディスパッチャでSP値の比較に使う一時レジスタ番地
const int CALL_TARGET_TEMP_REGISTER = 0x1d;       // call展開時に呼び出し先pcを一時保存するレジスタ番地
const int CALL_RETURN_TEMP_REGISTER = 0x1e;       // call展開時に復帰先pcを一時保存するレジスタ番地
const char *CALL_DISPATCHER_LABEL = "__CALL_DISPATCHER__";  // callディスパッチャの先頭行番号プレースホルダ
const char *RET_DISPATCHER_LABEL = "__RET_DISPATCHER__";    // retディスパッチャの先頭行番号プレースホルダ

int g_call_dispatcher_line = -1;  // callディスパッチャの先頭行番号（output_call_ret_dispatchers内で確定）
int g_ret_dispatcher_line = -1;   // retディスパッチャの先頭行番号（output_call_ret_dispatchers内で確定）

// メイン関数
// 処理に成功したら0，失敗したら1を返り値にする
int main(int argc, char **argv) {
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

    // 出力ファイルを開く
    sv_file.open(args.sv_file_name);
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
// -a: 必須引数．アセンブリファイル名．
// -o: 出力ファイル名．省略した場合，アセンブリファイル名の拡張子を変更して同階層に出力される．
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
            if      (kind == "-a") args.asm_file_name = argv[i];
            else if (kind == "-o") args.sv_file_name  = argv[i];
        }
        // 指定子の直後ではないなら
        else {
            args.asm_file_name = argv[i];
        }
    }

    // バイナリファイル名が指定されていないなら
    if (args.sv_file_name.empty()) {
        // いったんアセンブリファイル名を入れる
        args.sv_file_name = args.asm_file_name;

        // 拡張子を更新
        args.sv_file_name.replace(
            args.sv_file_name.length() - 3,  // 置換するのは後ろから三文字
            3,      // 置換する文字数
            "sv"    // 拡張子は「.sv」にする
        );
    }

    // コマンドライン引数が不正ではないことをチェック
    if (
        // アセンブリファイルの拡張子が正しくない
        args.asm_file_name.substr(args.asm_file_name.length() - 4) != ".asm"
        // 出力ファイルの拡張子が正しくない
        || args.sv_file_name.substr(args.sv_file_name.length() - 3) != ".sv"
    ) {
        // メッセージ出力
        std::cout << "args fail" << std::endl
                  << "-a: asm file name. e.g. ~~.asm" << std::endl
                  << "    actual: " << args.asm_file_name << std::endl
                  << "-o: output file name. e.g. ~~.sv" << std::endl
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
    sv_file << "`include \"machine.svh\"\n"
            << "\n"
            << "module rom(\n"
            << "    input  logic clk,\n"
            << "    input  logic resetn,\n"
            << "\n"
            << "    input  logic [31:0] pc,\n"
            << "    output logic [31:0] machine,\n"
            << "    output logic [31:0] imm\n"
            << "    );\n"
            << "\n"
            << "    logic [63:0] machines[0:" + std::to_string(MAX_LINE_NUM) + "] = {\n";
}

// バイナリ部分を出力する
void output_bin(std::ifstream &asm_file, std::ofstream &sv_file) {
    std::map<std::string, std::size_t> functions;                          // 関数とその開始行
    std::map<std::string, std::vector<std::string>> call_graph;            // 呼び出しグラフ（キー: 関数名，値: その関数が呼び出す関数の一覧）
    std::stringstream bin;    // バイナリ化したコード(System Verilog)
    std::string line;         // アセンブリファイルの一文
    std::string current_function_name;  // 現在変換中の関数名（callグラフ構築に使う）

    // .globalが現れるまで飛ばす
    while (getline(asm_file, line)) {
        if (strncmp(".global ", line.c_str(), strlen(".global ")) == 0) break;
    }

    // プログラムに登場する関数を読み込む
    get_function_names(functions, line);

    // main関数が指定されていなければ
    if (functions.find("main") == functions.end()) {
        throw std::string("asm syntax error: main function not found");
    }

    // 各関数が呼び出す関数を初期化する
    for (const auto &function : functions) {
        call_graph[function.first] = {};
    }

    // SP の初期値を設定する
    // 実行開始時は「戻り先レジスタをまだ1本も使っていない状態」にしておく
    int line_num = BOOTSTRAP_PC;
    bin << "        machine::mov(4'hf, 0, " << STACK_POINTER_REGISTER
        << ", 33'h1_0000_0000 + " << STACK_POINTER_REGISTER << "),\n";
    line_num++;

    // アセンブリを一行ずつ読む
    while (getline(asm_file, line)) {
        // 空行はスキップ
        if (line == "") continue;

        // 関数名の宣言なら
        std::size_t colon_index = line.find_first_of(':');
        if (colon_index != std::string::npos) {
            std::string function_name = line.substr(0, colon_index);

            // エラーチェック
            // 関数一覧にないなら
            if (functions.find(function_name) == functions.end()) {
                throw "asm syntax error: not defined function '" + function_name + "'";
            }
            // すでにセット済みなら
            if (functions[function_name] != std::string::npos) {
                throw "asm syntax error: function overlapping definition '" + function_name + "'";
            }
            // 最初に宣言された関数がmainではない
            if (line_num == MAIN_ENTRY_PC && function_name != "main") {
                throw "asm syntax error: first function is not main '" + function_name + "'";
            }

            // 関数の最初の行数を記録
            functions[function_name] = line_num;

            // 現在変換中の関数名を更新
            current_function_name = function_name;
            continue;
        }

        // main関数の宣言前にコードがある
        if (functions["main"] == std::string::npos) {
            throw std::string("asm syntax error: not found main function");
        }

        // call グラフは後段でまとめて検証する
        // 戻り先レジスタ本数を超える深さや再帰呼び出しは
        // ディスパッチャを生成する前にコンパイルエラーにする
        std::string trimmed = line;
        // 先頭スペースを除去
        while (!trimmed.empty() && trimmed[0] == ' ') {
            trimmed = trimmed.substr(1);
        }
        // コメント行でなければcall命令かどうか調べる
        if (!trimmed.empty() && trimmed[0] != ';') {
            std::string command = trimmed.substr(0, str_find_first_of(trimmed, ' '));
            if (command == "call") {
                // 引数前のスペースを除去
                trimmed = trimmed.substr(std::min(command.length() + 1, trimmed.length()));
                while (!trimmed.empty() && trimmed[0] == ' ') {
                    trimmed = trimmed.substr(1);
                }
                // 呼び出し先関数名を取得してグラフに追加
                std::string target = trimmed.substr(0, str_find_first_of(trimmed, ' '));
                call_graph[current_function_name].push_back(target);
            }
        }

        // アセンブリを機械語にする
        // プログラム行だった場合はline_numを加算する
        output_bin_line(bin, functions, line, line_num);

        // 最大行数を超えた
        if (line_num >= MAX_LINE_NUM) {
            throw std::string("asm syntax error: line more than 255");
        }
    }

    // callグラフを検証する
    validate_call_graph(call_graph);
    // call/retディスパッチャをROM末尾に出力する
    output_call_ret_dispatchers(bin, line_num);

    // ディスパッチャ出力後に最大行数を超えた
    if (line_num >= MAX_LINE_NUM) {
        throw std::string("asm syntax error: line more than 255");
    }

    // 生成したプログラムをファイル出力する
    std::string output = bin.str();
    // プレースホルダをディスパッチャの実行アドレスに置換する
    replace(output, CALL_DISPATCHER_LABEL, std::to_string(g_call_dispatcher_line));
    replace(output, RET_DISPATCHER_LABEL, std::to_string(g_ret_dispatcher_line));
    sv_file << function_name2line_num(functions, output);
}

// call グラフを検証する
// - 再帰呼び出しがないか
// - 戻り先レジスタ数を超える深さがないか
void validate_call_graph(
    const std::map<std::string, std::vector<std::string>> &call_graph
) {
    std::map<std::string, int> memo;
    std::set<std::string> visiting;
    const int max_call_depth = get_max_call_depth(call_graph, "main", memo, visiting);
    const int max_return_register_depth =
        RETURN_PC_REGISTER_END - RETURN_PC_REGISTER_BEGIN + 1;

    if (max_call_depth > max_return_register_depth) {
        throw std::string("asm syntax error: call depth more than return pc registers");
    }
}

int get_max_call_depth(
    const std::map<std::string, std::vector<std::string>> &call_graph,
    const std::string &function_name,
    std::map<std::string, int> &memo,
    std::set<std::string> &visiting
) {
    if (memo.find(function_name) != memo.end()) {
        return memo[function_name];
    }

    if (visiting.find(function_name) != visiting.end()) {
        throw std::string("asm syntax error: recursive call is not supported");
    }

    visiting.insert(function_name);

    int max_depth = 0;
    auto it = call_graph.find(function_name);
    if (it != call_graph.end()) {
        for (const std::string &callee : it->second) {
            if (call_graph.find(callee) == call_graph.end()) {
                throw "asm syntax error: not defined function '" + callee + "'";
            }

            max_depth = std::max(
                max_depth,
                1 + get_max_call_depth(call_graph, callee, memo, visiting)
            );
        }
    }

    visiting.erase(function_name);
    memo[function_name] = max_depth;
    return max_depth;
}

// 関数に存在する関数の名前を取得する
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

        // すでにその名前の関数が登録されていれば
        if (functions.find(function_name) != functions.end()) {
            throw "asm syntax error: function name fail '" + function_name + "'";
        }
        functions[function_name] = std::string::npos;   // いったんnposを入れる

        // 関数名ぶんiに加算
        i += last_index;
    }
}

// アセンブリ一行からバイナリを取得
void output_bin_line(
    std::stringstream &bin, const std::map<std::string, std::size_t> &functions,
    std::string &line, int &line_num
) {
    std::string command;                    // 命令
    bool command_is_call = false;           // 命令がcallだった
    bool command_is_ret = false;            // 命令がretだった

    // 最初のスペースを飛ばす
    while (line[0] == ' ') {
        line = line.substr(1);
    }

    // セミコロンなら
    if (line[0] == ';') return;

    // 命令を取得
    command = line.substr(0, str_find_first_of(line, ' '));
    command_is_call = (command == "call");
    command_is_ret = (command == "ret");

    // 命令が不正なら
    if (
        // 命令が命令一覧の中にない
        commands.find(command) == commands.end()
        // 命令がcall，retではない
        && !command_is_call && !command_is_ret
    ) {
        throw "asm syntax error: fail command '" + command + "'";
    }

    // 命令がcallなら
    if (command_is_call) {
        // 引数前のスペースを除去
        line = line.substr(std::min(command.length() + 1, line.length()));
        while (!line.empty() && line[0] == ' ') {
            line = line.substr(1);
        }

        // 呼び出し先関数名を取得して展開
        const int first_space = str_find_first_of(line, ' ');
        const std::string target = convert_arg(
            functions, line.substr(0, first_space), commands.at("jmp"), 0
        );
        output_call_function(bin, target, line_num);
        return;
    }

    // 命令がretなら
    if (command_is_ret) {
        output_ret_function(bin, line_num);
        return;
    }

    // 命令を出力
    bin << "        machine::" << get_machine_function_name(command) << "(";

    // 引数を取得
    int arg_num = 0;
    line = line.substr(std::min(command.length() + 1, line.length()));  // 引数がなかった時のためstd::min
    while (!line.empty() && line[0] != ';') {
        // スペースを飛ばす
        if (line[0] == ' ') {
            line = line.substr(1);
            continue;
        }

        // 引数を出力
        if (arg_num != 0) bin << ", ";
        int first_space = str_find_first_of(line, ' ');
        const std::string arg = convert_arg(
            functions, line.substr(0, first_space), commands.at(command), arg_num
        );
        bin << arg;

        // 次のループの準備
        line = line.substr(first_space);  // 引数直後のスペースは飛ばさない．最後の引数である可能性があるため
        arg_num++;
    }

    // 引数の数があっているか確認
    command_arg_t command_arg = commands.at(command);
    if (
        // イミディエイトデータが必須で，引数の個数が違う
        (command_arg.imm_required && arg_num != command_arg.arg_num_min)
        // イミディエイトデータが必須ではなく，引数の個数が違う
        || (!command_arg.imm_required && arg_num != command_arg.arg_num_min && arg_num != command_arg.arg_num_min + 1)
    ) {
        throw "asm syntax error: fail program " + command + " " + std::to_string(arg_num);
    }

    // イミディエイトデータ未出力なら，0にしておく
    if (!command_arg.imm_required && (arg_num == command_arg.arg_num_min)) {
        bin << ", 0";
    }

    // 改行を出力
    bin << "),\n";

    // 行数を加算
    line_num++;
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
    const std::string &arg, const command_arg_t &command_arg, const int arg_num
) {
    std::string converted_arg = arg;   // 引数は加工できないので，加工用の変数を用意

    // 引数が関数名なら (命令がcallの場合は関数名が引数になる)
    if (functions.find(converted_arg) != functions.end()) {
        // いったん関数名を返す
        return converted_arg;
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

    // エラーが起きていれば
    if (converted_arg.empty()) throw "asm syntax error: fail arg '" + arg + "'";

    // イミディエイトデータを使用するなら
    if (command_arg.arg_types[arg_num] == arg_t::RAW_DATA) {
        converted_arg = "33'h1_0000_0000 + " + converted_arg;
    }

    // 加工した引数を返す
    return converted_arg;
}

void output_call_function(
    std::stringstream &bin, const std::string &target, int &line_num
) {
    // call 命令はその場で大きく展開しない
    // 1. 呼び出し先関数の開始 pc を一時レジスタへ保存
    // 2. 復帰先 pc を別の一時レジスタへ保存
    // 3. 共通 call ディスパッチャへジャンプ
    // 実際にどの戻り先レジスタへ保存するかは共通ディスパッチャ側で決める
    bin << "        machine::mov(4'hf, 0, " << CALL_TARGET_TEMP_REGISTER
        << ", 33'h1_0000_0000 + " << target << "),\n";
    bin << "        machine::mov(4'hf, 0, " << CALL_RETURN_TEMP_REGISTER
        << ", 33'h1_0000_0000 + " << (line_num + 3) << "),\n";
    bin << "        machine::jmp(0, 33'h1_0000_0000 + "
        << CALL_DISPATCHER_LABEL << "),\n";
    line_num += 3;
}

void output_ret_function(std::stringstream &bin, int &line_num) {
    // ret もその場では展開せず、共通 ret ディスパッチャへ飛ぶだけにする
    // 実際の戻り先レジスタ選択と SP の更新はディスパッチャ側で行う
    bin << "        machine::jmp(0, 33'h1_0000_0000 + "
        << RET_DISPATCHER_LABEL << "),\n";
    line_num++;
}

void output_call_ret_dispatchers(std::stringstream &bin, int &line_num) {
    // call/ret の分岐処理は ROM 末尾に 1 回だけ出力する
    // これにより、各 call ごとに同じ SP 判定コードを複製せずに済む

    // callディスパッチャの先頭行番号を記録
    g_call_dispatcher_line = line_num;

    const int call_state_count =
        RETURN_PC_REGISTER_END - STACK_POINTER_REGISTER + 1;  // SPが取りうる状態数（SP自身〜戻り先レジスタ末尾）
    const int call_compare_line_count = call_state_count * 2;  // 比較命令の行数（各状態につきmov+eqの2命令）
    const int call_case_begin = g_call_dispatcher_line + call_compare_line_count;  // ケース処理が始まる行番号

    // SPの値を各候補値と順番に比較し，一致したケース処理へジャンプする
    for (int current_sp = STACK_POINTER_REGISTER;
         current_sp <= RETURN_PC_REGISTER_END;
         current_sp++) {
        const int case_line =
            call_case_begin + (current_sp - STACK_POINTER_REGISTER) * 3;  // このSP値に対応するケース処理の行番号

        bin << "        machine::mov(4'hf, 0, "
            << DISPATCH_COMPARE_TEMP_REGISTER
            << ", 33'h1_0000_0000 + " << current_sp << "),\n";
        bin << "        machine::eq(" << STACK_POINTER_REGISTER
            << ", " << DISPATCH_COMPARE_TEMP_REGISTER
            << ", 33'h1_0000_0000 + " << case_line << "),\n";
    }

    // 各SPに対応するケース処理：戻り先レジスタへ復帰先pcを保存し，SPを更新してジャンプ
    for (int current_sp = STACK_POINTER_REGISTER;
         current_sp <= RETURN_PC_REGISTER_END;
         current_sp++) {
        if (current_sp < RETURN_PC_REGISTER_END) {
            const int return_register = current_sp + 1;  // 次に使うべき戻り先レジスタ

            // mask=0 の mov は rs1 の値をそのまま rd へ移す用途で使う
            // ここでは一時レジスタに入れておいた復帰先 pc を
            // 選ばれた戻り先レジスタへコピーしている
            bin << "        machine::mov(4'h0, " << CALL_RETURN_TEMP_REGISTER
                << ", " << return_register << ", 0),\n";
            bin << "        machine::mov(4'hf, 0, " << STACK_POINTER_REGISTER
                << ", 33'h1_0000_0000 + " << return_register << "),\n";
            bin << "        machine::jmp(" << CALL_TARGET_TEMP_REGISTER
                << ", 0),\n";
        }
        else {
            // 呼び出し深さは静的検査で弾くので通常この経路には入らない
            // 万一入った場合に暴走しないよう、その場ループで止める
            const int trap_line =
                call_case_begin + (current_sp - STACK_POINTER_REGISTER) * 3;
            bin << "        machine::jmp(0, 33'h1_0000_0000 + "
                << trap_line << "),\n";
            bin << "        machine::nop(),\n";
            bin << "        machine::nop(),\n";
        }
    }

    // callディスパッチャの行数をline_numに加算
    line_num += call_compare_line_count + call_state_count * 3;
    // retディスパッチャの先頭行番号を記録
    g_ret_dispatcher_line = line_num;

    const int ret_state_count =
        RETURN_PC_REGISTER_END - STACK_POINTER_REGISTER + 1;  // SPが取りうる状態数
    const int ret_compare_line_count = ret_state_count * 2;   // 比較命令の行数（各状態につきmov+eqの2命令）
    const int ret_case_begin = g_ret_dispatcher_line + ret_compare_line_count;  // ケース処理が始まる行番号

    // SPの値を各候補値と順番に比較し，一致したケース処理へジャンプする
    for (int current_sp = STACK_POINTER_REGISTER;
         current_sp <= RETURN_PC_REGISTER_END;
         current_sp++) {
        const int case_line =
            ret_case_begin + (current_sp - STACK_POINTER_REGISTER) * 2;  // このSP値に対応するケース処理の行番号

        bin << "        machine::mov(4'hf, 0, "
            << DISPATCH_COMPARE_TEMP_REGISTER
            << ", 33'h1_0000_0000 + " << current_sp << "),\n";
        bin << "        machine::eq(" << STACK_POINTER_REGISTER
            << ", " << DISPATCH_COMPARE_TEMP_REGISTER
            << ", 33'h1_0000_0000 + " << case_line << "),\n";
    }

    // 各SPに対応するケース処理：SPをひとつ戻し，対応する戻り先レジスタへジャンプ
    for (int current_sp = STACK_POINTER_REGISTER;
         current_sp <= RETURN_PC_REGISTER_END;
         current_sp++) {
        if (current_sp == STACK_POINTER_REGISTER) {
            // SP が空スタック位置を指したままの ret は main の終了を意味する
            // 実行状態を明確に保つため、その場ループへ入れる
            const int halt_line =
                ret_case_begin + (current_sp - STACK_POINTER_REGISTER) * 2;
            bin << "        machine::jmp(0, 33'h1_0000_0000 + "
                << halt_line << "),\n";
            bin << "        machine::nop(),\n";
        }
        else {
            bin << "        machine::mov(4'hf, 0, " << STACK_POINTER_REGISTER
                << ", 33'h1_0000_0000 + " << (current_sp - 1) << "),\n";
            bin << "        machine::jmp(" << current_sp << ", 0),\n";
        }
    }

    // retディスパッチャの行数をline_numに加算
    line_num += ret_compare_line_count + ret_state_count * 2;
}

// 関数名を行番号に置換する
std::string function_name2line_num(
    const std::map<std::string, std::size_t> &functions, const std::string &bin
) {
    std::string rtn = bin;  // 引数は加工できないので，加工用の変数を用意

    // 全関数名を対応する行番号に置換
    for (auto function: functions) {
        replace(rtn, function.first, std::to_string(function.second));
    }

    return rtn;
}

// svファイルのフッターを出力する
void output_footer(std::ofstream &sv_file) {
    sv_file << "    };\n"
            << "\n"
            << "    always_comb begin\n"
            << "        if (pc >= 32'h100) begin\n"
            << "            machine <= 32'b0;\n"
            << "            imm <= 32'b0;\n"
            << "        end else begin\n"
            << "            machine <= machines[pc][63:32];\n"
            << "            imm <= machines[pc][31:0];\n"
            << "        end\n"
            << "    end\n"
            << "\n"
            << "endmodule\n";
}
