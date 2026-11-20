/*
 * Copyright (C) 2011-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Derived from Source/JavaScriptCore/bytecode/SpeculatedType.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/SpeculatedTypeExports.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace {

constexpr std::uint64_t SpecInt32AsInt52 = 1ull << 34;
constexpr std::uint64_t SpecNonInt32AsInt52 = 1ull << 35;
constexpr std::uint64_t SpecNonIntAsDouble = 1ull << 37;
constexpr std::uint64_t SpecDoublePureNaN = 1ull << 38;
constexpr std::uint64_t SpecBoolean = 1ull << 40;
constexpr std::uint64_t SpecOther = 1ull << 41;
constexpr std::uint64_t SpecEmpty = 1ull << 42;

constexpr std::uint64_t NumberTag = 0xfffe000000000000ull;
constexpr std::uint64_t DoubleEncodeOffset = 1ull << 49;
constexpr std::uint64_t NotCellMask = NumberTag | 0x2;
constexpr std::uint64_t ValueFalse = 0x6;

bool IsInt52(double number) {
    constexpr double limit = 2251799813685248.0;
    return number >= -limit && number < limit && std::trunc(number) == number && !(number == 0 && std::signbit(number));
}

}

extern "C" {

std::uint64_t APS5_VABI _ZN3JSC30int52AwareSpeculationFromValueENS_7JSValueE(std::uint64_t value) {
    if (!value) return SpecEmpty;
    if ((value & NumberTag) == NumberTag) return SpecInt32AsInt52;
    if (value & NumberTag) {
        const std::uint64_t bits = value - DoubleEncodeOffset;
        double number;
        std::memcpy(&number, &bits, sizeof(number));
        if (IsInt52(number)) {
            const auto integer = static_cast<std::int64_t>(number);
            return integer == static_cast<std::int32_t>(integer) ? SpecInt32AsInt52 : SpecNonInt32AsInt52;
        }
        return number != number ? SpecDoublePureNaN : SpecNonIntAsDouble;
    }
    if (!(value & NotCellMask)) throw std::invalid_argument("JSC::int52AwareSpeculationFromValue: cell values are not supported");
    if ((value & ~1ull) == ValueFalse) return SpecBoolean;
    return SpecOther;
}

}
