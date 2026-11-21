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
 */

/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

bool ShouldEncode(unsigned character) {
    return character <= 0x20 || character > 0x7E || std::string_view("\"#/:;<=>?@[\\]^`{|}").find(static_cast<char>(character)) != std::string_view::npos;
}

}  // namespace

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF28encodeWithURLEscapeSequencesERKNS_6StringE(const NKWebKit::WtfAbi::String& input) {
    using NKWebKit::WtfAbi::StringView;
    if (!input.impl) return input;
    auto& impl = *input.impl;
    bool is8Bit = impl.hashAndFlags & 4;
    if (impl.length && !impl.data) throw std::invalid_argument("null WTF URL escape data");
    auto unit = [&](unsigned index) -> unsigned {
        return is8Bit ? static_cast<const unsigned char*>(impl.data)[index] : static_cast<const char16_t*>(impl.data)[index];
    };
    unsigned first = 0;
    while (first < impl.length && !ShouldEncode(unit(first))) ++first;
    if (first == impl.length) return input;

    std::string output;
    auto append = [&](StringView view, unsigned mode) {
        auto utf8 = _ZNK3WTF10StringView4utf8ENS_14ConversionModeE(&view, mode);
        auto* bytes = reinterpret_cast<const unsigned char*>(utf8.buffer + 1);
        constexpr char hex[] = "0123456789ABCDEF";
        for (std::size_t i = 0; i < utf8.buffer->length; ++i) {
            unsigned byte = bytes[i];
            if (ShouldEncode(byte)) {
                output.push_back('%');
                output.push_back(hex[byte >> 4]);
                output.push_back(hex[byte & 15]);
            } else output.push_back(static_cast<char>(byte));
        }
    };
    bool trailingLead = !is8Bit && unit(impl.length - 1) >= 0xD800 && unit(impl.length - 1) <= 0xDBFF;
    append({impl.data, impl.length - static_cast<unsigned>(trailingLead), is8Bit}, 2);
    if (trailingLead) append({static_cast<const char16_t*>(impl.data) + impl.length - 1, 1, false}, 0);
    if (output.size() > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF URL escape length overflow");
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(output.data()), static_cast<unsigned>(output.size()));
}
