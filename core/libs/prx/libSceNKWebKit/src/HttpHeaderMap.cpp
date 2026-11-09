/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

using namespace NKWebKit::HttpAbi;

HTTPHeaderMap& Map(HTTPHeaderMap* self) {
    if (!self) throw std::invalid_argument("null HTTPHeaderMap");
    return *self;
}

const HTTPHeaderMap& Map(const HTTPHeaderMap* self) {
    if (!self) throw std::invalid_argument("null HTTPHeaderMap");
    return *self;
}

std::uint16_t Character(const String& value, unsigned index) {
    if (!value.impl->data) throw std::invalid_argument("null header string storage");
    if (value.impl->hashAndFlags & 4)
        return static_cast<const unsigned char*>(value.impl->data)[index];
    return static_cast<const char16_t*>(value.impl->data)[index];
}

unsigned Length(const String& value) {
    return value.impl ? value.impl->length : 0;
}

std::uint16_t Fold(std::uint16_t value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

bool Equal(const String& a, const String& b) {
    if (a.impl == b.impl) return true;
    if (!a.impl || !b.impl) return false;
    const auto length = Length(a);
    if (length != Length(b)) return false;
    for (unsigned i = 0; i < length; ++i)
        if (Fold(Character(a, i)) != Fold(Character(b, i))) return false;
    return true;
}

bool FindName(const String& name, HTTPHeaderName& result) {
    const auto length = Length(name);
    for (unsigned index = 0; index < headerNames.size(); ++index) {
        const std::string_view candidate(headerNames[index]);
        if (candidate.size() != length) continue;
        unsigned i = 0;
        for (; i < length; ++i)
            if (Fold(Character(name, i)) != Fold(static_cast<unsigned char>(candidate[i]))) break;
        if (i == length) {
            result = static_cast<HTTPHeaderName>(index);
            return true;
        }
    }
    return false;
}

template<class TEntry>
void Validate(const HeaderVector<TEntry>& vector) {
    if (vector.size > vector.capacity) throw std::invalid_argument("invalid header vector size");
    if (vector.size && !vector.buffer) throw std::invalid_argument("null header vector storage");
}

template<class TEntry, class TMatch>
unsigned Find(const HeaderVector<TEntry>& vector, TMatch match) {
    Validate(vector);
    for (unsigned i = 0; i < vector.size; ++i)
        if (match(vector.buffer[i])) return i;
    return vector.size;
}

template<class TEntry>
void Append(HeaderVector<TEntry>& vector, TEntry entry, unsigned minimum) {
    Validate(vector);
    constexpr auto maxCapacity = std::numeric_limits<unsigned>::max() / sizeof(TEntry);
    if (vector.size >= maxCapacity) throw std::length_error("header vector size overflow");
    if (vector.size == vector.capacity) {
        const std::uint64_t grown = std::uint64_t(vector.capacity) + vector.capacity / 4 + 1;
        const auto capacity = std::max({grown, std::uint64_t(minimum), std::uint64_t(vector.size) + 1});
        if (capacity > maxCapacity) throw std::length_error("header vector capacity overflow");
        auto* buffer = static_cast<TEntry*>(_ZN3WTF10fastMallocEm(capacity * sizeof(TEntry)));
        for (unsigned i = 0; i < vector.size; ++i) {
            std::construct_at(buffer + i, std::move(vector.buffer[i]));
            std::destroy_at(vector.buffer + i);
        }
        _ZN3WTF8fastFreeEPv(vector.buffer);
        vector.buffer = buffer;
        vector.capacity = static_cast<unsigned>(capacity);
    }
    if (!vector.buffer) throw std::invalid_argument("null header vector storage");
    std::construct_at(vector.buffer + vector.size, std::move(entry));
    ++vector.size;
}

void Replace(String& destination, const String& source) {
    String retained(source);
    std::swap(destination.impl, retained.impl);
}

String Join(const String& first, const String& second) {
    constexpr std::string_view separator = ", ";
    const std::uint64_t length = std::uint64_t(Length(first)) + separator.size() + Length(second);
    if (length > std::numeric_limits<unsigned>::max()) throw std::length_error("header value overflow");
    const bool is8Bit = (!first.impl || (first.impl->hashAndFlags & 4)) && (!second.impl || (second.impl->hashAndFlags & 4));
    if (is8Bit) {
        unsigned char* data;
        auto result = _ZN3WTF10StringImpl19createUninitializedEjRPh(static_cast<unsigned>(length), data);
        unsigned position = 0;
        for (unsigned i = 0; i < Length(first); ++i) data[position++] = Character(first, i);
        for (auto character : separator) data[position++] = character;
        for (unsigned i = 0; i < Length(second); ++i) data[position++] = Character(second, i);
        return result;
    }
    char16_t* data;
    auto result = _ZN3WTF10StringImpl19createUninitializedEjRPDs(static_cast<unsigned>(length), data);
    unsigned position = 0;
    for (unsigned i = 0; i < Length(first); ++i) data[position++] = Character(first, i);
    for (auto character : separator) data[position++] = character;
    for (unsigned i = 0; i < Length(second); ++i) data[position++] = Character(second, i);
    return result;
}

void Update(HTTPHeaderMap& map, const String& name, const String& value, bool add, bool append) {
    HTTPHeaderName known;
    if (FindName(name, known)) {
        auto& vector = map.common;
        const auto index = append ? vector.size : Find(vector, [&](const auto& entry) { return entry.key == known; });
        if (index == vector.size) Append(vector, CommonHeader { known, String(value) }, 6);
        else if (add) Replace(vector.buffer[index].value, Join(vector.buffer[index].value, value));
        else Replace(vector.buffer[index].value, value);
        return;
    }
    auto& vector = map.uncommon;
    const auto index = append ? vector.size : Find(vector, [&](const auto& entry) { return Equal(entry.key, name); });
    if (index == vector.size) Append(vector, UncommonHeader { String(name), String(value) }, 0);
    else if (add) Replace(vector.buffer[index].value, Join(vector.buffer[index].value, value));
    else Replace(vector.buffer[index].value, value);
}

template<class TEntry, class TMatch>
bool Remove(HeaderVector<TEntry>& vector, TMatch match) {
    const auto index = Find(vector, match);
    if (index == vector.size) return false;
    std::destroy_at(vector.buffer + index);
    for (unsigned i = index + 1; i < vector.size; ++i) {
        std::construct_at(vector.buffer + i - 1, std::move(vector.buffer[i]));
        std::destroy_at(vector.buffer + i);
    }
    --vector.size;
    return true;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore13HTTPHeaderMapC1Ev(NKWebKit::HttpAbi::HTTPHeaderMap* self) {
    std::construct_at(&Map(self), NKWebKit::HttpAbi::HTTPHeaderMap {});
}

void APS5_VABI _ZN7WebCore13HTTPHeaderMapC2Ev(NKWebKit::HttpAbi::HTTPHeaderMap* self) {
    std::construct_at(&Map(self), NKWebKit::HttpAbi::HTTPHeaderMap {});
}

void APS5_VABI _ZN7WebCore13HTTPHeaderMap3addERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value) {
    Update(Map(self), name, value, true, false);
}

void APS5_VABI _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value) {
    Update(Map(self), name, value, false, false);
}

void APS5_VABI _ZN7WebCore13HTTPHeaderMap6appendERKN3WTF6StringES4_(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name, const NKWebKit::WtfAbi::String& value) {
    Update(Map(self), name, value, false, true);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name) {
    const auto& vector = Map(self).common;
    const auto index = Find(vector, [&](const auto& entry) { return entry.key == name; });
    return index == vector.size ? String() : String(vector.buffer[index].value);
}

bool APS5_VABI _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name) {
    const auto& vector = Map(self).common;
    return Find(vector, [&](const auto& entry) { return entry.key == name; }) != vector.size;
}

bool APS5_VABI _ZNK7WebCore13HTTPHeaderMap8containsERKN3WTF6StringE(const NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name) {
    const auto& map = Map(self);
    HTTPHeaderName known;
    if (FindName(name, known)) return _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(self, known);
    const auto& vector = map.uncommon;
    return Find(vector, [&](const auto& entry) { return Equal(entry.key, name); }) != vector.size;
}

bool APS5_VABI _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(NKWebKit::HttpAbi::HTTPHeaderMap* self, NKWebKit::HttpAbi::HTTPHeaderName name) {
    return Remove(Map(self).common, [&](const auto& entry) { return entry.key == name; });
}

bool APS5_VABI _ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE(NKWebKit::HttpAbi::HTTPHeaderMap* self, const NKWebKit::WtfAbi::String& name) {
    auto& map = Map(self);
    HTTPHeaderName known;
    if (FindName(name, known)) return _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(self, known);
    return Remove(map.uncommon, [&](const auto& entry) { return Equal(entry.key, name); });
}

NKWebKit::HttpAbi::ASCIILiteral APS5_VABI _ZN7WebCore20httpHeaderNameStringENS_14HTTPHeaderNameE(NKWebKit::HttpAbi::HTTPHeaderName name) {
    const auto index = static_cast<unsigned>(name);
    if (index >= headerNames.size()) throw std::out_of_range("invalid HTTPHeaderName");
    return { headerNames[index] };
}

}
