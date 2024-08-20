#include <string.h>
#include <iostream>
#include <fstream>

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
void output_bin_line(                                             // アセンブリ一行からバイナリを取得
    std::ofstream &sv_file, const std::map<std::string, std::size_t> &functions,
    std::string &line, int &line_num
);
void output_footer(std::ofstream &sv_file);                       // svファイルのフッターを出力する

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
        std::cout << "args fail" << std::endl
                  << "-a: asm file name. e.g. ~~.asm" << std::endl
                  << "    actual: " << args.asm_file_name << std::endl
                  << "-o: output file name. e.g. ~~.sv" << std::endl
                  << "    actual: " << args.sv_file_name << std::endl;
        return 0;
    }

    // アセンブリファイルを開く
    asm_file.open(args.asm_file_name);
    if (!asm_file) {
        std::cout << "cannot open asm file: " << args.asm_file_name << std::endl;
        return 0;
    }

    // 出力ファイルを開く
    sv_file.open(args.sv_file_name);
    if (!sv_file) {
        std::cout << "cannot open sv file: " << args.sv_file_name << std::endl;
        return 0;
    }

    // アセンブリ言語をバイナリに変換する
    try {
        asm2bin(asm_file, sv_file);
    }
    catch (std::string msg) {
        std::cout << msg << std::endl;
    }

    // ファイルを閉じる
    asm_file.close();
    sv_file.close();

    return 0;
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
            2,      // 置換する文字数
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

    sv_file.flush();
}

// svファイルのヘッダーを出力する
void output_header(std::ofstream &sv_file) {
    sv_file << "`include \"machine.svh\"\n"
            << "\n"
            << "module rom(\n"
            << "   input clk,\n"
            << "   input resetn,\n"
            << "\n"
            << "   input  logic [31:0] pc,\n"
            << "   output logic [31:0] machine,\n"
            << "   output logic [31:0] imm\n"
            << "   );\n"
            << "\n"
            << "    logic[63:0] machines[0:255] = {\n";
}

// バイナリ部分を出力する
void output_bin(std::ifstream &asm_file, std::ofstream &sv_file) {
    std::map<std::string, std::size_t> functions;  // 関数とその開始行
    std::string line;   // アセンブリファイルの一文
    int i;              // 機械語の行数

    // プログラムに登場する関数を読み込む
    if (getline(asm_file, line)) {
        // 関数指定が正しくなければ
        if (strncmp(".global ", line.c_str(), strlen(".global ")) != 0) {
            throw "asm syntax error: .global fail";
        }

        // 関数の羅列部分を取得
        line = line.substr(strlen(".getline "));

        // 関数名一覧を取得
        for (int i = 0; i < line.length(); i++) {
            // スペースならスキップ
            if (line[i] == ' ') continue;

            // スペース以外なら，カンマまでを関数名として記録
            std::string word = line.substr(i);     // 厳密には一単語ではないが便宜上wordと呼ぶ
            int last_index = str_find_first_of(word, ',');
            functions[word.substr(0, last_index)] = std::string::npos;   // いったんnposを入れる

            // 関数名ぶんiに加算
            i = last_index;
        }
    }
    else {
        throw "asm syntax error: no .global";
    }

    // main関数が指定されていなければ
    if (functions.find("main") == functions.end()) {
        throw "asm syntax error: main function not found";
    }

    // アセンブリを一行ずつ読む
    int line_num = 0;    // 機械語の行数
    while (getline(asm_file, line)) {
        // 空行はスキップ
        if (line == "") continue;

        // 関数名の宣言なら
        std::size_t colon_index = line.find_first_of(':');
        if (colon_index != std::string::npos) {
            std::string function_name = line.substr(0, colon_index);

            // 関数一覧にないなら
            if (functions.find(function_name) == functions.end()) {
                throw "asm syntax error: not defined function '" + function_name + "'";
            }

            // すでにセット済みなら
            if (functions[function_name] != std::string::npos) {
                throw "asm syntax error: function overlapping definition '" + function_name + "'";
            }

            // 関数の最初の行数を記録
            functions[function_name] = line_num;

            continue;
        }

        // アセンブリを機械語にする
        output_bin_line(sv_file, functions, line, line_num);
    }
}

// アセンブリ一行からバイナリを取得
void output_bin_line(std::ofstream &sv_file, const std::map<std::string, std::size_t> &functions, std::string &line, int &line_num) {
    std::string command;     // 命令

    // 最初のスペースを飛ばす
    while (line[0] == ' ') {
        line = line.substr(1);
    }

    // セミコロンなら
    if (line[0] == ';') return;

    // 命令を取得
    command = line.substr(0, str_find_first_of(line, ' '));

    // 命令が命令一覧の中にないなら
    if (commands.find(command) == commands.end()) {
        throw "asm syntax error: fail command '" + command + "'";
    }

    // 命令を出力
    sv_file << "        machine::" << command << "(";

    // 引数を取得
    int arg_num = 0;
    while (!line.empty() && line[0] != ';') {
        // スペースを飛ばす
        if (line[0] == ' ') {
            line = line.substr(1);
            continue;
        }

        // 引数を出力
        if (arg_num != 0) sv_file << ", ";
        int first_space = str_find_first_of(line, ' ');
        sv_file << convert_arg(functions, line.substr(0, first_space));

        // 次のループの準備
        line = line.substr(first_space + 1);  // 引数直後のスペースも飛ばすので+1する
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

    // 行数を加算
    line_num++;
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
            << "            machine <= machines[pc][63:32];"
            << "            imm <= machines[pc][31:0];"
            << "        end\n"
            << "    end\n"
            << "\n"
            << "endmodule\n";
}
