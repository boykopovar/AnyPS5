/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
 * Derived from Source/JavaScriptCore/yarr/YarrFlags.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceJsc/include/YarrFlagsExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::StringView;
using Jsc::YarrAbi::Flags;
using Jsc::YarrAbi::OptionalFlags;

bool FlagFor(char16_t character, Flags& flag) {
    switch (character) {
        case u'd': flag = Flags::HasIndices; return true;
        case u'g': flag = Flags::Global; return true;
        case u'i': flag = Flags::IgnoreCase; return true;
        case u'm': flag = Flags::Multiline; return true;
        case u's': flag = Flags::DotAll; return true;
        case u'u': flag = Flags::Unicode; return true;
        case u'v': flag = Flags::UnicodeSets; return true;
        case u'y': flag = Flags::Sticky; return true;
        default: return false;
    }
}

char16_t CodeUnit(const StringView& string, unsigned index) {
    if (string.is8Bit) return static_cast<const std::uint8_t*>(string.characters)[index];
    return static_cast<const char16_t*>(string.characters)[index];
}

}

extern "C" {

OptionalFlags APS5_VABI _ZN3JSC4Yarr10parseFlagsEN3WTF10StringViewE(StringView string) {
    if (string.length && !string.characters) throw std::invalid_argument("JSC::Yarr::parseFlags: null characters with a nonzero length");
    std::uint16_t flags = 0;
    for (unsigned index = 0; index < string.length; ++index) {
        Flags flag;
        if (!FlagFor(CodeUnit(string, index), flag)) return {0, false};
        const auto bit = static_cast<std::uint16_t>(flag);
        if (flags & bit) return {0, false};
        flags |= bit;
    }
    const auto unicode = static_cast<std::uint16_t>(Flags::Unicode);
    const auto unicodeSets = static_cast<std::uint16_t>(Flags::UnicodeSets);
    if ((flags & unicode) && (flags & unicodeSets)) return {0, false};
    return {flags, true};
}

}
