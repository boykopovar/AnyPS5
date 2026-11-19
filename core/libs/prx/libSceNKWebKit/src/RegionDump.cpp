// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/graphics/Region.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/RegionDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WebCoreDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

using NKWebKit::WtfAbi::TextStream;

class IndentScope {
public:
    explicit IndentScope(TextStream& stream) : m_stream(stream) { ++m_stream.indent; }
    ~IndentScope() { --m_stream.indent; }
    IndentScope(const IndentScope&) = delete;
    IndentScope& operator=(const IndentScope&) = delete;

private:
    TextStream& m_stream;
};

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE(TextStream& stream, const WebCore::Region& region) {
    _ZN3WTF10TextStreamlsEPKc(&stream, "\n");
    {
        IndentScope indentScope(stream);
        auto rects = _ZNK7WebCore6Region5rectsEv(&region);
        for (std::size_t i = 0; i < rects.size(); ++i) {
            _ZN3WTF10TextStream11writeIndentEv(&stream);
            _ZN3WTF10TextStreamlsEPKc(&stream, "(rect ");
            _ZN7WebCorelsERN3WTF10TextStreamERKNS_7IntRectE(stream, rects[i]);
            _ZN3WTF10TextStreamlsEPKc(&stream, ")\n");
        }
    }
    return stream;
}

}
