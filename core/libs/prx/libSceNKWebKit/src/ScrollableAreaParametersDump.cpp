// Derived from WebKit/WebKit/Source/WebCore/page/scrolling/ScrollingCoordinator.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/ScrollableAreaParametersDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

using NKWebKit::WtfAbi::TextStream;

void StartProperty(TextStream& stream, const char* name) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, name);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
}

void DumpInt(TextStream& stream, const char* name, int value) {
    StartProperty(stream, name);
    _ZN3WTF10TextStreamlsEi(&stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

void DumpSetFlag(TextStream& stream, const char* name, bool value) {
    if (!value) return;
    StartProperty(stream, name);
    _ZN3WTF10TextStreamlsEb(&stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(TextStream& stream, WebCore::ScrollableAreaParameters parameters) {
    DumpInt(stream, "horizontal scroll elasticity", parameters.horizontalScrollElasticity);
    DumpInt(stream, "vertical scroll elasticity", parameters.verticalScrollElasticity);
    DumpInt(stream, "horizontal scrollbar mode", parameters.horizontalScrollbarMode);
    DumpInt(stream, "vertical scrollbar mode", parameters.verticalScrollbarMode);
    DumpSetFlag(stream, "has enabled horizontal scrollbar", parameters.hasEnabledHorizontalScrollbar);
    DumpSetFlag(stream, "has enabled vertical scrollbar", parameters.hasEnabledVerticalScrollbar);
    DumpSetFlag(stream, "horizontal scrollbar hidden by style", parameters.horizontalScrollbarHiddenByStyle);
    DumpSetFlag(stream, "vertical scrollbar hidden by style", parameters.verticalScrollbarHiddenByStyle);
    return stream;
}

}
