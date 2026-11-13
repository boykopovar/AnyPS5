/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
 * Derived from Source/WTF/wtf/URL.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 * Derived from Source/WTF/wtf/URLParser.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*

Copyright (C) 2014-2019 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Derived from Source/WTF/wtf/text/StringView.cpp@c55364d53f971f63a709976f168d932d558b8909
*/
/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * Derived from Source/WTF/wtf/text/StringBuilder.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
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
 * Derived from Source/WTF/wtf/TimeWithDynamicClockType.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/WtfUrlExports.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using namespace Jsc::WtfAbi;

constexpr char EmptyCharacters[] = "";

StringView View(const StringImpl* string) {
    if (!string) return {nullptr, 0, true};
    return {string->data, string->length, (string->hashAndFlags & StringImplFlag8Bit) != 0};
}

char16_t At(const StringView& view, unsigned index) {
    if (!view.characters) throw std::invalid_argument("WTF::StringView: null characters");
    return view.is8Bit ? static_cast<const std::uint8_t*>(view.characters)[index] : static_cast<const char16_t*>(view.characters)[index];
}

StringView Substring(const StringView& view, unsigned start, unsigned length) {
    if (start >= view.length) return {EmptyCharacters, 0, true};
    const unsigned maxLength = view.length - start;
    if (length >= maxLength) {
        if (!start) return view;
        length = maxLength;
    }
    if (!view.characters) throw std::invalid_argument("WTF::StringView::substring: null characters");
    const std::size_t width = view.is8Bit ? 1 : 2;
    return {static_cast<const char*>(view.characters) + static_cast<std::size_t>(start) * width, length, view.is8Bit};
}

const URL& Checked(const URL* url, const char* function) {
    if (!url) throw std::invalid_argument(function);
    return *url;
}

char16_t StringCharacterAt(const StringImpl* string, unsigned index) {
    if (!string || index >= string->length) return 0;
    return At(View(string), index);
}

bool IsASCIIAlphaCaselessEqual(char16_t character, char expectedLowercase) {
    return static_cast<char16_t>(character | 0x20) == static_cast<unsigned char>(expectedLowercase);
}

bool ProtocolIs(const URL& url, const char* protocol) {
    if (!(url.bits & URLValidBit)) return false;
    const unsigned schemeEnd = url.bits >> URLSchemeEndShift;
    for (unsigned index = 0; index < schemeEnd; ++index) {
        if (!protocol[index] || !IsASCIIAlphaCaselessEqual(StringCharacterAt(url.string, index), protocol[index])) return false;
    }
    return !protocol[schemeEnd];
}

bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return character <= ' ' && (character == ' ' || (character <= 0xD && character >= 0x9));
    return character == 0x1680 || (character >= 0x2000 && character <= 0x200A) || character == 0x2028 || character == 0x205F || character == 0x3000;
}

OptionalUInt16 ParseUInt16(const StringView& view) {
    if (!view.characters) return {};
    constexpr unsigned integralMax = std::numeric_limits<std::uint16_t>::max();
    constexpr unsigned maxMultiplier = integralMax / 10;
    unsigned index = 0;
    unsigned length = view.length;
    while (length && IsSpaceOrNewline(At(view, index))) {
        --length;
        ++index;
    }
    if (length && At(view, index) == '+') {
        --length;
        ++index;
    }
    auto isDigit = [](char16_t character) { return character >= '0' && character <= '9'; };
    if (!length || !isDigit(At(view, index))) return {};
    unsigned value = 0;
    while (length && isDigit(At(view, index))) {
        --length;
        const unsigned digit = At(view, index) - '0';
        if (value > maxMultiplier || (value == maxMultiplier && digit > integralMax % 10)) return {};
        value = value * 10 + digit;
        ++index;
    }
    while (length && IsSpaceOrNewline(At(view, index))) {
        --length;
        ++index;
    }
    if (length) return {};
    return {true, static_cast<std::uint16_t>(value)};
}

bool CharactersAreAllASCII(const StringView& view) {
    if (!view.characters) throw std::invalid_argument("WTF::StringBuilder::isAllASCII: null characters");
    char16_t bits = 0;
    for (unsigned index = 0; index < view.length; ++index) bits |= At(view, index);
    return !(bits & ~0x7F);
}

}

extern "C" {

OptionalUInt16 APS5_VABI _ZNK3WTF3URL4portEv(const URL* self) {
    const URL& url = Checked(self, __func__);
    const unsigned portLength = (url.bits >> URLPortLengthShift) & URLPortLengthMask;
    if (!portLength) return {};
    return ParseUInt16(Substring(View(url.string), url.hostEnd + 1, portLength - 1));
}

StringView APS5_VABI _ZNK3WTF3URL5queryEv(const URL* self) {
    const URL& url = Checked(self, __func__);
    if (url.queryEnd == url.pathEnd) return {nullptr, 0, true};
    return Substring(View(url.string), url.pathEnd + 1, url.queryEnd - (url.pathEnd + 1));
}

bool APS5_VABI _ZNK3WTF3URL11isLocalFileEv(const URL* self) {
    return ProtocolIs(Checked(self, __func__), "file");
}

bool APS5_VABI _ZNK3WTF3URL15protocolIsAboutEv(const URL* self) {
    return ProtocolIs(Checked(self, __func__), "about");
}

StringView APS5_VABI _ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(const URL* self) {
    const URL& url = Checked(self, __func__);
    if (!(url.bits & URLValidBit)) return View(url.string);
    return Substring(View(url.string), 0, url.pathEnd);
}

OptionalUInt16 APS5_VABI _ZN3WTF22defaultPortForProtocolENS_10StringViewE(StringView scheme) {
    const unsigned length = scheme.length;
    if (!length) return {};
    switch (At(scheme, 0)) {
        case 'w':
            switch (length) {
                case 2:
                    if (At(scheme, 1) == 's') return {true, 80};
                    return {};
                case 3:
                    if (At(scheme, 1) == 's' && At(scheme, 2) == 's') return {true, 443};
                    return {};
                default:
                    return {true, 0};
            }
        case 'h':
            switch (length) {
                case 4:
                    if (At(scheme, 1) == 't' && At(scheme, 2) == 't' && At(scheme, 3) == 'p') return {true, 80};
                    return {};
                case 5:
                    if (At(scheme, 1) == 't' && At(scheme, 2) == 't' && At(scheme, 3) == 'p' && At(scheme, 4) == 's') return {true, 443};
                    return {};
                default:
                    return {};
            }
        case 'f':
            if (length == 3 && At(scheme, 1) == 't' && At(scheme, 2) == 'p') return {true, 21};
            return {};
        default:
            return {};
    }
}

bool APS5_VABI _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(const TimeWithDynamicClockType* self, const TimeWithDynamicClockType* other) {
    if (!self || !other) throw std::invalid_argument(__func__);
    if (self->type != other->type) throw std::invalid_argument("WTF::TimeWithDynamicClockType::operator<: clock types differ");
    return self->value < other->value;
}

TimeWithDynamicClockType APS5_VABI _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(const Seconds* self, const TimeWithDynamicClockType* other) {
    if (!self || !other) throw std::invalid_argument(__func__);
    return {other->value + self->value, other->type};
}

bool APS5_VABI _ZNK3WTF13StringBuilder10isAllASCIIEv(const StringBuilder* self) {
    if (!self) throw std::invalid_argument(__func__);
    if (self->overflowed) throw std::invalid_argument("WTF::StringBuilder::isAllASCII: the builder has overflowed");
    if (self->length < 0) throw std::invalid_argument("WTF::StringBuilder::isAllASCII: negative length");
    const auto length = static_cast<unsigned>(self->length);
    if (!length) return true;
    const StringImpl* characters = self->string ? self->string : self->buffer;
    if (!characters) throw std::invalid_argument("WTF::StringBuilder::isAllASCII: no string or buffer");
    return CharactersAreAllASCII({characters->data, length, self->is8Bit});
}

}
