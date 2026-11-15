// Derived from Source/WebCore/platform/graphics/ImageBufferBackend.cpp@a77a158d4e2086fbe712e488ed147e8a54d44d3c
/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace {

bool IsWithinIntRange(float value) {
    return value > static_cast<float>(std::numeric_limits<int>::min()) && value < static_cast<float>(std::numeric_limits<int>::max());
}

}

extern "C" {

WebCore::IntSize APS5_VABI _ZN7WebCore18ImageBufferBackend20calculateBackendSizeERKNS_9FloatSizeEf(
    const WebCore::FloatSize& size, float resolutionScale) {
    WebCore::FloatSize scaledSize{std::ceil(resolutionScale * size.m_width), std::ceil(resolutionScale * size.m_height)};
    if (scaledSize.m_width <= 0.0f || scaledSize.m_height <= 0.0f) return {};
    if (!IsWithinIntRange(scaledSize.m_width) || !IsWithinIntRange(scaledSize.m_height)) return {};
    WebCore::IntSize backendSize;
    _ZN7WebCore7IntSizeC1ERKNS_9FloatSizeE(&backendSize, scaledSize);
    std::uint64_t bytesPerRow = 4ull * static_cast<unsigned>(backendSize.m_width);
    if (bytesPerRow > std::numeric_limits<unsigned>::max()) return {};
    if (static_cast<unsigned>(backendSize.m_height) * bytesPerRow > std::numeric_limits<unsigned>::max()) return {};
    return backendSize;
}

}
