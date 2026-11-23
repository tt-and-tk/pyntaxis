#pragma once

#include <cstdint>
#include <string>

#include "asm2mc.hpp"

// アセンブリの.reserve宣言と.stack宣言に書いた内容
// どちらも実行ファイルが領域に必要とする大きさを表し，実行ファイルのヘッダに書く
typedef struct {
    bool has_reserve;            // .reserveを書いたか
    std::uint32_t reserve_size;  // 命令列の直後に確保する範囲のバイト数(.reserveの値)
    bool has_stack;              // .stackを書いたか(書かなければスタックの量は未定)
    std::uint32_t stack_size;    // スタックの量のバイト数(.stackの値)
} region_declaration_t;

// アセンブルした命令列の出力先
// 出力先ごとに，命令数の上限・mainのretの扱い・宣言の扱い・出力の形式が異なる
// 同じメソッドを順に呼べば，インスタンスの種類に応じた出力の内容が組み上がる
class machine_writer {
public:
    virtual ~machine_writer() {}

    virtual std::size_t max_instructions() const = 0;                     // 命令数の上限
    virtual bool main_ret_halts() const = 0;                              // mainのretを自分自身へのjmpに置き換えるか
    virtual void check_declaration(                                       // 宣言がこの出力先で使えるかを確かめ，使えなければエラーにする
        std::size_t instruction_num, const region_declaration_t &declaration
    ) const = 0;
    virtual void write_header(                                            // 命令列の前に置く内容を出力する
        std::size_t instruction_num, const region_declaration_t &declaration
    ) = 0;
    virtual void write_instruction(const instruction_t &instruction) = 0; // 命令一つを出力する
    virtual void write_footer() = 0;                                      // 命令列の後に置く内容を出力する

    // 組み上がった出力内容を返す
    const std::string &content() const { return this->output; }

protected:
    std::string output;    // 組み上がった出力内容
};

// ROMとして埋め込むSystemVerilogの出力先
// 各命令はmachine.svhの関数呼び出しとして書き，機械語への変換はSystemVerilog側に任せる
class sv_writer : public machine_writer {
public:
    // プログラムカウンタのビット幅(14ビット)がちょうど表現できる範囲として設定したハードウェア側と揃える
    // ROM自体に固定容量は無い(ROM_SIZEはプログラムの命令数から自動算出する)
    static constexpr std::size_t MAX_INSTRUCTIONS = 16384;    // ROMに置ける命令数の上限

    // ROMに置ける命令数の上限
    std::size_t max_instructions() const override { return this->MAX_INSTRUCTIONS; }

    // mainは戻り先を持たないため，retをプログラムの終了(同じ命令を実行し続ける)に置き換える
    bool main_ret_halts() const override { return true; }

    // ROMは領域を割り振られないため，大きさの宣言を書いたらエラーにする
    // (.stackは.reserveの後にだけ書けるため，.reserveの有無で判定する)
    void check_declaration(std::size_t, const region_declaration_t &declaration) const override {
        if (declaration.has_reserve) {
            throw std::string("asm syntax error: .reserve and .stack cannot be used for ROM");
        }
    }

    // モジュールの宣言から，命令数のlocalparamと命令の配列の開始までを出力する
    void write_header(std::size_t instruction_num, const region_declaration_t &) override {
        this->output += "`include \"rom.svh\"\n"
                        "`include \"machine.svh\"\n"
                        "\n"
                        "module rom_sv(\n"
                        "    input logic clk,\n"
                        "    rom_read_if.slave rom_read1,\n"
                        "    rom_read_if.slave rom_read2\n"
                        "    );\n"
                        "    import machine_p::*;\n"
                        "\n";
        this->output += "    localparam integer ROM_SIZE = " + std::to_string(instruction_num) + ";\n\n";
        this->output += "    (* rom_style = \"block\" *) machine_t machines[0:ROM_SIZE - 1] = {\n";

        // 配列の最後の要素を判定するため，出力する命令の数を覚えておく
        this->rest = instruction_num;
    }

    // 命令をmachine.svhの関数呼び出しとして，8スペースインデントで出力する
    void write_instruction(const instruction_t &instruction) override {
        this->output += "        " + this->machine_function_name(instruction.command) + "(";

        // 機械語の引数をSystemVerilog上の表記でカンマ区切りに並べる
        for (std::size_t i = 0; i < instruction.operands.size(); i++) {
            if (i != 0) this->output += ", ";
            this->output += instruction.operands[i].sv;
        }

        // SystemVerilogの配列初期化子では末尾カンマが構文エラーになるため，末尾要素にはカンマを付けない
        this->rest--;
        this->output += (this->rest > 0) ? "),\n" : ")\n";
    }

    // 命令の配列を閉じ，読み出しポートごとにPCに対応する命令を返す処理を出力する
    void write_footer() override {
        this->output += "    };\n"
                        "\n";
        this->write_read_port("rom_read1");
        this->output += "\n";
        this->write_read_port("rom_read2");
        this->output += "\n"
                        "endmodule\n";
    }

private:
    std::size_t rest = 0;    // まだ出力していない命令の数

    // 読み出しポート一つ分の，PCに対応する命令を返す処理を出力する
    // ポートごとにalways_ffを分けるのは，同じ配列をブロックRAMの2ポートとして推論させるVivadoの記述例に合わせるため
    void write_read_port(const std::string &port) {
        this->output += "    always_ff @(posedge clk) begin\n"
                        "        " + port + ".valid <= (" + port + ".pc < ROM_SIZE);\n"
                        "\n"
                        "        if (" + port + ".pc < ROM_SIZE) begin\n"
                        "            " + port + ".machine <= machines[" + port + ".pc];\n"
                        "        end else begin\n"
                        "            " + port + ".machine <= nop();\n"
                        "        end\n"
                        "    end\n";
    }

    // ニーモニックをmachine.svh側の関数名に変換する
    // SystemVerilog予約語と衝突するand/or/xor/not/nandは末尾に_を付ける
    std::string machine_function_name(const std::string &command) const {
        if (command == "and") return "and_";
        if (command == "or") return "or_";
        if (command == "xor") return "xor_";
        if (command == "not") return "not_";
        if (command == "nand") return "nand_";

        return command;
    }
};

// Qosmosの実行ファイルの出力先
// 実行ファイルは，16バイトのヘッダの後に，命令を1命令8バイトずつ並べたもの
// ヘッダは4バイトのワードを4つ並べる: 識別子・命令列の大きさ・命令列の直後に確保する範囲の大きさ・スタックの量
// シェルはヘッダから，領域に必要な大きさを確かめる
// シェルはヘッダを領域へ書き写さず，命令列だけを領域の先頭へ書き写す
// シェルは先頭の命令(領域の中のPC 0)からユーザーモードで実行する
class bin_writer : public machine_writer {
public:
    static constexpr std::size_t REGION_SIZE = 32 * 1024;       // シェルが実行ファイルに割り振る領域の大きさ(バイト．どの実行ファイルにも同じ大きさを割り振る)
    static constexpr std::size_t ARGS_AREA_SIZE = 256;          // 領域の末尾のうち，シェルがコマンド名と引数を置くために取る大きさ(バイト)
    static constexpr std::size_t INSTRUCTION_SIZE = 8;          // 1命令の大きさ(バイト)
    static constexpr std::uint32_t DEFAULT_STACK_SIZE = 4096;   // スタックの量が未定のときに，シェルが領域に収まるかを確かめるのに使うスタックの量(バイト)
    static constexpr std::uint32_t STACK_SIZE_UNSPECIFIED = 0xFFFFFFFF;  // ヘッダのスタックの量に書く，未定を表す値
    static constexpr unsigned char FORMAT_VERSION = 0;          // 実行ファイルの形式の版数(形式を変えたら上げ，シェルは対応する版数の実行ファイルだけを実行する)

    // 領域のうち，末尾のコマンド名と引数の置き場と重ならずに収まる命令数
    std::size_t max_instructions() const override { return (this->REGION_SIZE - this->ARGS_AREA_SIZE) / this->INSTRUCTION_SIZE; }

    // 実行ファイルは終了をシステムコールでシェルへ伝え，mainのretには到達しない前提のため，retのまま残す
    bool main_ret_halts() const override { return false; }

    // 命令列の直後に確保する範囲の大きさを宣言していなければエラーにする
    // 命令列・確保する範囲・スタックの合計が，領域のうち引数の置き場より手前に収まらなければエラーにする
    // (スタックの量が未定なら，シェルと同じく既定の量で確かめる)
    void check_declaration(std::size_t instruction_num, const region_declaration_t &declaration) const override {
        if (!declaration.has_reserve) {
            throw std::string("asm syntax error: .reserve not found (required for executable)");
        }

        // 32ビットの値の合計で桁あふれしないよう，64ビットで足す
        const std::uint64_t code_size = static_cast<std::uint64_t>(instruction_num) * this->INSTRUCTION_SIZE;  // 命令列のバイト数
        const std::uint64_t stack_size = declaration.has_stack ? declaration.stack_size : this->DEFAULT_STACK_SIZE;  // 確かめるスタックの量
        if (code_size + declaration.reserve_size + stack_size > this->REGION_SIZE - this->ARGS_AREA_SIZE) {
            throw "asm syntax error: instructions (" + std::to_string(code_size) + " bytes), reserved area ("
                  + std::to_string(declaration.reserve_size) + " bytes) and stack (" + std::to_string(stack_size)
                  + " bytes) exceed area for executable (" + std::to_string(this->REGION_SIZE - this->ARGS_AREA_SIZE) + " bytes)";
        }
    }

    // ヘッダの4つのワードを出力する
    // 識別子はファイル上で0x7F・'Q'・'S'・版数の順に並べる(0x7Fはテキストに現れないため，テキストファイルを実行ファイルと取り違えない)
    // スタックの量が未定なら，未定を表す値を書く
    void write_header(std::size_t instruction_num, const region_declaration_t &declaration) override {
        this->output += static_cast<char>(0x7F);
        this->output += 'Q';
        this->output += 'S';
        this->output += static_cast<char>(this->FORMAT_VERSION);
        this->write_word(static_cast<std::uint32_t>(instruction_num * this->INSTRUCTION_SIZE));
        this->write_word(declaration.reserve_size);
        this->write_word(declaration.has_stack ? declaration.stack_size : this->STACK_SIZE_UNSPECIFIED);
    }

    // 命令を64ビットの機械語にし，下位のバイトから順に8バイトで出力する
    // imm[31:0]が先の4バイト，機械語の上位32ビットが後の4バイトになり，それぞれの中もリトルエンディアンになる
    // メモリ上の整数をそのまま書き出さないのは，バイトの並びが処理系のエンディアンに依存するため
    void write_instruction(const instruction_t &instruction) override {
        const std::uint64_t machine = this->encode(instruction);    // 64ビットの機械語

        // 下位のバイトから順に取り出して出力する
        for (std::size_t byte = 0; byte < this->INSTRUCTION_SIZE; byte++) {
            this->output += static_cast<char>((machine >> (byte * 8)) & 0xff);
        }
    }

    // フッターを持たない
    void write_footer() override {}

private:
    // 4バイトのワードを，下位のバイトから順に出力する
    void write_word(std::uint32_t word) {
        for (std::size_t byte = 0; byte < 4; byte++) {
            this->output += static_cast<char>((word >> (byte * 8)) & 0xff);
        }
    }

    // 命令を {m_type(3), func(6), mask(4), rs1(6), rs2(6), rd(6), imm(33)} の64ビットに組み立てる
    std::uint64_t encode(const instruction_t &instruction) const {
        const command_t &command = commands.at(instruction.command);    // 命令の定義

        // m_typeとfuncを置く
        std::uint64_t machine = (static_cast<std::uint64_t>(command.type) << 61)
                              | (static_cast<std::uint64_t>(command.func) << 55);

        // 機械語の引数を，それぞれのフィールドの幅に切り詰めて置く
        // 切り詰めはSystemVerilog側で機械語の関数の引数型へ渡すときと同じ扱い
        for (std::size_t i = 0; i < command.fields.size(); i++) {
            const std::uint64_t value = instruction.operands[i].value;    // 引数の値

            switch (command.fields[i]) {
                case field_t::MASK: machine |= (value & 0xf)           << 51; break;
                case field_t::RS1:  machine |= (value & 0x3f)          << 45; break;
                case field_t::RS2:  machine |= (value & 0x3f)          << 39; break;
                case field_t::RD:   machine |= (value & 0x3f)          << 33; break;
                case field_t::IMM:  machine |= (value & 0x1ffffffffULL);      break;

                default:
                    // 起きないはずのエラーなのでエラーメッセージは適当
                    throw std::string("asm syntax error: field type is fail");
            }
        }

        return machine;
    }
};
