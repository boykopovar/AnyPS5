// Derived from Source/WebCore/platform/LengthBox.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "prx/libSceNKWebKit/include/WebCoreGeometryDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(NKWebKit::WtfAbi::TextStream& stream, const NKWebKit::WebCoreAbi::FloatBoxExtent& value) {
    _ZN3WTF10TextStreamlsEPKc(&stream, "top: ");
    _ZN3WTF10TextStreamlsEf(&stream, value.top);
    _ZN3WTF10TextStreamlsEPKc(&stream, " right: ");
    _ZN3WTF10TextStreamlsEf(&stream, value.right);
    _ZN3WTF10TextStreamlsEPKc(&stream, " bottom: ");
    _ZN3WTF10TextStreamlsEf(&stream, value.bottom);
    _ZN3WTF10TextStreamlsEPKc(&stream, " left: ");
    return _ZN3WTF10TextStreamlsEf(&stream, value.left);
}

}
