// Derived from Source/WebCore/platform/graphics/FloatRoundedRect.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreGeometryDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

using NKWebKit::WtfAbi::TextStream;

void DumpProperty(TextStream& stream, const char* name, const WebCore::FloatSize& value) {
    _ZN3WTF10TextStream10startGroupEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, name);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatSizeE(stream, value);
    _ZN3WTF10TextStream8endGroupEv(&stream);
}

}

extern "C" {

TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE(TextStream& stream, const WebCore::FloatRoundedRect& value) {
    _ZN7WebCorelsERN3WTF10TextStreamERKNS_9FloatRectE(stream, value.m_rect);
    DumpProperty(stream, "top-left", value.m_radii.m_topLeft);
    DumpProperty(stream, "top-right", value.m_radii.m_topRight);
    DumpProperty(stream, "bottom-left", value.m_radii.m_bottomLeft);
    DumpProperty(stream, "bottom-right", value.m_radii.m_bottomRight);
    return stream;
}

}
