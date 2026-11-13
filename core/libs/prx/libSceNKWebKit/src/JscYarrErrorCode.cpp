// Derived from Source/JavaScriptCore/yarr/YarrErrorCode.cpp (PS5-WebKit-11.x 80f77e94c63e)
/*
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>

namespace {

#define REGEXP_ERROR_PREFIX "Invalid regular expression: "

constexpr std::array<const char*, 32> ErrorMessages{
    nullptr,
    REGEXP_ERROR_PREFIX "regular expression too large",
    REGEXP_ERROR_PREFIX "numbers out of order in {} quantifier",
    REGEXP_ERROR_PREFIX "nothing to repeat",
    REGEXP_ERROR_PREFIX "number too large in {} quantifier",
    REGEXP_ERROR_PREFIX "incomplete {} quantifier for Unicode pattern",
    REGEXP_ERROR_PREFIX "invalid quantifier",
    REGEXP_ERROR_PREFIX "missing )",
    REGEXP_ERROR_PREFIX "unmatched ] or } bracket for Unicode pattern",
    REGEXP_ERROR_PREFIX "unmatched parentheses",
    REGEXP_ERROR_PREFIX "unrecognized character after (?",
    REGEXP_ERROR_PREFIX "invalid group specifier name",
    REGEXP_ERROR_PREFIX "duplicate group specifier name",
    REGEXP_ERROR_PREFIX "missing terminating ] for character class",
    REGEXP_ERROR_PREFIX "range out of order in character class",
    REGEXP_ERROR_PREFIX "invalid range in character class for Unicode pattern",
    REGEXP_ERROR_PREFIX "missing terminating } for class string disjunction",
    REGEXP_ERROR_PREFIX "\\ at end of pattern",
    REGEXP_ERROR_PREFIX "invalid Unicode \\u escape",
    REGEXP_ERROR_PREFIX "invalid Unicode code point \\u{} escape",
    REGEXP_ERROR_PREFIX "invalid backreference for Unicode pattern",
    REGEXP_ERROR_PREFIX "invalid \\k<> named backreference",
    REGEXP_ERROR_PREFIX "invalid escaped character for Unicode pattern",
    REGEXP_ERROR_PREFIX "invalid octal escape for Unicode pattern",
    REGEXP_ERROR_PREFIX "invalid \\c escape for Unicode pattern",
    REGEXP_ERROR_PREFIX "invalid property expression",
    REGEXP_ERROR_PREFIX "too many nested disjunctions",
    REGEXP_ERROR_PREFIX "pattern exceeds string length limits",
    REGEXP_ERROR_PREFIX "invalid flags",
    REGEXP_ERROR_PREFIX "invalid operation in class set",
    REGEXP_ERROR_PREFIX "negated class set may contain strings",
    REGEXP_ERROR_PREFIX "invalid class set character",
};

#undef REGEXP_ERROR_PREFIX

static_assert(ErrorMessages.size() == static_cast<std::size_t>(NKWebKit::YarrAbi::ErrorCode::InvalidClassSetCharacter) + 1);

}

extern "C" NKWebKit::HttpAbi::ASCIILiteral APS5_VABI _ZN3JSC4Yarr12errorMessageENS0_9ErrorCodeE(NKWebKit::YarrAbi::ErrorCode error) {
    const auto index = static_cast<std::size_t>(error);
    if (index >= ErrorMessages.size()) throw std::out_of_range("invalid Yarr ErrorCode");
    return {ErrorMessages[index]};
}
