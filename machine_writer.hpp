#pragma once

#include <cstdint>
#include <string>

#include "asm2mc.hpp"

// アセンブルした命令列の出力先
// 出力先ごとに，命令を置くPC・命令数の上限・mainのretの扱いと，出力の形式が異なる
// 同じメソッドを順に呼べば，インスタンスの種類に応じた内容がcontentに組み上がる
class machine_writer {
public:
    virtual ~machine_writer() {}

    virtual std::size_t base_pc() const = 0;                              // 先頭の命令のPC
    virtual std::size_t max_instructions() const = 0;                     // 命令数の上限
    virtual bool main_ret_halts() const = 0;                              // mainのretを自分自身へのjmpに置き換えるか
    virtual void write_header(std::size_t instruction_num) = 0;           // 命令列の前に置く内容を出力する
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

    // ROMの先頭がPC 0にあたる
    std::size_t base_pc() const override { return 0; }

    // ROMに置ける命令数の上限
    std::size_t max_instructions() const override { return this->MAX_INSTRUCTIONS; }

    // mainは戻り先を持たないため，retをプログラムの終了(同じ命令を実行し続ける)に置き換える
    bool main_ret_halts() const override { return true; }

    // モジュールの宣言から，命令数のlocalparamと命令の配列の開始までを出力する
    void write_header(std::size_t instruction_num) override {
        this->output += "`include \"rom.svh\"\n"
                        "`include \"machine.svh\"\n"
                        "\n"
                        "module rom_sv(\n"
                        "    input logic clk,\n"
                        "    rom_read_if.slave rom_read\n"
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

    // 命令の配列を閉じ，PCに対応する命令を返す処理を出力する
    void write_footer() override {
        this->output += "    };\n"
                        "\n"
                        "    always_ff @(posedge clk) begin\n"
                        "        rom_read.valid <= (rom_read.pc < ROM_SIZE);\n"
                        "\n"
                        "        if (rom_read.pc < ROM_SIZE) begin\n"
                        "            rom_read.machine <= machines[rom_read.pc];\n"
                        "        end else begin\n"
                        "            rom_read.machine <= nop();\n"
                        "        end\n"
                        "    end\n"
                        "\n"
                        "endmodule\n";
    }

private:
    std::size_t rest = 0;    // まだ出力していない命令の数

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
// 実行ファイルはヘッダを持たず，命令を先頭から1命令8バイトずつ並べたもの
// シェルはこれをメモリの後半(0x8000番地からの32KB．PC 0x4000からに対応する)の先頭へ書き写し，先頭の命令をCALLで呼び出す
class bin_writer : public machine_writer {
public:
    static constexpr std::size_t BASE_PC = 0x4000;              // メモリの後半の先頭の命令のPC
    static constexpr std::size_t CODE_AREA_SIZE = 32 * 1024;    // メモリの後半の大きさ(バイト．命令列を置くコード領域の大きさの上限)
    static constexpr std::size_t INSTRUCTION_SIZE = 8;          // 1命令の大きさ(バイト)

    // シェルはメモリの後半の先頭から実行ファイルを置く
    std::size_t base_pc() const override { return this->BASE_PC; }

    // メモリの後半に収まる命令数
    std::size_t max_instructions() const override { return this->CODE_AREA_SIZE / this->INSTRUCTION_SIZE; }

    // シェルがCALLで呼び出すため，mainのretはそのままシェルへ戻る
    bool main_ret_halts() const override { return false; }

    // ヘッダを持たない
    void write_header(std::size_t) override {}

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
