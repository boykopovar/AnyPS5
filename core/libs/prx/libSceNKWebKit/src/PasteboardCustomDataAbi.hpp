// Adapted from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/PasteboardCustomData.h
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/src/SharedBufferAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct PasteboardPlatformData {
    union {
        WtfAbi::StringImpl* string;
        SharedBuffer* buffer;
    };
    signed char index;

    PasteboardPlatformData();
    PasteboardPlatformData(const PasteboardPlatformData& other);
    PasteboardPlatformData(PasteboardPlatformData&& other) noexcept;
    ~PasteboardPlatformData();
    PasteboardPlatformData& operator=(PasteboardPlatformData other) noexcept;
};

struct PasteboardEntry {
    WtfAbi::String type;
    WtfAbi::String customData;
    PasteboardPlatformData platformData;
};

struct PasteboardCustomData {
    WtfAbi::String origin;
    WtfAbi::Vector<PasteboardEntry> data;
};

static_assert(sizeof(PasteboardPlatformData) == 16);
static_assert(offsetof(PasteboardPlatformData, index) == 8);
static_assert(sizeof(PasteboardEntry) == 32);
static_assert(offsetof(PasteboardEntry, type) == 0);
static_assert(offsetof(PasteboardEntry, customData) == 8);
static_assert(offsetof(PasteboardEntry, platformData) == 16);
static_assert(sizeof(PasteboardCustomData) == 24);
static_assert(alignof(PasteboardCustomData) == 8);
static_assert(offsetof(PasteboardCustomData, data) == 8);
static_assert(!std::is_trivially_destructible_v<PasteboardCustomData>);

}  // namespace NKWebKit::WebCoreAbi
