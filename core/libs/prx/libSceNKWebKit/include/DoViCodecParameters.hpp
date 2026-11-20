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

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace WebCore {

struct DoViParameterSet {
    NKWebKit::WtfAbi::String codecName;
    unsigned short bitstreamProfileID { 0 };
    unsigned short bitstreamLevelID { 0 };
};

struct DoViOptionalParameterSet {
    bool engaged;
    union {
        unsigned char dummy;
        DoViParameterSet value;
    };
    DoViOptionalParameterSet() : engaged(false), dummy(0) {}
    explicit DoViOptionalParameterSet(DoViParameterSet&& parameters) : engaged(true), value(std::move(parameters)) {}
    DoViOptionalParameterSet(const DoViOptionalParameterSet&) = delete;
    DoViOptionalParameterSet(DoViOptionalParameterSet&& other) noexcept : engaged(other.engaged), dummy(0) {
        if (engaged) new (&value) DoViParameterSet(std::move(other.value));
    }
    ~DoViOptionalParameterSet() {
        if (engaged) value.~DoViParameterSet();
    }
};

static_assert(sizeof(DoViParameterSet) == 16);
static_assert(offsetof(DoViParameterSet, codecName) == 0);
static_assert(offsetof(DoViParameterSet, bitstreamProfileID) == 8);
static_assert(offsetof(DoViParameterSet, bitstreamLevelID) == 10);
static_assert(sizeof(DoViOptionalParameterSet) == 24);
static_assert(alignof(DoViOptionalParameterSet) == 8);
static_assert(offsetof(DoViOptionalParameterSet, engaged) == 0);
static_assert(offsetof(DoViOptionalParameterSet, value) == 8);
static_assert(!std::is_trivially_copyable_v<DoViOptionalParameterSet>);
static_assert(!std::is_trivially_destructible_v<DoViOptionalParameterSet>);

}  // namespace WebCore

extern "C" WebCore::DoViOptionalParameterSet APS5_VABI _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& input);
