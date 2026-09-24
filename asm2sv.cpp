#include <string.h>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "asm2sv.hpp"
#include "asm2sv_main.hpp"
#include "machine_writer.hpp"
#include "util.hpp"

// コマンドライン引数情報
typedef struct {
    std::string asm_file_name;    // アセンブリファイル名
    std::string sv_file_name;     // SystemVerilogの出力ファイル名
    std::string bin_file_name;    // 実行ファイルの出力ファイル名(指定した場合はSystemVerilogの代わりに出力する)
} args_t;

// 局所ラベルの定義情報
typedef struct {
    std::size_t index;            // ラベルの位置（直後の命令のindex）
    std::string function;         // ラベルを定義した関数名
} local_label_t;

// 関数
// (assemble_asm_to_sv以外はこのファイル内でしか使わないため，pn2sv.exeへのリンク時に
//  コンパイラ側の同名シンボルと衝突しないようすべてstaticにする)
static void get_args(int argc, char **argv, args_t &args);                // コマンドライン引数を取得
static void assemble(std::ifstream &asm_file, machine_writer &writer);    // アセンブリを機械語にし，出力先の形式で書き出す
static std::string read_global_line(std::ifstream &asm_file);            // .global行まで読み飛ばし，コメントを除いて返す
static void get_function_names(                                          // プログラムに存在する関数の名前を取得する
    std::map<std::string, std::size_t> &functions, std::string line
);
static void assemble_body(                                               // 本体をアセンブルしfunctions/local_labels/instructionsを埋める
    std::ifstream &asm_file, const machine_writer &writer,
    std::map<std::string, std::size_t> &functions,
    std::map<std::string, local_label_t> &local_labels,
    std::vector<instruction_t> &instructions
);
static instruction_t assemble_line(                                      // アセンブリ一行を命令にする
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
static operand_t convert_arg(                                            // 書かれた引数を機械語の引数にする
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const arg_t arg_type, const std::string &command
);
static bool is_digits_of_base(const std::string &digits, const int base); // 全ての桁がその基数で表せるか
static bool is_number_notation(const std::string &word);                 // 数値表記(基数接尾辞を含む)として妥当か
static bool is_negative_notation(const std::string &word);               // 負の数値表記('-'+10進)として妥当か
static bool is_register_notation(const std::string &word);               // レジスタ表記('r'+数値表記)として妥当か
static void throw_if_tab(const std::string &line);                       // タブ文字があればエラーにする
static bool is_executable_name(const std::string &name);                 // Qosmosの実行ファイル名として使えるか
static void resolve_refs(                                                // 関数・局所ラベルの参照をPC/相対オフセットに解決する
    std::vector<instruction_t> &instructions,
    const std::map<std::string, std::size_t> &functions,
    const std::map<std::string, local_label_t> &local_labels,
    const std::size_t base_pc
);
static std::string offset2imm(const long offset);                        // 相対オフセットをイミディエイト表記にする（負は32bit2の補数）

const std::uint64_t IMM_FLAG = 1ULL << 32;          // immの即値使用フラグ(imm[32])
const std::string IMM_FLAG_SV = "33'h1_0000_0000 + "; // SystemVerilog上で即値使用フラグを立てる表記（後ろに即値を足す）

// メイン関数: assemble_asm_to_svをそのまま呼ぶだけ
// pn2sv.cppに直接組み込むビルド(ASM2SV_NO_MAIN定義時)ではmain多重定義を避けるため除外する
#ifndef ASM2SV_NO_MAIN
int main(int argc, char **argv) {
    return assemble_asm_to_sv(argc, argv);
}
#endif

// アセンブリをSystemVerilog ROMまたは実行ファイルに変換する本処理
// 処理に成功したら0，失敗したら1を返り値にする
int assemble_asm_to_sv(int argc, char **argv) {
    args_t args;              // コマンドライン引数
    std::ifstream asm_file;   // アセンブリファイル
    sv_writer sv;             // SystemVerilog ROMの出力先
    bin_writer bin;           // 実行ファイルの出力先

    // コマンドライン引数を取得
    get_args(argc, argv, args);

    // コマンドライン引数の取得に失敗していれば
    if (
        // アセンブリファイル名
        args.asm_file_name.empty()
        // 出力ファイル名
        || (args.sv_file_name.empty() && args.bin_file_name.empty())
    ) {
        std::cout << "fail args" << std::endl;
        return 1;
    }

    // 実行ファイル名の指定があれば実行ファイル，なければSystemVerilogを出力する
    const bool output_bin = !args.bin_file_name.empty();
    machine_writer &writer = output_bin ? static_cast<machine_writer &>(bin) : sv;
    const std::string &output_file_name = output_bin ? args.bin_file_name : args.sv_file_name;

    // アセンブリファイルを開く
    asm_file.open(args.asm_file_name);
    if (!asm_file) {
        std::cout << "cannot open asm file: " << args.asm_file_name << std::endl;
        return 1;
    }

    // アセンブリを出力先の形式に変換する
    try {
        assemble(asm_file, writer);
    }
    catch (std::string msg) {
        std::cout << msg << std::endl;

        return 1;
    }

    // 変換に成功した場合だけ出力ファイルを作る
    // 失敗時に空や途中までのファイルを残すと，誤ってROMや/binへ置いてしまうため
    // テキストモードによる改行コード変換(LF→CRLF)を避けるためバイナリモードで開く
    std::ofstream output_file(output_file_name, std::ios::binary);
    if (!output_file) {
        std::cout << "cannot open output file: " << output_file_name << std::endl;
        return 1;
    }
    output_file << writer.content();
    output_file.close();

    // 書き込みの途中で失敗した場合も，書きかけのファイルを残さない
    // 開けなかった場合に削除しないのは，開けなかった既存のファイルやディレクトリを消してしまうため
    if (!output_file) {
        std::remove(output_file_name.c_str());
        std::cout << "cannot write output file: " << output_file_name << std::endl;
        return 1;
    }

    // 正常終了を報告
    std::cout << "assembled: " << output_file_name << std::endl;

    // ファイルを閉じる
    asm_file.close();

    return 0;
}

// コマンドライン引数を取得
// -pt: 必須引数．アセンブリファイル名．
// -sv: 出力ファイル名．省略した場合，アセンブリファイル名の拡張子を変更して同階層に出力される．
// -bin: 実行ファイル名．指定した場合，SystemVerilogの代わりに実行ファイルを出力する．-svと同時には指定できない．
// 何も指定せずに引数を置いた場合，アセンブリファイル名と解釈される．
void get_args(int argc, char **argv, args_t &args) {
    bool output_bin = false;    // 実行ファイルを出力するか(-binが書かれていれば，値が欠けていても出力を求めたものとする)

    // 全ての引数でループ(コマンド名は飛ばす)
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];    // 引数一つ

        // 指定子なら
        if (arg[0] == '-') {
            std::string kind = arg;   // 指定を保存

            // 値の無い -bin で黙ってSystemVerilogを出力しないよう，値を読む前に指定を覚えておく
            if (kind == "-bin") output_bin = true;

            // インクリメントして次のパラメータを取得
            i++;
            if (i >= argc) break;
            arg = argv[i];

            // 指定されたパラメータを保存
            if      (kind == "-pt")  args.asm_file_name = argv[i];
            else if (kind == "-sv")  args.sv_file_name  = argv[i];
            else if (kind == "-bin") args.bin_file_name = argv[i];
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

    // -sv と -bin を両方指定したか（-sv の自動導出より前に，書かれた指定だけで判定する）
    const bool both_output = output_bin && !args.sv_file_name.empty();

    // SystemVerilogを出力するのに出力ファイル名が指定されていないなら，アセンブリ名の拡張子を .sv にして使う
    if (asm_name_ok && !output_bin && args.sv_file_name.empty()) {
        // いったんアセンブリファイル名を入れる
        args.sv_file_name = args.asm_file_name;

        // 拡張子を更新
        args.sv_file_name.replace(
            args.sv_file_name.length() - 2,  // 置換するのは後ろから二文字
            2,      // 置換する文字数
            "sv"    // 拡張子は「.sv」にする
        );
    }

    // 出力ファイル名が .sv で終わっているか（実行ファイルを出力する場合は使わないため問わない）
    const bool sv_name_ok =
        output_bin
        || (
            args.sv_file_name.length() >= 3
            && args.sv_file_name.substr(args.sv_file_name.length() - 3) == ".sv"
        );

    // 実行ファイル名のうちファイル名の部分(最後のパス区切りより後)が，Qosmosの実行ファイル名として使えるか
    // ディレクトリ部分の.(../binなど)は名前に関わらないため問わない
    const std::string bin_base_name = args.bin_file_name.substr(args.bin_file_name.find_last_of("/\\") + 1);
    const bool bin_name_ok = !output_bin || is_executable_name(bin_base_name);

    // コマンドライン引数が不正ではないことをチェック
    if (!asm_name_ok || both_output || !sv_name_ok || !bin_name_ok) {
        // メッセージ出力
        std::cout << "args fail" << std::endl
                  << "-pt: asm file name. e.g. ~~.pt" << std::endl
                  << "    actual: " << args.asm_file_name << std::endl
                  << "-sv: output file name. e.g. ~~.sv" << std::endl
                  << "    actual: " << args.sv_file_name << std::endl
                  << "-bin: executable file name (1-8 characters, no extension). e.g. HELLO (cannot be used with -sv)" << std::endl
                  << "    actual: " << args.bin_file_name << std::endl;

        // 後の処理でエラーになるよう，コマンドライン引数をクリア
        args.asm_file_name.clear();
        args.sv_file_name.clear();
        args.bin_file_name.clear();
    }
}

// アセンブリを機械語にし，出力先の形式で書き出す
// 関数・局所ラベルは後ろで定義されたものも参照できるため，全ての行を命令にして参照を解決してから書き出す
void assemble(std::ifstream &asm_file, machine_writer &writer) {
    std::map<std::string, std::size_t> functions;     // 関数とその先頭index
    std::map<std::string, local_label_t> local_labels; // 局所ラベルとその位置・定義した関数
    std::vector<instruction_t> instructions;          // アセンブルした命令一覧（1要素=1命令）

    // .global 行を取得し，宣言された関数名を読み込む
    std::string global_line = read_global_line(asm_file);
    get_function_names(functions, global_line);

    // main関数が指定されていなければ
    if (functions.find("main") == functions.end()) {
        throw std::string("asm syntax error: main function not found");
    }

    // 本体をアセンブルする（functions/local_labels の位置確定 + instructions 生成）
    assemble_body(asm_file, writer, functions, local_labels, instructions);

    // .global で宣言された関数がすべて定義されているか確認する
    // indexがnposのまま残っていれば，宣言だけで定義(ラベル)がない関数
    for (const auto &function : functions) {
        if (function.second == std::string::npos) {
            throw "asm syntax error: declared but not defined function '" + function.first + "'";
        }
    }

    // 関数・局所ラベルの参照を解決する（関数・jmpはPC，F系は相対オフセット）
    resolve_refs(instructions, functions, local_labels, writer.base_pc());

    // 命令列を出力先の形式で書き出す
    writer.write_header(instructions.size());
    for (const instruction_t &instruction : instructions) {
        writer.write_instruction(instruction);
    }
    writer.write_footer();
}

// .global行まで読み飛ばし，コメントを除いて返す
// .global より前は空行とコメント行(;)のみ許可し，それ以外はエラーにする
std::string read_global_line(std::ifstream &asm_file) {
    std::string line;
    while (getline(asm_file, line)) {
        const std::string code = strip_comment(line);   // コメントを除いた行

        // .global 行が見つかったら(タブ非対応を確認して)コメントを除いた行を返す
        if (strncmp(".global ", code.c_str(), strlen(".global ")) == 0) {
            throw_if_tab(code);
            return code;
        }

        // 空行・コメント行は読み飛ばす
        if (ltrim(code).empty()) {
            continue;
        }

        // 先頭の語(先頭の空白・タブを飛ばし，次の空白・タブ・カンマの手前まで)を取り出す
        // 空白・タブのみの行では head が行末になり，空文字列になる
        const std::size_t head = std::min(code.find_first_not_of(" \t"), code.size());  // 先頭の語の開始位置
        const std::string word = code.substr(head, code.find_first_of(" \t,", head) - head);  // 先頭の語

        // 先頭の語が .global の行は，.global 行の書き方を誤ったものとして原因が分かるエラーにする
        // 例: `  .global main`(行頭に空白)・`.global;x`(直後がコメント)・`.global<タブ>main`・`.global,main`
        // 語単位で比べるのは，.globalx のような別の語を .global 行と誤認しないため
        if (word == ".global") {
            throw "asm syntax error: .global must start at the beginning of the line and be followed by a space '" + line + "'";
        }

        // それ以外(`  mov ...` や `.globalx` のように先頭の語が .global でない行)は .global より前のコードとしてエラー
        throw "asm syntax error: code before .global '" + line + "'";
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

// 本体をアセンブルしfunctions/local_labels/instructionsを埋める
// 命令の先頭からの位置は instructions のインデックスに対応する
void assemble_body(
    std::ifstream &asm_file, const machine_writer &writer,
    std::map<std::string, std::size_t> &functions,
    std::map<std::string, local_label_t> &local_labels,
    std::vector<instruction_t> &instructions
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

            // 局所ラベル（先頭が '.'）なら，関数ラベルとは別の表に位置と定義した関数を記録する
            // 命令は生成せず，.global 照合・main先頭チェック・ret追跡の対象外
            if (!label_name.empty() && label_name[0] == '.') {
                // main関数の宣言前にある（どの関数にも属さず，参照できない）
                if (current_function.empty()) {
                    throw "asm syntax error: local label before main function '" + label_name + "'";
                }
                // すでに定義済みなら（ラベルはプログラム全体で一意）
                if (local_labels.find(label_name) != local_labels.end()) {
                    throw "asm syntax error: label overlapping definition '" + label_name + "'";
                }
                // ラベル位置（直後の命令のindex）と定義した関数を記録する
                // 所属をindexで判定しないのは，関数末尾のラベルのindexが次の関数の先頭と一致するため
                local_labels[label_name] = {instructions.size(), current_function};
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

            // 関数の先頭indexを記録し，現在の関数を更新する
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

        // アセンブリを命令にし，属する関数を記録してinstructionsに追加する
        // 属する関数は，局所ラベル参照が同じ関数内か確かめるために使う
        instruction_t instruction = assemble_line(functions, code);
        instruction.function = current_function;
        instructions.push_back(instruction);

        // ROMではmainをCALLできず戻り先が無いため，main内のretはすべてプログラムの終了を表す
        // 自分自身へのjmp(無限ループ)に置き換えて，どの経路でmainを抜けても同じ終了状態にする
        // 先頭から最初に現れるretだけを置き換える方式は，早期returnがあると末尾のretが残るため採用しない
        // 実行ファイルではシェルがmainをCALLするため，retのまま残してシェルへ戻す
        if (writer.main_ret_halts() && current_function == "main" && command == "ret") {
            const std::size_t pc = writer.base_pc() + instructions.size() - 1;    // このretのPC
            instructions.back().command = "jmp";
            instructions.back().operands = {
                {"0", 0, ref_t::NONE, ""},
                {IMM_FLAG_SV + std::to_string(pc), IMM_FLAG | pc, ref_t::NONE, ""},
            };
        }

        // 出力先の最大命令数を超えた
        if (instructions.size() > writer.max_instructions()) {
            throw std::string("asm syntax error: instructions more than ") + std::to_string(writer.max_instructions());
        }
    }

    // 最後の関数が ret を1つも持たないならエラー
    if (!current_function.empty() && !current_has_ret) {
        throw "asm syntax error: function without ret '" + current_function + "'";
    }
}

// アセンブリ一行を命令にする(属する関数は呼び出し側が記録する)
// lineはコメントを除いた，空白以外の文字を含む行とする
instruction_t assemble_line(
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
    const command_form_t &form = select_form(functions, commands.at(command).forms, args, command);

    // 命令本体を組み立てる
    instruction_t instruction;
    instruction.command = command;

    // 機械語の引数を形式の順に並べる
    // ZEROは0を出し，それ以外は書かれた引数を先頭から順に受け取る
    int arg_num = 0;
    for (std::size_t i = 0; i < form.size(); i++) {
        if (form[i] == arg_t::ZERO) {
            instruction.operands.push_back({"0", 0, ref_t::NONE, ""});
            continue;
        }

        instruction.operands.push_back(convert_arg(functions, args[arg_num], form[i], command));
        arg_num++;
    }

    return instruction;
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

            // 即値は数値表記か，先頭PCに解決される関数名
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

// 書かれた引数を機械語の引数(SystemVerilog上の表記と値)にする
// 関数名・局所ラベルは位置が確定していないため参照として返し，resolve_refsが表記と値を埋める
operand_t convert_arg(
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const arg_t arg_type, const std::string &command
) {
    std::string converted_arg = arg;   // 引数は加工できないので，加工用の変数を用意

    // 引数が局所ラベルなら (jmp/F系の飛び先)
    // 飛び先は局所ラベルのみ．ここでは参照として返し，resolve_refsで実値に解決する
    if (arg_type == arg_t::LABEL) {
        // ラベルは先頭が '.'
        if (converted_arg.empty() || converted_arg[0] != '.') {
            throw "asm syntax error: jump target must be a local label '" + arg + "'";
        }

        // jmpは絶対PC，F系は相対オフセットに解決する（命令名で区別）
        const ref_t ref = (command == "jmp") ? ref_t::LABEL_ABS : ref_t::LABEL_REL;
        return {"", 0, ref, converted_arg};
    }

    // 引数が関数名なら (callの呼び出し先，または即値の位置に書いた関数の先頭PC)
    // 関数名として解決するのは関数名・即値の位置だけ．レジスタ・マスクの位置では解決せず，
    // 通常の引数として検証するため，関数名は後続の種類・数値表記の検証でエラーになる
    if (
        (arg_type == arg_t::FUNC_NAME || arg_type == arg_t::RAW_DATA)
        && functions.find(converted_arg) != functions.end()
    ) {
        // mainはプログラムの開始点で，呼び出しても戻り先へ復帰できない(ROMではmain内のretは自分自身へのjmpになる)
        // 間接呼び出しも防ぐため，callの呼び出し先だけでなく番地の取得もエラーにする
        if (converted_arg == "main") {
            if (arg_type == arg_t::FUNC_NAME) throw std::string("asm syntax error: cannot call 'main'");
            throw std::string("asm syntax error: cannot take the address of 'main'");
        }

        // 関数の参照として返し，resolve_refsが先頭PCを即値として埋める
        return {"", 0, ref_t::FUNCTION, converted_arg};
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

    // 負でない値は，表記を書き直す前に数値化しておく(負の値は下の即値の処理で数値化する)
    std::uint64_t number = (converted_arg[0] == '-') ? 0 : notation2value(converted_arg);    // 引数の値

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
            // stolはlongが32bitの環境で範囲外を例外にし，64bitの環境では黙って通すため，
            // 環境によらず64bitのstollで数値化してから32bitの範囲を検証する
            long long value = INT64_MIN;  // 数値化した即値．64bitにも収まらない桁数なら最小値のまま範囲外とする
            try {
                value = std::stoll(converted_arg);
            }
            catch (const std::out_of_range &) {}

            // 32bit2の補数で表せる最小値(-2147483648)未満はエラーにする
            if (value < INT32_MIN) {
                throw "asm syntax error: immediate out of range '" + arg + "'";
            }

            converted_arg = offset2imm(static_cast<long>(value));
            number = static_cast<std::uint32_t>(value);
        }
        converted_arg = IMM_FLAG_SV + converted_arg;

        // イミディエイトデータ(32bit)に即値使用フラグを立てる
        number = IMM_FLAG | (number & 0xffffffff);
    }

    // 加工した引数を返す
    return {converted_arg, number, ref_t::NONE, ""};
}

// タブ文字があればエラーにする
void throw_if_tab(const std::string &line) {
    if (line.find('\t') != std::string::npos) {
        throw "asm syntax error: tab character is not supported '" + line + "'";
    }
}

// Qosmosの実行ファイル名として使えるかを返す
// Qosmosは拡張子(.)を持たないファイルだけを実行ファイルとして探し，名前は8.3形式の短い名前で扱う
// このため基本名だけの1〜8文字で，FATの短い名前に使えない文字(制御文字・空白・記号)を含まないものに限る
// 8文字を超える名前はカード上で別の短い名前(HELLOW~1など)になり，付けた名前で実行できないため受け付けない
bool is_executable_name(const std::string &name) {
    // 基本名は1〜8文字
    if (name.empty() || name.length() > 8) return false;

    for (const unsigned char c : name) {
        // 制御文字・空白と，拡張子の区切り・短い名前に使えない記号
        if (c <= ' ' || c == 0x7f || strchr(".\"*+,/:;<=>?[\\]|", c) != nullptr) return false;
    }

    return true;
}

// 関数・局所ラベルの参照を解決し，引数のSystemVerilog上の表記と値を埋める
// 各命令のインデックスがそのまま先頭の命令からの位置になるため，ループのindexを使って計算できる
// 関数・jmp（絶対）は「先頭の命令のPC + 参照先のindex」，F系（相対）は「ラベルのindex − 自命令のindex」に解決する
void resolve_refs(
    std::vector<instruction_t> &instructions,
    const std::map<std::string, std::size_t> &functions,
    const std::map<std::string, local_label_t> &local_labels,
    const std::size_t base_pc
) {
    for (std::size_t index = 0; index < instructions.size(); index++) {
        for (operand_t &operand : instructions[index].operands) {
            // 参照を持たない引数は何もしない（大多数はここで抜ける）
            if (operand.ref == ref_t::NONE) continue;

            std::string imm;              // 解決した即値のSystemVerilog上の表記
            std::uint32_t imm_value = 0;  // 解決した即値

            // 関数なら，その先頭PCにする
            if (operand.ref == ref_t::FUNCTION) {
                const std::size_t pc = base_pc + functions.at(operand.name);
                imm = std::to_string(pc);
                imm_value = static_cast<std::uint32_t>(pc);
            }
            // 局所ラベルなら，定義を確かめてからPCまたは相対オフセットにする
            else {
                // 参照先ラベルが定義されているか
                auto label = local_labels.find(operand.name);
                if (label == local_labels.end()) {
                    throw "asm syntax error: undefined label reference '" + operand.name + "'";
                }
                // 参照先ラベルが参照元と同じ関数内に定義されているか
                if (label->second.function != instructions[index].function) {
                    throw "asm syntax error: label '" + operand.name + "' defined in '" + label->second.function
                        + "' referenced from '" + instructions[index].function + "'";
                }

                // jmp（絶対）はラベルのPC，F系（相対）は「ラベルのindex − 自命令のindex」に解決する
                if (operand.ref == ref_t::LABEL_ABS) {
                    const std::size_t pc = base_pc + label->second.index;
                    imm = std::to_string(pc);
                    imm_value = static_cast<std::uint32_t>(pc);
                }
                else {
                    const long offset = static_cast<long>(label->second.index) - static_cast<long>(index);
                    imm = offset2imm(offset);
                    imm_value = static_cast<std::uint32_t>(offset);
                }
            }

            // 即値使用フラグを立てて埋める
            operand.sv = IMM_FLAG_SV + imm;
            operand.value = IMM_FLAG | imm_value;
        }
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
