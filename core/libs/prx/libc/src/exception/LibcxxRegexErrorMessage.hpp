//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Diagnostic text adapted from https://github.com/llvm/llvm-project/blob/3b5b5c1ec4a3095ab096dd780e84d7ab81f3d7ff/libcxx/src/regex.cpp

#pragma once

#include <array>
#include <cstdint>

namespace LibcException {

inline constexpr const char* LibcxxRegexErrorMessage(std::int32_t code) {
    constexpr std::array messages{
        "Unknown error type",
        "The expression contained an invalid collating element name.",
        "The expression contained an invalid character class name.",
        "The expression contained an invalid escaped character, or a "
        "trailing escape.",
        "The expression contained an invalid back reference.",
        "The expression contained mismatched [ and ].",
        "The expression contained mismatched ( and ).",
        "The expression contained mismatched { and }.",
        "The expression contained an invalid range in a {} expression.",
        "The expression contained an invalid character range, "
        "such as [b-a] in most encodings.",
        "There was insufficient memory to convert the expression into "
        "a finite state machine.",
        "One of *?+{ was not preceded by a valid regular expression.",
        "The complexity of an attempted match against a regular "
        "expression exceeded a pre-set level.",
        "There was insufficient memory to determine whether the regular "
        "expression could match the specified character sequence.",
        "An invalid regex grammar has been requested.",
        "An empty regex is not allowed in the POSIX grammar.",
        "Unknown error type",
        "The parser did not consume the entire regular expression.",
    };
    const auto index = static_cast<std::uint32_t>(code);
    return index < messages.size() ? messages[index] : messages.front();
}

}
