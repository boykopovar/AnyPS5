// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/LocalizedStrings.cpp
/*
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
#include "prx/libSceNKWebKit/include/LocalizedStringsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;

String FromUTF8(const char* data) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(data));
}

String FormatLocalizedString(const char* format, ...) {
    va_list args, argsCopy;
    va_start(args, format);
    va_copy(argsCopy, args);

    int resultSize = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);

    if (resultSize <= 0) {
        va_end(args);
        return FromUTF8(format);
    }

    std::vector<char> result(static_cast<std::size_t>(resultSize) + 1);
    std::vsnprintf(result.data(), result.size(), format, args);
    va_end(args);
    return FromUTF8(result.data());
}

}

extern "C" {

String APS5_VABI _ZN7WebCore15localizedStringEPKc(const char* key) {
    if (!key) throw std::invalid_argument("null WebCore::localizedString key");
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(key), std::strlen(key));
}

String APS5_VABI _ZN7WebCore22multipleFileUploadTextEj(unsigned numberOfFiles) {
    return FormatLocalizedString("%d files", numberOfFiles);
}

String APS5_VABI _ZN7WebCore18pluginTooSmallTextEv() {
    return _ZN7WebCore15localizedStringEPKc("Plug-In too small");
}

String APS5_VABI _ZN7WebCore32useBlockedPlugInContextMenuTitleEv() {
    return _ZN7WebCore15localizedStringEPKc("Show in blocked plug-in");
}

}
