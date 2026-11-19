// Derived from Source/WebCore/page/ActivityState.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ActivityStateExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <utility>

namespace {

constexpr std::pair<std::uint32_t, const char*> kActivityStateNames[] = {
    {1u << 0, "active window"},
    {1u << 1, "focused"},
    {1u << 2, "visible"},
    {1u << 3, "visible or occluded"},
    {1u << 4, "in-window"},
    {1u << 5, "visually idle"},
    {1u << 6, "audible"},
    {1u << 7, "loading"},
    {1u << 8, "capturing media"},
};

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS0_9OptionSetINS_13ActivityState4FlagEEE(NKWebKit::WtfAbi::TextStream& stream, NKWebKit::WebCoreAbi::ActivityStateFlags flags) {
    bool didAppend = false;
    for (const auto& [flag, message] : kActivityStateNames) {
        if (!(flags.storage & flag)) continue;
        if (didAppend) _ZN3WTF10TextStreamlsEPKc(&stream, ", ");
        _ZN3WTF10TextStreamlsEPKc(&stream, message);
        didAppend = true;
    }
    return stream;
}

}
