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

/*
 *  Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <optional>

namespace NKWebKit::HEVCAbi {

struct ConstraintFlags {
    unsigned char* buffer = inlineStorage;
    unsigned capacity = 6;
    unsigned size = 6;
    unsigned char inlineStorage[6] {};

    ConstraintFlags() = default;
    ConstraintFlags(const ConstraintFlags& other) : size(other.size) {
        std::copy_n(other.buffer, size, inlineStorage);
    }
    ConstraintFlags(ConstraintFlags&& other) noexcept : ConstraintFlags(other) { other.size = 0; }
    ConstraintFlags& operator=(const ConstraintFlags& other) {
        std::copy_n(other.buffer, other.size, inlineStorage);
        size = other.size;
        return *this;
    }
    ~ConstraintFlags() {}
};

struct HEVCParameters {
    enum class Codec { Hev1, Hvc1 } codec { Codec::Hvc1 };
    uint16_t generalProfileSpace { 0 };
    uint16_t generalProfileIDC { 0 };
    uint32_t generalProfileCompatibilityFlags { 0 };
    uint8_t generalTierFlag { 0 };
    ConstraintFlags generalConstraintIndicatorFlags;
    uint16_t generalLevelIDC { 0 };
};

static_assert(sizeof(ConstraintFlags) == 24);
static_assert(alignof(ConstraintFlags) == 8);
static_assert(offsetof(ConstraintFlags, buffer) == 0);
static_assert(offsetof(ConstraintFlags, capacity) == 8);
static_assert(offsetof(ConstraintFlags, size) == 12);
static_assert(offsetof(ConstraintFlags, inlineStorage) == 16);
static_assert(sizeof(HEVCParameters::Codec) == 4);
static_assert(sizeof(HEVCParameters) == 48);
static_assert(alignof(HEVCParameters) == 8);
static_assert(offsetof(HEVCParameters, codec) == 0);
static_assert(offsetof(HEVCParameters, generalProfileSpace) == 4);
static_assert(offsetof(HEVCParameters, generalProfileIDC) == 6);
static_assert(offsetof(HEVCParameters, generalProfileCompatibilityFlags) == 8);
static_assert(offsetof(HEVCParameters, generalTierFlag) == 12);
static_assert(offsetof(HEVCParameters, generalConstraintIndicatorFlags) == 16);
static_assert(offsetof(HEVCParameters, generalLevelIDC) == 40);
static_assert(sizeof(std::optional<HEVCParameters>) == 56);
static_assert(!std::is_trivially_copyable_v<std::optional<HEVCParameters>>);
static_assert(!std::is_trivially_destructible_v<std::optional<HEVCParameters>>);

}

extern "C" {
std::optional<NKWebKit::HEVCAbi::HEVCParameters> APS5_VABI _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& string);
}
