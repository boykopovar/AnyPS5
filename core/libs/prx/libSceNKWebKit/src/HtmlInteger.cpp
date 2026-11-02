/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/html/parser/HTMLParserIdioms.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <limits>
#include <stdexcept>

namespace {

using NKWebKit::WtfAbi::HTMLIntegerParsingError;
using NKWebKit::WtfAbi::HTMLNonNegativeIntegerResult;

HTMLNonNegativeIntegerResult Error(HTMLIntegerParsingError error) {
    HTMLNonNegativeIntegerResult result{};
    result.error = error;
    result.hasValue = false;
    return result;
}

template<typename Character> HTMLNonNegativeIntegerResult Parse(const Character* position, const Character* end) {
    while (position < end && (*position == ' ' || *position == '\t' || *position == '\n' || *position == '\r' || *position == '\f'))
        ++position;
    if (position == end)
        return Error(HTMLIntegerParsingError::Other);

    bool isNegative = false;
    if (*position == '-') {
        isNegative = true;
        ++position;
    } else if (*position == '+')
        ++position;

    if (position == end || *position < '0' || *position > '9')
        return Error(HTMLIntegerParsingError::Other);

    constexpr unsigned intMax = std::numeric_limits<int>::max();
    constexpr unsigned base = 10;
    constexpr unsigned maxMultiplier = intMax / base;
    unsigned result = 0;
    do {
        unsigned digitValue = *position - '0';
        if (result > maxMultiplier || (result == maxMultiplier && digitValue > (intMax % base) + isNegative))
            return Error(isNegative ? HTMLIntegerParsingError::NegativeOverflow : HTMLIntegerParsingError::PositiveOverflow);
        result = base * result + digitValue;
        ++position;
    } while (position < end && *position >= '0' && *position <= '9');

    if (isNegative && result)
        return Error(HTMLIntegerParsingError::NegativeOverflow);
    return {{result}, true};
}

}  // namespace

extern "C" NKWebKit::WtfAbi::HTMLNonNegativeIntegerResult APS5_VABI _ZN7WebCore27parseHTMLNonNegativeIntegerEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView input) {
    if (!input.length)
        return Error(HTMLIntegerParsingError::Other);
    if (!input.data)
        throw std::runtime_error("null WTF::StringView characters");
    if (input.is8Bit) {
        auto* start = static_cast<const unsigned char*>(input.data);
        return Parse(start, start + input.length);
    }
    auto* start = static_cast<const char16_t*>(input.data);
    return Parse(start, start + input.length);
}
