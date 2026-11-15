// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/rendering/EventRegion.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/FloatRoundedRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/RectEdges.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/Region.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/scrolling/ScrollingCoordinatorTypes.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/ScrollTypes.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
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

/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>

namespace {

using NKWebKit::WtfAbi::TextStream;

TextStream& Text(TextStream& stream, const char* text) {
    return _ZN3WTF10TextStreamlsEPKc(&stream, text);
}

TextStream& Int(TextStream& stream, int value) {
    return _ZN3WTF10TextStreamlsEi(&stream, value);
}

TextStream& Float(TextStream& stream, float value) {
    return _ZN3WTF10TextStreamlsEf(&stream, value);
}

template<typename WriteValue> void DumpProperty(TextStream& stream, const char* name, WriteValue writeValue) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Text(stream, name);
    Text(stream, " ");
    writeValue();
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

struct IndentScope {
    TextStream& stream;
    explicit IndentScope(TextStream& s) : stream(s) { ++stream.indent; }
    ~IndentScope() { --stream.indent; }
};

TextStream& Write(TextStream& stream, WebCore::ScrollElasticity value) {
    using WebCore::ScrollElasticity;
    switch (value) {
    case ScrollElasticity::Automatic: return Int(stream, 0);
    case ScrollElasticity::None: return Int(stream, 1);
    case ScrollElasticity::Allowed: return Int(stream, 2);
    }
    return stream;
}

TextStream& Write(TextStream& stream, WebCore::ScrollbarMode value) {
    using WebCore::ScrollbarMode;
    switch (value) {
    case ScrollbarMode::Auto: return Int(stream, 0);
    case ScrollbarMode::AlwaysOff: return Int(stream, 1);
    case ScrollbarMode::AlwaysOn: return Int(stream, 2);
    }
    return stream;
}

TextStream& Write(TextStream& stream, WebCore::NativeScrollbarVisibility value) {
    using WebCore::NativeScrollbarVisibility;
    switch (value) {
    case NativeScrollbarVisibility::Visible: return Int(stream, 0);
    case NativeScrollbarVisibility::HiddenByStyle: return Int(stream, 1);
    case NativeScrollbarVisibility::ReplacedByCustomScrollbar: return Int(stream, 2);
    }
    return stream;
}

TextStream& Write(TextStream& stream, bool value) {
    return _ZN3WTF10TextStreamlsEb(&stream, value);
}

template<typename Value> void DumpValueProperty(TextStream& stream, const char* name, Value value) {
    DumpProperty(stream, name, [&] { Write(stream, value); });
}

void DumpRadius(TextStream& stream, const char* name, const WebCore::FloatSize& radius) {
    DumpProperty(stream, name, [&] { _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(stream, radius); });
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(TextStream& stream, WebCore::TouchAction value) {
    using WebCore::TouchAction;
    switch (value) {
    case TouchAction::None: return Text(stream, "none");
    case TouchAction::Manipulation: return Text(stream, "manipulation");
    case TouchAction::PanX: return Text(stream, "pan-x");
    case TouchAction::PanY: return Text(stream, "pan-y");
    case TouchAction::PinchZoom: return Text(stream, "pinch-zoom");
    case TouchAction::Auto: return Text(stream, "auto");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(TextStream& stream, const WebCore::FloatRoundedRect& value) {
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(stream, value.m_rect);
    DumpRadius(stream, "top-left", value.m_radii.m_topLeft);
    DumpRadius(stream, "top-right", value.m_radii.m_topRight);
    DumpRadius(stream, "bottom-left", value.m_radii.m_bottomLeft);
    DumpRadius(stream, "bottom-right", value.m_radii.m_bottomRight);
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(TextStream& stream, const WebCore::RectEdges<float>& value) {
    using WebCore::BoxSide;
    Text(stream, "[top ");
    Float(stream, value.at(BoxSide::Top));
    Text(stream, " right ");
    Float(stream, value.at(BoxSide::Right));
    Text(stream, " bottom ");
    Float(stream, value.at(BoxSide::Bottom));
    Text(stream, " left ");
    Float(stream, value.at(BoxSide::Left));
    return Text(stream, "]");
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(TextStream& stream, const WebCore::Region& value) {
    Text(stream, "\n");
    IndentScope scope{stream};
    auto rects = _ZNK7WebCore6Region5rectsEv(&value);
    for (std::size_t i = 0; i < rects.size(); ++i) {
        _ZN3WTF10TextStream11writeIndentEv(&stream);
        Text(stream, "(rect ");
        _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(stream, rects[i]);
        Text(stream, ")\n");
    }
    return stream;
}

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(TextStream& stream, WebCore::ScrollableAreaParameters value) {
    using WebCore::NativeScrollbarVisibility;
    DumpValueProperty(stream, "horizontal scroll elasticity", value.horizontalScrollElasticity);
    DumpValueProperty(stream, "vertical scroll elasticity", value.verticalScrollElasticity);
    DumpValueProperty(stream, "horizontal scrollbar mode", value.horizontalScrollbarMode);
    DumpValueProperty(stream, "vertical scrollbar mode", value.verticalScrollbarMode);
    if (value.allowsHorizontalScrolling) DumpValueProperty(stream, "allows horizontal scrolling", value.allowsHorizontalScrolling);
    if (value.allowsVerticalScrolling) DumpValueProperty(stream, "allows vertical scrolling", value.allowsVerticalScrolling);
    if (value.horizontalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        DumpValueProperty(stream, "horizontal scrollbar hidden by style", value.horizontalNativeScrollbarVisibility);
    if (value.verticalNativeScrollbarVisibility == NativeScrollbarVisibility::HiddenByStyle)
        DumpValueProperty(stream, "vertical scrollbar hidden by style", value.verticalNativeScrollbarVisibility);
    return stream;
}

}
