#pragma once

#include <math.h>
#include <string>
#include <map>

#include "asm2bin.hpp"

int str_find_first_of(const std::string &str, const char ch);     // 最初にchが出現する文字数を返す．なければ末尾までの文字数
const std::string b2d(const std::string &bin);                    // 2進数を10進数に変換する
const std::string o2d(const std::string &oct);                    // 8進数を10進数に変換する
const std::string h2d(const std::string &hex);                    // 16進数を10進数に変換する

// 最初にchが出現する文字数を返す．なければ末尾までの文字数
int str_find_first_of(const std::string &str, const char ch) {
    int index = str.find_first_of(ch);

    if (index == std::string::npos) index = str.length();

    return index;
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
