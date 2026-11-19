// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/scrolling/ScrollingConstraints.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ScrollingConstraintsDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

using NKWebKit::WtfAbi::TextStream;

template <typename T, auto Write>
void DumpProperty(TextStream& stream, const char* name, const T& value) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, name);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    Write(stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

void DumpPoint(TextStream& stream, const char* name, const WebCore::FloatPoint& value) {
    DumpProperty<WebCore::FloatPoint, _ZN7WebCorelsERN3WTF10TextStreamERKNS_10FloatPointE>(stream, name, value);
}

void DumpRect(TextStream& stream, const char* name, const WebCore::FloatRect& value) {
    DumpProperty<WebCore::FloatRect, _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE>(stream, name, value);
}

void DumpSize(TextStream& stream, const char* name, const WebCore::FloatSize& value) {
    DumpProperty<WebCore::FloatSize, _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE>(stream, name, value);
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_27AbsolutePositionConstraintsE(TextStream& stream, const WebCore::AbsolutePositionConstraints& constraints) {
    DumpPoint(stream, "layer-position-at-last-layout", constraints.m_layerPositionAtLastLayout);
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_32FixedPositionViewportConstraintsE(TextStream& stream, const WebCore::FixedPositionViewportConstraints& constraints) {
    DumpRect(stream, "viewport-rect-at-last-layout", constraints.m_viewportRectAtLastLayout);
    DumpPoint(stream, "layer-position-at-last-layout", constraints.m_layerPositionAtLastLayout);
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_33StickyPositionViewportConstraintsE(TextStream& stream, const WebCore::StickyPositionViewportConstraints& constraints) {
    DumpSize(stream, "sticky-position-at-last-layout", constraints.m_stickyOffsetAtLastLayout);
    DumpPoint(stream, "layer-position-at-last-layout", constraints.m_layerPositionAtLastLayout);
    DumpRect(stream, "sticky-box-rect", constraints.m_stickyBoxRect);
    DumpRect(stream, "containing-block-rect", constraints.m_containingBlockRect);
    DumpRect(stream, "constraining-rect-at-last-layout", constraints.m_constrainingRectAtLastLayout);
    return stream;
}

}
