/* Derived from Source/WebCore/page/scrolling/ScrollingConstraints.cpp@c55364d53f971f63a709976f168d932d558b8909 */
/* Derived from Source/WebCore/page/scrolling/ScrollingConstraints.h@c55364d53f971f63a709976f168d932d558b8909 */
/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

namespace {

using NKWebKit::WtfAbi::TextStream;

template<class TValue, class TWrite>
void DumpProperty(TextStream& stream, const char* name, const TValue& value, TWrite write) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, name);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    write(stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

}

extern "C" {

TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE(TextStream& stream, const WebCore::AbsolutePositionConstraints& constraints) {
    DumpProperty(stream, "layer-position-at-last-layout", constraints.m_layerPositionAtLastLayout, _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE);
    return stream;
}

TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(TextStream& stream, const WebCore::FixedPositionViewportConstraints& constraints) {
    DumpProperty(stream, "viewport-rect-at-last-layout", constraints.m_viewportRectAtLastLayout, _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE);
    DumpProperty(stream, "layer-position-at-last-layout", constraints.m_layerPositionAtLastLayout, _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE);
    return stream;
}

}
