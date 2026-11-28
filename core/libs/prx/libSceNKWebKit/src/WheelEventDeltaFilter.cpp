// Derived from Source/WebCore/page/WheelEventDeltaFilter.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WheelEventDeltaFilterExports.hpp"

#include <stdexcept>

namespace {

const NKWebKit::WheelAbi::WheelEventDeltaFilter& Checked(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self) {
    if (!self) throw std::invalid_argument("null WebCore::WheelEventDeltaFilter");
    return *self;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self) {
    return Checked(self).isFilteringDeltas;
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self) {
    return Checked(self).currentFilteredDelta;
}

WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self) {
    return Checked(self).currentFilteredVelocity;
}

}
