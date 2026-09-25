#pragma once

#include <cstdint>
#include <string>
#include <map>

#include "asm2mc.hpp"

int str_find_first_of(const std::string &str, const char ch);     // 最初にchが出現する文字数を返す．なければ末尾までの文字数
std::string ltrim(const std::string &str);                        // 先頭の半角スペースを除去する
std::string strip_comment(const std::string &line);               // コメント(;以降)を除去する
std::uint64_t notation2value(const std::string &notation);        // 数値表記を値にする

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

// コメント(;以降)を除去する
std::string strip_comment(const std::string &line) {
    return line.substr(0, line.find(';'));
}

// 数値表記を値にする
// 末尾のb/o/hがあればその基数，なければ10進として読む(表記の妥当性はis_number_notationで確かめておく)
// 64ビットに収まらない上位の桁は捨てる(出力するフィールドの幅に切り詰めるのは呼び出し側)
std::uint64_t notation2value(const std::string &notation) {
    // 末尾の文字から基数と桁の範囲を決める
    const char last = notation[notation.length() - 1];                                  // 基数接尾辞の候補
    const int base = (last == 'b') ? 2 : (last == 'o') ? 8 : (last == 'h') ? 16 : 10;   // 基数
    const std::string digits = (base == 10) ? notation : notation.substr(0, notation.length() - 1);  // 桁の並び

    // 上の桁から順に基数を掛けて足していく
    std::uint64_t value = 0;
    for (const char digit : digits) {
        // 16進数ではa〜f(A〜F)も桁として使える
        const int digit_value = ('0' <= digit && digit <= '9') ? digit - '0'
                              : ('a' <= digit && digit <= 'f') ? digit - 'a' + 10
                              : digit - 'A' + 10;
        value = value * base + digit_value;
    }

    return value;
}
