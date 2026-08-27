#pragma once

#include <math.h>
#include <string>
#include <map>

#include "asm2sv.hpp"

int str_find_first_of(const std::string &str, const char ch);     // 最初にchが出現する文字数を返す．なければ末尾までの文字数
std::string ltrim(const std::string &str);                        // 先頭の半角スペースを除去する
const std::string b2d(const std::string &bin);                    // 2進数を10進数に変換する
const std::string o2d(const std::string &oct);                    // 8進数を10進数に変換する
const std::string h2d(const std::string &hex);                    // 16進数を10進数に変換する
void replace(                                                     // 文字列のうち，パターンに当てはまる部分を全て置換する
    std::string &source, const std::string &pattern, const std::string &replacement
);

// 最初にchが出現する文字数を返す．なければ末尾までの文字数
int str_find_first_of(const std::string &str, const char ch) {
    std::size_t index = str.find_first_of(ch);

    if (index == std::string::npos) index = str.length();

    return static_cast<int>(index);
}

// 先頭の半角スペースを除去する
std::string ltrim(const std::string &str) {
    std::size_t i = 0;
    while (i < str.length() && str[i] == ' ') i++;
    return str.substr(i);
}

// b2d/o2d/h2d は現在どこからも呼ばれていない（未使用）．
// 基数付きの値は convert_arg で Verilog のサイズ付きリテラル（例 6'h2）へ書き直し，
// 実際の数値変換は SystemVerilog 側に任せているため，10進への変換関数は使っていない．
// 将来 C++ 側で実値出力したくなった場合に備えて残してある．

// 2進数を10進数に変換する
const std::string b2d(const std::string &bin) {
    const int len = bin.length();
    int dec = 0;

    for (int i = 0; i < len; i++) {
        // 2進数として不正な値
        if (bin[i] != '0' && bin[i] != '1') throw "asm syntax error: fail number '" + bin + "'";

        // その桁の数字を足す
        dec += (bin[i] - '0') * std::pow(2, len - i - 1);
    }

    return std::to_string(dec);
}

// 8進数を10進数に変換する
const std::string o2d(const std::string &oct) {
    const int len = oct.length();
    int dec = 0;

    for (int i = 0; i < len; i++) {
        // 8進数として不正な値
        if (oct[i] < '0' || oct[i] > '7') throw "asm syntax error: fail number '" + oct + "'";

        // その桁の数字を足す
        dec += (oct[i] - '0') * std::pow(8, len - i - 1);
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

// 文字列のうち，パターンに当てはまる部分を全て置換する
void replace(
    std::string &source, const std::string &pattern, const std::string &replacement
) {
    std::size_t patternLength = pattern.length();
    std::size_t position = 0;

    while ((position = source.find(pattern)) != std::string::npos) {
        source.replace(position, patternLength, replacement);
    }
}
