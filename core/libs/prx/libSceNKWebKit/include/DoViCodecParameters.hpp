/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
#include <optional>
#include <type_traits>

namespace WebCore {

struct DoViParameters {
    enum class Codec { AVC1, AVC3, HEV1, HVC1 } codec { Codec::HVC1 };
    uint16_t bitstreamProfileID { 0 };
    uint16_t bitstreamLevelID { 0 };
};

struct DoViOptionalAbi {
    DoViParameters value;
    bool engaged;
};

static_assert(offsetof(DoViOptionalAbi, engaged) == 8);
static_assert(sizeof(DoViOptionalAbi) == sizeof(std::optional<DoViParameters>));
static_assert(alignof(DoViOptionalAbi) == alignof(std::optional<DoViParameters>));
static_assert(sizeof(DoViParameters::Codec) == 4);
static_assert(sizeof(DoViParameters) == 8);
static_assert(alignof(DoViParameters) == 4);
static_assert(offsetof(DoViParameters, codec) == 0);
static_assert(offsetof(DoViParameters, bitstreamProfileID) == 4);
static_assert(offsetof(DoViParameters, bitstreamLevelID) == 6);
static_assert(sizeof(std::optional<DoViParameters>) == 12);
static_assert(alignof(std::optional<DoViParameters>) == 4);
static_assert(std::is_trivially_copyable_v<std::optional<DoViParameters>>);

}  // namespace WebCore

extern "C" std::optional<WebCore::DoViParameters> APS5_VABI _ZN7WebCore24parseDoViCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& input);
