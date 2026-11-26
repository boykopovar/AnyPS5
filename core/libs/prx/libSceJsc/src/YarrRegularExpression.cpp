// Derived from Source/JavaScriptCore/yarr/RegularExpression.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
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

#include "prx/libSceJsc/include/YarrRegularExpressionExports.hpp"
#include "prx/libSceJsc/include/WtfSha1Exports.hpp"
#include "prx/libSceJsc/src/YarrRegExpEngine.hpp"
#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include "libregexp.h"
}

namespace {

using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringImplFlag8Bit;
using EngineState = Jsc::YarrEngine::State;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;
using Jsc::YarrAbi::StringReference;

struct Units {
    const void* data;
    unsigned length;
    bool is8Bit;
    char16_t At(unsigned index) const {
        return is8Bit ? static_cast<const std::uint8_t*>(data)[index] : static_cast<const char16_t*>(data)[index];
    }
};

Units UnitsOf(const StringImpl* impl, const char* message) {
    if (!impl) return {nullptr, 0, true};
    if (impl->length && !impl->data) throw std::invalid_argument(message);
    return {impl->data, impl->length, (impl->hashAndFlags & StringImplFlag8Bit) != 0};
}

int HexValue(char16_t unit) {
    if (unit >= u'0' && unit <= u'9') return unit - u'0';
    if (unit >= u'a' && unit <= u'f') return unit - u'a' + 10;
    if (unit >= u'A' && unit <= u'F') return unit - u'A' + 10;
    return -1;
}

bool ReadEscape(const Units& pattern, unsigned& index, unsigned end, char32_t& codePoint) {
    if (index + 1 >= end || pattern.At(index) != u'\\' || pattern.At(index + 1) != u'u') return false;
    unsigned cursor = index + 2;
    char32_t value = 0;
    if (cursor < end && pattern.At(cursor) == u'{') {
        unsigned digits = 0;
        for (++cursor; cursor < end && pattern.At(cursor) != u'}'; ++cursor, ++digits) {
            const int digit = HexValue(pattern.At(cursor));
            if (digit < 0) return false;
            value = value * 16 + static_cast<char32_t>(digit);
            if (value > 0x10FFFF) return false;
        }
        if (!digits || cursor >= end) return false;
        ++cursor;
    } else {
        if (cursor + 4 > end) return false;
        for (unsigned digits = 0; digits < 4; ++digits, ++cursor) {
            const int digit = HexValue(pattern.At(cursor));
            if (digit < 0) return false;
            value = value * 16 + static_cast<char32_t>(digit);
        }
    }
    index = cursor;
    codePoint = value;
    return true;
}

std::u32string GroupName(const Units& pattern, unsigned begin, unsigned end) {
    std::u32string name;
    unsigned index = begin;
    while (index < end) {
        char32_t codePoint = 0;
        if (!ReadEscape(pattern, index, end, codePoint)) codePoint = pattern.At(index++);
        if (codePoint >= 0xDC00 && codePoint <= 0xDFFF && !name.empty() && name.back() >= 0xD800 && name.back() <= 0xDBFF) {
            name.back() = 0x10000 + ((name.back() - 0xD800) << 10) + (codePoint - 0xDC00);
            continue;
        }
        name.push_back(codePoint);
    }
    return name;
}

bool RejectedByYarr(const Units& pattern) {
    std::vector<std::u32string> names;
    bool inClass = false;
    for (unsigned index = 0; index < pattern.length; ++index) {
        const char16_t unit = pattern.At(index);
        if (unit == u'\\') {
            ++index;
            continue;
        }
        if (inClass) {
            if (unit == u']') inClass = false;
            continue;
        }
        if (unit == u'[') {
            inClass = true;
            continue;
        }
        if (unit != u'(' || index + 2 >= pattern.length || pattern.At(index + 1) != u'?') continue;
        const char16_t kind = pattern.At(index + 2);
        if (kind == u':' || kind == u'=' || kind == u'!') continue;
        if (kind != u'<') return true;
        if (index + 3 < pattern.length && (pattern.At(index + 3) == u'=' || pattern.At(index + 3) == u'!')) return true;
        unsigned end = index + 3;
        while (end < pattern.length && pattern.At(end) != u'>') ++end;
        if (end == pattern.length) continue;
        std::u32string name = GroupName(pattern, index + 3, end);
        for (const std::u32string& seen : names) {
            if (seen == name) return true;
        }
        names.push_back(std::move(name));
    }
    return false;
}

void AppendUtf8(std::string& out, std::uint32_t codePoint) {
    if (codePoint < 0x80) {
        out.push_back(static_cast<char>(codePoint));
    } else if (codePoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 63)));
    } else if (codePoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 63)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 63)));
        out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (codePoint & 63)));
    }
}

std::string EncodePattern(const Units& pattern, bool unicode) {
    std::string out;
    for (unsigned index = 0; index < pattern.length; ++index) {
        std::uint32_t codePoint = pattern.At(index);
        if (unicode && codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < pattern.length) {
            const std::uint32_t next = pattern.At(index + 1);
            if (next >= 0xDC00 && next <= 0xDFFF) {
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (next - 0xDC00);
                ++index;
            }
        }
        AppendUtf8(out, codePoint);
    }
    return out;
}

bool IsEngineLimit(std::string_view message) {
    for (const std::string_view limit : {"too many captures", "too many named groups", "too many ranges", "too many imbricated quantifiers", "stack overflow"}) {
        if (message == limit) return true;
    }
    return false;
}

std::uint8_t* Compile(const Units& pattern, int flags) {
    if (RejectedByYarr(pattern)) return nullptr;
    const std::string encoded = EncodePattern(pattern, (flags & LRE_FLAG_UNICODE) != 0);
    EngineState state{static_cast<const char*>(__builtin_frame_address(0)), 0};
    char message[128] = "";
    int length = 0;
    std::uint8_t* bytecode = lre_compile(&length, message, sizeof(message), encoded.c_str(), encoded.size(), flags, &state);
    if (bytecode) return bytecode;
    if (std::string_view(message) == "out of memory") throw std::bad_alloc();
    if (IsEngineLimit(message)) throw std::logic_error(std::string("JSC::Yarr::RegularExpression: pattern beyond a libregexp limit: ") + message);
    return nullptr;
}

const RegularExpression& Checked(const RegularExpression* self, const char* message) {
    if (!self || !self->d) throw std::invalid_argument(message);
    return *self;
}

int MatchUnits(RegularExpressionPrivate& d, const Units& input, int startFrom, int* matchLength) {
    if (!d.regExpByteCode) return -1;
    const auto* bytecode = static_cast<const std::uint8_t*>(d.regExpByteCode);
    bool matched = false;
    std::uint8_t* captures[2 * 255] = {};
    static const std::uint8_t empty[2] = {};
    const auto* buffer = input.data ? static_cast<const std::uint8_t*>(input.data) : empty;
    if (input.length <= static_cast<unsigned>(INT_MAX) && static_cast<unsigned>(startFrom) <= input.length) {
        EngineState state{static_cast<const char*>(__builtin_frame_address(0)), 0};
        const int type = input.is8Bit ? 0 : 1;
        const int result = lre_exec(captures, bytecode, buffer, startFrom, static_cast<int>(input.length), type, &state);
        if (result == LRE_RET_MEMORY_ERROR) throw std::bad_alloc();
        if (result == 1) matched = true;
        else if (result != 0 && result != LRE_RET_TIMEOUT) throw std::logic_error("JSC::Yarr::RegularExpression: unknown libregexp result " + std::to_string(result));
    }
    if (!matched) {
        d.lastMatchLength = -1;
        return -1;
    }
    const unsigned shift = input.is8Bit ? 0 : 1;
    const int begin = static_cast<int>((captures[0] - buffer) >> shift);
    const int end = static_cast<int>((captures[1] - buffer) >> shift);
    d.lastMatchLength = end - begin;
    if (matchLength) *matchLength = d.lastMatchLength;
    return begin;
}

int Match(RegularExpressionPrivate& d, const StringReference* string, int startFrom, int* matchLength) {
    if (!string) throw std::invalid_argument("JSC::Yarr::RegularExpression: null string reference");
    if (!d.regExpByteCode) return -1;
    if (!string->impl) return -1;
    const Units input = UnitsOf(string->impl, "JSC::Yarr::RegularExpression: string with null characters and a nonzero length");
    return MatchUnits(d, input, startFrom, matchLength);
}

void Construct(RegularExpression* self, const StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode) {
    if (!self) throw std::invalid_argument("JSC::Yarr::RegularExpression: null this");
    if (!pattern) throw std::invalid_argument("JSC::Yarr::RegularExpression: null pattern reference");
    const Units units = UnitsOf(pattern->impl, "JSC::Yarr::RegularExpression: pattern with null characters and a nonzero length");
    int flags = 0;
    if (caseSensitivity == Jsc::YarrAbi::TextCaseInsensitive) flags |= LRE_FLAG_IGNORECASE;
    if (multilineMode == Jsc::YarrAbi::MultilineEnabled) flags |= LRE_FLAG_MULTILINE;
    if (unicodeMode == Jsc::YarrAbi::UnicodeAwareMode) flags |= LRE_FLAG_UNICODE;
    std::uint8_t* bytecode = Compile(units, flags);
    auto* d = static_cast<RegularExpressionPrivate*>(malloc_nid_postfix(sizeof(RegularExpressionPrivate)));
    if (!d) {
        std::free(bytecode);
        throw std::bad_alloc();
    }
    std::memset(d, 0, sizeof(*d));
    d->refCount = 1;
    d->lastMatchLength = -1;
    d->numSubpatterns = bytecode ? static_cast<std::uint32_t>(lre_get_capture_count(bytecode) - 1) : 0;
    d->regExpByteCode = bytecode;
    self->d = d;
}

void* AllocateStringStorage(std::size_t size) {
    const auto* allocator = RegisteredPersistenceAllocator_nid_no_patch();
    if (!allocator || !allocator->allocate || !allocator->deallocate)
        throw std::runtime_error("JSC::Yarr::replace: the string requires libSceNKWebKit fastMalloc");
    void* storage = allocator->allocate(size);
    if (!storage) throw std::bad_alloc();
    return storage;
}

void DropStringReference(StringImpl* impl) {
    if (!impl) return;
    if (impl->refCount > 2) {
        impl->refCount -= 2;
        return;
    }
    impl->refCount = 0;
}

StringImpl* AllocateString(const std::vector<char16_t>& units, bool is8Bit) {
    if (units.size() > std::numeric_limits<unsigned>::max()) throw std::length_error("JSC::Yarr::replace: result length above UINT_MAX");
    constexpr auto charactersOffset = offsetof(StringImpl, hashAndFlags) + sizeof(StringImpl::hashAndFlags);
    const std::size_t byteLength = units.size() * (is8Bit ? 1u : 2u);
    void* storage = AllocateStringStorage(std::max<std::size_t>(sizeof(StringImpl), charactersOffset + byteLength));
    auto* characters = static_cast<std::uint8_t*>(storage) + charactersOffset;
    auto* impl = new (storage) StringImpl{2u, static_cast<unsigned>(units.size()), characters, is8Bit ? StringImplFlag8Bit : 0u};
    if (is8Bit) {
        for (std::size_t index = 0; index < units.size(); ++index) characters[index] = static_cast<std::uint8_t>(units[index]);
    } else if (byteLength) {
        std::memcpy(characters, units.data(), byteLength);
    }
    return impl;
}

void Replace(StringReference* string, const RegularExpression* target, const StringReference* replacement) {
    if (!string) throw std::invalid_argument("JSC::Yarr::replace: null string reference");
    if (!target || !target->d) throw std::invalid_argument("JSC::Yarr::replace: null regular expression");
    if (!replacement) throw std::invalid_argument("JSC::Yarr::replace: null replacement reference");
    if (!string->impl) return;
    RegularExpressionPrivate& d = *target->d;
    const Units input = UnitsOf(string->impl, "JSC::Yarr::replace: string with null characters and a nonzero length");
    if (input.length > static_cast<unsigned>(INT_MAX)) return;
    const Units replacementUnits = UnitsOf(replacement->impl, "JSC::Yarr::replace: replacement with null characters and a nonzero length");
    std::vector<char16_t> text;
    text.reserve(input.length);
    for (unsigned at = 0; at < input.length; ++at) text.push_back(input.At(at));
    std::vector<char16_t> rebuilt;
    unsigned index = 0;
    bool changed = false;
    while (index < text.size()) {
        const Units current{text.data(), static_cast<unsigned>(text.size()), false};
        int matchLength = 0;
        const int begin = MatchUnits(d, current, static_cast<int>(index), &matchLength);
        if (begin < 0) break;
        const unsigned start = static_cast<unsigned>(begin);
        const unsigned removed = static_cast<unsigned>(matchLength);
        if (!removed && !replacementUnits.length) break;
        rebuilt.assign(text.begin(), text.begin() + start);
        for (unsigned at = 0; at < replacementUnits.length; ++at) rebuilt.push_back(replacementUnits.At(at));
        rebuilt.insert(rebuilt.end(), text.begin() + start + removed, text.end());
        text.swap(rebuilt);
        index = start + replacementUnits.length;
        changed = true;
        if (!removed) break;
    }
    if (!changed) return;
    StringImpl* next = AllocateString(text, input.is8Bit && replacementUnits.is8Bit);
    DropStringReference(string->impl);
    string->impl = next;
}

}

extern "C" {

void APS5_VABI _ZN3JSC4Yarr7replaceERN3WTF6StringERKNS0_17RegularExpressionERKS2_(StringReference* string, const RegularExpression* target, const StringReference* replacement) {
    Replace(string, target, replacement);
}

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE(RegularExpression* self, const StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode) {
    Construct(self, pattern, caseSensitivity, multilineMode, unicodeMode);
}

void APS5_VABI _ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE(RegularExpression* self, const StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode) {
    Construct(self, pattern, caseSensitivity, multilineMode, unicodeMode);
}

int APS5_VABI _ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi(const RegularExpression* self, const StringReference* string, int startFrom, int* matchLength) {
    return Match(*Checked(self, "JSC::Yarr::RegularExpression::match: null this").d, string, startFrom, matchLength);
}

int APS5_VABI _ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE(const RegularExpression* self, const StringReference* string) {
    RegularExpressionPrivate& d = *Checked(self, "JSC::Yarr::RegularExpression::searchRev: null this").d;
    int start = 0;
    int pos;
    int lastPos = -1;
    int lastMatchLength = -1;
    do {
        int matchLength;
        pos = Match(d, string, start, &matchLength);
        if (pos >= 0) {
            if (pos + matchLength > lastPos + lastMatchLength) {
                lastPos = pos;
                lastMatchLength = matchLength;
            }
            start = pos + 1;
        }
    } while (pos != -1);
    d.lastMatchLength = lastMatchLength;
    return lastPos;
}

bool APS5_VABI _ZNK3JSC4Yarr17RegularExpression7isValidEv(const RegularExpression* self) {
    return Checked(self, "JSC::Yarr::RegularExpression::isValid: null this").d->regExpByteCode != nullptr;
}

void APS5_VABI JscRegularExpressionConstruct_nid_no_patch(RegularExpression* self, const StringReference* pattern, int caseSensitivity, int multilineMode, int unicodeMode) {
    Construct(self, pattern, caseSensitivity, multilineMode, unicodeMode);
}

int APS5_VABI JscRegularExpressionMatch_nid_no_patch(const RegularExpression* self, const StringReference* string, int startFrom, int* matchLength) {
    return _ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi(self, string, startFrom, matchLength);
}

void APS5_VABI JscRegularExpressionRelease_nid_no_patch(RegularExpressionPrivate* d) {
    if (!d) return;
    if (d->refCount != 1) {
        --d->refCount;
        return;
    }
    std::free(d->regExpByteCode);
    free_nid_postfix(d);
}

}
