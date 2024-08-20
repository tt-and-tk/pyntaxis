#pragma once

#include <string>
#include <map>

#include "asm2bin.hpp"

int str_find_first_of(const std::string &str, const char ch);     // 最初にchが出現する文字数を返す．なければ末尾までの文字数
std::string convert_arg(                                          // 引数を加工して返す
    const std::map<std::string, std::size_t> &functions, const std::string &arg
);

// 最初にchが出現する文字数を返す．なければ末尾までの文字数
int str_find_first_of(const std::string &str, const char ch) {
    int index = str.find_first_of(ch);

    if (index == std::string::npos) index = str.length();

    return index;
}

// 引数を加工して返す
std::string convert_arg(const std::map<std::string, std::size_t> &functions, const std::string &arg) {
    std::string converted_arg = arg;

    // 引数が関数名なら
    if (functions.find(arg) != functions.end()) {
        std::size_t index = functions.at(arg);  // その関数の先頭行数

        // 先頭行数が未セットなら
        if (index == std::string::npos) {
            throw "asm syntax error: function not found " + arg;
        }

        return std::to_string(index);
    }

    // 引数が番地なら
    if (arg[0] == 'r') {

    }
}
