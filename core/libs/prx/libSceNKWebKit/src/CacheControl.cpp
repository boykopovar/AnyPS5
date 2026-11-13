/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/CacheControlExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using NKWebKit::HttpAbi::CacheControlDirectives;
using NKWebKit::WtfAbi::String;

template<class TCharacter>
bool IsSeparator(TCharacter c) {
    return std::u16string_view(u"()<>@,;:\\\"/[]?={} \t").find(c) != std::u16string_view::npos;
}

template<class TCharacter>
std::basic_string_view<TCharacter> Trim(std::basic_string_view<TCharacter> value) {
    std::size_t end = 0;
    while (end < value.size() && !IsSeparator(value[end])) ++end;
    return value.substr(0, end);
}

template<class TCharacter>
bool Equal(std::basic_string_view<TCharacter> value, std::string_view literal) {
    if (value.size() != literal.size()) return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        auto c = value[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != literal[i]) return false;
    }
    return true;
}

template<class TCharacter>
std::vector<std::pair<std::basic_string_view<TCharacter>, std::basic_string_view<TCharacter>>> Parse(std::basic_string_view<TCharacter> header) {
    std::vector<std::pair<std::basic_string_view<TCharacter>, std::basic_string_view<TCharacter>>> result;
    const auto missing = header.npos;
    std::size_t pos = 0;
    while (pos < header.size()) {
        auto comma = header.find(',', pos);
        auto equal = header.find('=', pos);
        if (equal == missing && comma == missing) {
            result.emplace_back(Trim(header.substr(pos)), header.substr(0, 0));
            return result;
        }
        if (comma != missing && (comma < equal || equal == missing)) {
            result.emplace_back(Trim(header.substr(pos, comma - pos)), header.substr(0, 0));
            pos = comma + 1;
            continue;
        }
        auto directive = Trim(header.substr(pos, equal - pos));
        pos = equal + 1;
        auto value = header.substr(pos);
        if (value.starts_with(static_cast<TCharacter>('"'))) {
            auto quote = value.find('"', 1);
            if (quote == missing) {
                result.emplace_back(directive, Trim(value.substr(1)));
                return result;
            }
            result.emplace_back(directive, value.substr(1, quote - 1));
            pos += quote + 1;
            comma = header.find(',', pos);
            if (comma == missing) return result;
            pos = comma + 1;
            continue;
        }
        comma = value.find(',');
        if (comma == missing) {
            result.emplace_back(directive, Trim(value));
            return result;
        }
        result.emplace_back(directive, Trim(value.substr(0, comma)));
        pos += comma + 1;
    }
    return result;
}

template<class TCharacter>
void Apply(const String& input, CacheControlDirectives& result) {
    std::basic_string<TCharacter> safe;
    auto* characters = static_cast<const TCharacter*>(input.impl->data);
    for (unsigned i = 0; i < input.impl->length; ++i)
        if (characters[i] > ' ' && characters[i] != 127) safe.push_back(characters[i]);
    for (auto [name, value] : Parse(std::basic_string_view<TCharacter>(safe))) {
        if (Equal(name, "no-cache") && value.empty()) result.noCache = true;
        else if (Equal(name, "no-store")) result.noStore = true;
        else if (Equal(name, "must-revalidate")) result.mustRevalidate = true;
        else if (Equal(name, "immutable")) result.immutable = true;
        else {
            double* destination = nullptr;
            if (Equal(name, "max-age")) destination = &result.maxAge;
            else if (Equal(name, "max-stale")) destination = &result.maxStale;
            else if (Equal(name, "stale-while-revalidate")) destination = &result.staleWhileRevalidate;
            if (!destination || !std::isnan(*destination)) continue;
            if (Equal(name, "max-stale") && value.empty()) {
                *destination = std::numeric_limits<double>::infinity();
                continue;
            }
            bool ok;
            double number;
            if constexpr (sizeof(TCharacter) == 1)
                number = _ZN3WTF18charactersToDoubleEPKhmPb(value.data(), value.size(), &ok);
            else
                number = _ZN3WTF18charactersToDoubleEPKDsmPb(value.data(), value.size(), &ok);
            if (ok) *destination = number;
        }
    }
}

template<class TCharacter>
bool PragmaNoCache(const String& input) {
    std::basic_string_view<TCharacter> value(static_cast<const TCharacter*>(input.impl->data), input.impl->length);
    for (std::size_t i = 0; i + 8 <= value.size(); ++i)
        if (Equal(value.substr(i, 8), "no-cache")) return true;
    return false;
}

}

extern "C" NKWebKit::HttpAbi::CacheControlDirectives APS5_VABI _ZN7WebCore27parseCacheControlDirectivesERKNS_13HTTPHeaderMapE(const NKWebKit::HttpAbi::HTTPHeaderMap& headers) {
    CacheControlDirectives result;
    auto control = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&headers, NKWebKit::HttpAbi::HTTPHeaderName::CacheControl);
    if (control.impl && control.impl->length) {
        if (control.impl->hashAndFlags & 4) Apply<unsigned char>(control, result);
        else Apply<char16_t>(control, result);
    }
    if (!result.noCache) {
        auto pragma = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&headers, NKWebKit::HttpAbi::HTTPHeaderName::Pragma);
        if (pragma.impl && pragma.impl->length)
            result.noCache = pragma.impl->hashAndFlags & 4 ? PragmaNoCache<unsigned char>(pragma) : PragmaNoCache<char16_t>(pragma);
    }
    return result;
}
