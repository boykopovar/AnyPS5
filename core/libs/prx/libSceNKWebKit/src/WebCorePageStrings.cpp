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
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * Derived from Source/WebCore/page/DisabledAdaptations.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/page/PerformanceLoggingClient.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * Derived from Source/WebCore/page/scrolling/ScrollingCoordinatorTypes.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;

enum class SynchronousScrollingReason : std::uint8_t {
    ForcedOnMainThread = 1 << 0,
    HasViewportConstrainedObjectsWithoutSupportingFixedLayers = 1 << 1,
    HasNonLayerViewportConstrainedObjects = 1 << 2,
    IsImageDocument = 1 << 3,
    HasSlowRepaintObjects = 1 << 4,
    DescendantScrollersHaveSynchronousScrolling = 1 << 5,
};

bool Contains(std::uint8_t reasons, SynchronousScrollingReason reason) {
    return reasons & static_cast<std::uint8_t>(reason);
}

String Create(const std::string& text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

}  // namespace

extern "C" {

String APS5_VABI _ZN7WebCore19watchAdaptationNameEv() {
    return Create("watch");
}

String APS5_VABI _ZN7WebCore24PerformanceLoggingClient35synchronousScrollingReasonsAsStringEN3WTF9OptionSetINS_26SynchronousScrollingReasonEEE(std::uint8_t reasons) {
    if (!reasons) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    std::string string;
    if (Contains(reasons, SynchronousScrollingReason::ForcedOnMainThread)) string += "forced,";
    if (Contains(reasons, SynchronousScrollingReason::HasSlowRepaintObjects)) string += "slow-repaint objects,";
    if (Contains(reasons, SynchronousScrollingReason::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)) string += "viewport-constrained objects,";
    if (Contains(reasons, SynchronousScrollingReason::HasNonLayerViewportConstrainedObjects)) string += "non-layer viewport-constrained objects,";
    if (Contains(reasons, SynchronousScrollingReason::IsImageDocument)) string += "image document,";
    if (!string.empty()) string.pop_back();
    return Create(string);
}

}
