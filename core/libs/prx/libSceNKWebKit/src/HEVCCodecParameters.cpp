/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/HEVCCodecParameters.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

bool IsSpace(char16_t c) {
    return c == ' ' || (c >= 9 && c <= 13);
}

template<typename T> std::optional<T> ParseInteger(std::u16string_view text, unsigned base = 10) {
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    if (!text.empty() && text.front() == '+') text.remove_prefix(1);
    T value = 0;
    bool any = false;
    while (!text.empty()) {
        auto c = text.front();
        unsigned digit = c >= '0' && c <= '9' ? c - '0' : (c | 0x20) >= 'a' && (c | 0x20) <= 'z' ? (c | 0x20) - 'a' + 10 : base;
        if (digit >= base) break;
        if (value > (std::numeric_limits<T>::max() - digit) / base) return std::nullopt;
        value = static_cast<T>(value * base + digit);
        any = true;
        text.remove_prefix(1);
    }
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    if (!any || !text.empty()) return std::nullopt;
    return value;
}

std::u16string_view Next(std::u16string_view& text) {
    while (!text.empty() && text.front() == '.') text.remove_prefix(1);
    auto end = text.find('.');
    auto result = text.substr(0, end);
    text.remove_prefix(end == text.npos ? text.size() : end + 1);
    return result;
}

}

std::optional<NKWebKit::HEVCAbi::HEVCParameters> APS5_VABI _ZN7WebCore24parseHEVCCodecParametersERKN3WTF6StringE(const NKWebKit::WtfAbi::String& string) {
    using NKWebKit::HEVCAbi::HEVCParameters;
    std::u16string storage;
    if (string.impl) {
        if (string.impl->length && !string.impl->data) throw std::invalid_argument("null HEVC string storage");
        for (unsigned i = 0; i < string.impl->length; ++i)
            storage.push_back(string.impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i]);
    }
    std::u16string_view text(storage);
    HEVCParameters parameters;
    auto codec = Next(text);
    if (codec == u"hvc1") parameters.codec = HEVCParameters::Codec::Hvc1;
    else if (codec == u"hev1") parameters.codec = HEVCParameters::Codec::Hev1;
    else return std::nullopt;

    auto profile = Next(text);
    if (!profile.empty() && profile.front() >= 'A' && profile.front() <= 'C') {
        parameters.generalProfileSpace = 1 + profile.front() - 'A';
        profile.remove_prefix(1);
    }
    auto profileIDC = ParseInteger<uint8_t>(profile);
    if (!profileIDC) return std::nullopt;
    parameters.generalProfileIDC = *profileIDC;

    auto compatibility = ParseInteger<uint32_t>(Next(text), 16);
    if (!compatibility) return std::nullopt;
    uint32_t reversed = 0;
    for (unsigned i = 0; i < 32; ++i) reversed = (reversed << 1) | ((*compatibility >> i) & 1);
    parameters.generalProfileCompatibilityFlags = reversed;

    auto tier = Next(text);
    if (tier.empty() || (tier.front() != 'L' && tier.front() != 'H')) return std::nullopt;
    parameters.generalTierFlag = tier.front() == 'H';
    auto level = ParseInteger<uint8_t>(tier.substr(1));
    if (!level) return std::nullopt;
    parameters.generalLevelIDC = *level;

    for (unsigned i = 0; i < 6; ++i) {
        auto component = Next(text);
        if (component.empty()) break;
        auto flag = ParseInteger<uint8_t>(component, 16);
        if (!flag) return std::nullopt;
        parameters.generalConstraintIndicatorFlags.buffer[i] = *flag;
    }
    return parameters;
}
