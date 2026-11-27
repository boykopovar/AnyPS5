// Derived from Source/WebCore/platform/graphics/HEVCUtilities.h@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace WebCore {

struct HEVCParameterSet {
    NKWebKit::WtfAbi::String codecName;
    unsigned short generalProfileSpace { 0 };
    unsigned short generalProfileIDC { 0 };
    uint32_t generalProfileCompatibilityFlags { 0 };
    bool generalTierFlag { false };
    unsigned short generalLevelIDC { 0 };
    NKWebKit::WtfAbi::Vector<unsigned short> constraintFlags;
};

struct HEVCOptionalParameterSet {
    bool engaged;
    union {
        unsigned char dummy;
        HEVCParameterSet value;
    };
    HEVCOptionalParameterSet() : engaged(false), dummy(0) {}
    explicit HEVCOptionalParameterSet(HEVCParameterSet&& parameters) : engaged(true), value(std::move(parameters)) {}
    HEVCOptionalParameterSet(const HEVCOptionalParameterSet&) = delete;
    HEVCOptionalParameterSet(HEVCOptionalParameterSet&& other) noexcept : engaged(other.engaged), dummy(0) {
        if (engaged) new (&value) HEVCParameterSet(std::move(other.value));
    }
    ~HEVCOptionalParameterSet() {
        if (engaged) value.~HEVCParameterSet();
    }
};

static_assert(sizeof(HEVCParameterSet) == 40);
static_assert(alignof(HEVCParameterSet) == 8);
static_assert(offsetof(HEVCParameterSet, codecName) == 0);
static_assert(offsetof(HEVCParameterSet, generalProfileSpace) == 8);
static_assert(offsetof(HEVCParameterSet, generalProfileIDC) == 10);
static_assert(offsetof(HEVCParameterSet, generalProfileCompatibilityFlags) == 12);
static_assert(offsetof(HEVCParameterSet, generalTierFlag) == 16);
static_assert(offsetof(HEVCParameterSet, generalLevelIDC) == 18);
static_assert(offsetof(HEVCParameterSet, constraintFlags) == 24);
static_assert(sizeof(NKWebKit::WtfAbi::Vector<unsigned short>) == 16);
static_assert(sizeof(HEVCOptionalParameterSet) == 48);
static_assert(offsetof(HEVCOptionalParameterSet, engaged) == 0);
static_assert(offsetof(HEVCOptionalParameterSet, value) == 8);
static_assert(!std::is_trivially_copyable_v<HEVCOptionalParameterSet>);
static_assert(!std::is_trivially_destructible_v<HEVCOptionalParameterSet>);

}

extern "C" {
WebCore::HEVCOptionalParameterSet APS5_VABI _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& string);
}
