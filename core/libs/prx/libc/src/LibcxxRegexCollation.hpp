//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp

#pragma once

#include <algorithm>
#include <string_view>

namespace LibcxxRegex {

struct CollatingElement {
    std::string_view name;
    char character;
};

inline constexpr CollatingElement CollatingElements[] = {
    {"NUL", '\0'},
    {"alert", '\a'},
    {"ampersand", '&'},
    {"apostrophe", '\''},
    {"asterisk", '*'},
    {"backslash", '\\'},
    {"backspace", '\b'},
    {"carriage-return", '\r'},
    {"circumflex", '^'},
    {"circumflex-accent", '^'},
    {"colon", ':'},
    {"comma", ','},
    {"commercial-at", '@'},
    {"dollar-sign", '$'},
    {"eight", '8'},
    {"equals-sign", '='},
    {"exclamation-mark", '!'},
    {"five", '5'},
    {"form-feed", '\f'},
    {"four", '4'},
    {"full-stop", '.'},
    {"grave-accent", '`'},
    {"greater-than-sign", '>'},
    {"hyphen", '-'},
    {"hyphen-minus", '-'},
    {"left-brace", '{'},
    {"left-curly-bracket", '{'},
    {"left-parenthesis", '('},
    {"left-square-bracket", '['},
    {"less-than-sign", '<'},
    {"low-line", '_'},
    {"newline", '\n'},
    {"nine", '9'},
    {"number-sign", '#'},
    {"one", '1'},
    {"percent-sign", '%'},
    {"period", '.'},
    {"plus-sign", '+'},
    {"question-mark", '?'},
    {"quotation-mark", '"'},
    {"reverse-solidus", '\\'},
    {"right-brace", '}'},
    {"right-curly-bracket", '}'},
    {"right-parenthesis", ')'},
    {"right-square-bracket", ']'},
    {"semicolon", ';'},
    {"seven", '7'},
    {"six", '6'},
    {"slash", '/'},
    {"solidus", '/'},
    {"space", ' '},
    {"tab", '\t'},
    {"three", '3'},
    {"tilde", '~'},
    {"two", '2'},
    {"underscore", '_'},
    {"vertical-line", '|'},
    {"vertical-tab", '\v'},
    {"zero", '0'},
};
static_assert(std::ranges::is_sorted(CollatingElements, {}, &CollatingElement::name));
}
