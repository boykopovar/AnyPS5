/* Derived from Source/WebCore/platform/graphics/FourCC.cpp@c55364d53f971f63a709976f168d932d558b8909 */
/* Derived from Source/WTF/wtf/text/WTFString.cpp@c55364d53f971f63a709976f168d932d558b8909 */
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
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "prx/libSceNKWebKit/include/FourCCExports.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

using NKWebKit::WebCoreAbi::OptionalFourCC;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

namespace {

void RequireCopied(bool copied, int savedErrno, const char* what) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string("FourCC::fromString: unreadable ") + what);
}

}

extern "C" OptionalFourCC* APS5_VABI _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(OptionalFourCC* result, const String* string) {
    if (!result || !string) throw std::invalid_argument("null FourCC result or String reference");
    const int savedErrno = errno;
    const StringImpl* guestImpl = nullptr;
    RequireCopied(GuestCopyIn_nid_no_patch(&guestImpl, &string->impl, sizeof(guestImpl)), savedErrno, "string");
    StringImpl impl{};
    if (guestImpl) RequireCopied(GuestCopyIn_nid_no_patch(&impl, guestImpl, sizeof(impl)), savedErrno, "string implementation");
    if (guestImpl && impl.length && !impl.data) throw std::invalid_argument("null WTF::StringImpl characters");
    if (!guestImpl || impl.length != 4) {
        result->engaged = false;
        return result;
    }
    const bool is8Bit = impl.hashAndFlags & 4;
    unsigned char characters[4 * sizeof(char16_t)];
    RequireCopied(GuestCopyIn_nid_no_patch(characters, impl.data, is8Bit ? 4 : sizeof(characters)), savedErrno, "string characters");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) {
        char16_t ch = characters[i];
        if (!is8Bit) std::memcpy(&ch, characters + i * sizeof(char16_t), sizeof(ch));
        const auto byte = ch && (ch < 0x20 || ch > 0x7f) ? '?' : ch;
        value = (value << 8) | byte;
    }
    result->engaged = true;
    result->value = value;
    return result;
}
