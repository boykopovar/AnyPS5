/* Derived from Source/WebCore/rendering/EventRegion.cpp@c55364d53f971f63a709976f168d932d558b8909 */
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

#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(NKWebKit::WtfAbi::TextStream& stream, WebCore::TouchAction touchAction) {
    switch (touchAction) {
    case WebCore::TouchAction::None:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "none");
    case WebCore::TouchAction::Manipulation:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "manipulation");
    case WebCore::TouchAction::PanX:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "pan-x");
    case WebCore::TouchAction::PanY:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "pan-y");
    case WebCore::TouchAction::PinchZoom:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "pinch-zoom");
    case WebCore::TouchAction::Auto:
        return _ZN3WTF10TextStreamlsEPKc(&stream, "auto");
    }
    return stream;
}

}
