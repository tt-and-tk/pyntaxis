#pragma once

#include <math.h>
#include <string>
#include <map>

#include "asm2bin.hpp"

int str_find_first_of(const std::string &str, const char ch);     // 最初にchが出現する文字数を返す．なければ末尾までの文字数
std::string convert_arg(                                          // 引数を加工して返す
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const command_arg_t &command_arg, const int arg_num
);
const std::string b2d(const std::string &bin);                    // 2進数を10進数に変換する
const std::string o2d(const std::string &oct);                    // 8進数を10進数に変換する
const std::string h2d(const std::string &hex);                    // 16進数を10進数に変換する

// 最初にchが出現する文字数を返す．なければ末尾までの文字数
int str_find_first_of(const std::string &str, const char ch) {
    int index = str.find_first_of(ch);

    if (index == std::string::npos) index = str.length();

    return index;
}

// 引数を加工して返す
std::string convert_arg(
    const std::map<std::string, std::size_t> &functions,
    const std::string &arg, const command_arg_t &command_arg, const int arg_num
) {
    std::string converted_arg = arg;   // 引数は加工できないので，加工用の変数を用意

    // 引数が関数名なら
    if (functions.find(converted_arg) != functions.end()) {
        std::size_t index = functions.at(converted_arg);  // その関数の先頭行数

        // 先頭行数が未セットなら
        if (index == std::string::npos) {
            throw "asm syntax error: function not found " + arg;
        }
        // callをjmpに変換してるので，引数タイプはチェックしない

        return std::to_string(index);
    }

    // 引数がレジスタなら
    if (converted_arg[0] == 'r') {
        // 引数タイプが違うなら
        if (command_arg.args[arg_num] != arg_t::REGISTER) {
            throw "asm syntax error: arg register address fail '" + arg + "'";
        }

        converted_arg = arg.substr(1);
    }
    else {
        // 引数タイプがマスクまたは生の値ではないなら
        if (command_arg.args[arg_num] != arg_t::MASK && command_arg.args[arg_num] != arg_t::RAW_DATA) {
            throw "asm syntax error: arg mask or raw data fail '" + arg + "'";
        }
    }

    // 引数が十進数表記ではないなら
    const char last = converted_arg[converted_arg.length() - 1];
    if (last < '0' || last > '9') {
        switch (last) {
            case 'b':  // 2進数
                converted_arg = b2d(converted_arg.substr(0, converted_arg.length() - 1));
                break;
            case 'o':  // 8進数
                converted_arg = o2d(converted_arg.substr(0, converted_arg.length() - 1));
                break;
            case 'h':  // 16進数
                converted_arg = h2d(converted_arg.substr(0, converted_arg.length() - 1));
                break;
            default:
                throw std::string("asm syntax error: fail base number '") + last + "'";
        }
    }

    // エラーが起きていれば
    if (converted_arg.empty()) throw "asm syntax error: fail arg '" + arg + "'";

    // イミディエイトデータを使用するなら
    if (command_arg.args[arg_num] == arg_t::RAW_DATA) {
        converted_arg = "33'h1_0000_0000 + " + converted_arg;
    }

    // 加工した引数を返す
    return converted_arg;
}

// 2進数を10進数に変換する
const std::string b2d(const std::string &bin) {
    const int len = bin.length();
    int dec = 0;

    for (int i = 0; i < len; i++) {
        // 2進数として不正な値
        if (bin[i] != '0' && bin[i] != '1') throw "asm syntax error: fail number '" + bin + "'";

        // その桁の数字を足す
        dec += (bin[i] - '0') * std::pow(2, len - i);
    }

    return std::to_string(dec);
}

// 8進数を10進数に変換する
const std::string o2d(const std::string &oct) {
    const int len = oct.length();
    int dec = 0;

    for (int i = 0; i < len; i++) {
        // 8進数として不正な値
        if (oct[i] < '0' || oct[i] > '8') throw "asm syntax error: fail number '" + oct + "'";

        // その桁の数字を足す
        dec += (oct[i] - '0') * std::pow(8, len - i);
    }

    return std::to_string(dec);
}

// 16進数を10進数に変換する
const std::string h2d(const std::string &hex) {
    const int len = hex.length();
    int dec = 0;

    for (int i = 0; i < len; i++) {
        const char c = hex[i];

        // 0から9
        if (c >= '0' && c <= '9') {
            dec += (c - '0') * std::pow(16, len - i - 1);
        }
        else if (c >= 'a' && c <= 'f') {
            dec += (c - 'a' + 10) * std::pow(16, len - i - 1);
        }
        else if (c >= 'A' && c <= 'F') {
            dec += (c - 'A' + 10) * std::pow(16, len - i - 1);
        }
        else {
            throw "asm syntax error: fail number '" + hex + "'";
        }
    }

    return std::to_string(dec);
}
