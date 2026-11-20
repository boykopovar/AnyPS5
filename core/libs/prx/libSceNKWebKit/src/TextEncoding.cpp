/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 *
 * Derived from Source/WebCore/PAL/pal/text/TextEncoding.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextEncodingRegistry.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodec.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecLatin1.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecUTF8.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecUTF16.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecReplacement.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecUserDefined.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecSingleByte.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/PAL/pal/text/TextCodecCJK.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba and
 * Source/WebCore/PAL/pal/text/TextCodecICU.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/TextEncodingExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/EncodingTables.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using WebCore::TextEncoding;
using WebCore::UnencodableHandling;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::Vector;

constexpr char16_t replacementCharacter = 0xFFFD;
constexpr std::size_t maxEncodingNameLength = 63;

void Validate(StringView view) {
    if (!view.data && view.length) throw std::invalid_argument("null WTF character buffer");
}

char16_t ViewUnit(StringView view, unsigned index) {
    if (!view.data && index < view.length) throw std::invalid_argument("null WTF character buffer");
    return view.is8Bit ? static_cast<char16_t>(static_cast<const unsigned char*>(view.data)[index])
                       : static_cast<const char16_t*>(view.data)[index];
}

bool IsASCII(unsigned value) {
    return value <= 0x7F;
}

char ToASCIILower(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 32) : value;
}

String MakeString(const std::u16string& buffer, bool force16Bit = false) {
    bool is8Bit = !force16Bit;
    for (char16_t unit : buffer) is8Bit &= unit <= 0xFF;
    void* characters;
    auto* impl = NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(buffer.size()), is8Bit, characters);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        if (is8Bit) static_cast<unsigned char*>(characters)[i] = static_cast<unsigned char>(buffer[i]);
        else static_cast<char16_t*>(characters)[i] = buffer[i];
    }
    return String(impl);
}

class Builder {
public:
    void Append(char16_t unit) { m_buffer.push_back(unit); }

    void AppendCharacter(char32_t codePoint) {
        if (codePoint <= 0xFFFF) {
            Append(static_cast<char16_t>(codePoint));
            return;
        }
        Append(static_cast<char16_t>(0xD800 + ((codePoint - 0x10000) >> 10)));
        Append(static_cast<char16_t>(0xDC00 + ((codePoint - 0x10000) & 0x3FF)));
    }

    String ToString() { return MakeString(m_buffer); }

private:
    std::u16string m_buffer;
};

class CodePointRange {
public:
    explicit CodePointRange(StringView view) : m_view(view) {}

    bool AtEnd() const { return m_index >= m_view.length; }

    char32_t Current() const {
        auto unit = ViewUnit(m_view, m_index);
        if (m_view.is8Bit || unit < 0xD800 || unit > 0xDBFF) return unit;
        if (m_index + 1 < m_view.length) {
            auto next = ViewUnit(m_view, m_index + 1);
            if (next >= 0xDC00 && next <= 0xDFFF) return 0x10000 + ((unit - 0xD800) << 10) + (next - 0xDC00);
        }
        return unit;
    }

    void Advance() {
        auto unit = ViewUnit(m_view, m_index);
        if (!m_view.is8Bit && unit >= 0xD800 && unit <= 0xDBFF && m_index + 1 < m_view.length) {
            auto next = ViewUnit(m_view, m_index + 1);
            if (next >= 0xDC00 && next <= 0xDFFF) {
                m_index += 2;
                return;
            }
        }
        ++m_index;
    }

private:
    StringView m_view;
    unsigned m_index { 0 };
};

class BytesVector {
public:
    BytesVector() = default;

    explicit BytesVector(std::size_t size) {
        if (size) {
            m_buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(size));
            m_capacity = size;
            m_size = size;
        }
    }

    BytesVector(const BytesVector&) = delete;
    BytesVector& operator=(const BytesVector&) = delete;

    BytesVector(BytesVector&& other) noexcept
        : m_buffer(std::exchange(other.m_buffer, nullptr))
        , m_capacity(std::exchange(other.m_capacity, std::size_t { 0 }))
        , m_size(std::exchange(other.m_size, std::size_t { 0 })) {}

    ~BytesVector() {
        if (m_buffer) _ZN3WTF8fastFreeEPv(m_buffer);
    }

    std::size_t Size() const { return m_size; }
    std::size_t Capacity() const { return m_capacity; }
    std::uint8_t* Data() { return m_buffer; }

    void Reserve(std::size_t newCapacity) {
        if (newCapacity <= m_capacity) return;
        auto* buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(newCapacity));
        if (m_size) std::memcpy(buffer, m_buffer, m_size);
        if (m_buffer) _ZN3WTF8fastFreeEPv(m_buffer);
        m_buffer = buffer;
        m_capacity = newCapacity;
    }

    void ReserveInitialCapacity(std::size_t initialCapacity) {
        if (initialCapacity && !m_capacity) {
            m_buffer = static_cast<std::uint8_t*>(_ZN3WTF10fastMallocEm(initialCapacity));
            m_capacity = initialCapacity;
        }
    }

    void Append(std::uint8_t value) {
        if (m_size == m_capacity) Expand(m_size + 1);
        m_buffer[m_size++] = value;
    }

    void AppendRange(const std::uint8_t* data, std::size_t length) {
        if (m_size + length > m_capacity) Expand(m_size + length);
        std::memcpy(m_buffer + m_size, data, length);
        m_size += length;
    }

    void Shrink(std::size_t size) { m_size = size; }

    void Release() {
        m_buffer = nullptr;
        m_capacity = 0;
        m_size = 0;
    }

    Vector<std::uint8_t> Take() {
        Vector<std::uint8_t> result;
        result.buffer = m_buffer;
        result.capacity = static_cast<unsigned>(m_capacity);
        result.size = static_cast<unsigned>(m_size);
        Release();
        return result;
    }

private:
    void Expand(std::size_t newMinCapacity) {
        Reserve(std::max(newMinCapacity, std::max<std::size_t>(16, m_capacity + m_capacity / 4 + 1)));
    }

    std::uint8_t* m_buffer { nullptr };
    std::size_t m_capacity { 0 };
    std::size_t m_size { 0 };
};

int GetUnencodableReplacement(char32_t codePoint, UnencodableHandling handling, std::array<char, 32>& replacement) {
    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) codePoint = replacementCharacter;
    switch (handling) {
    case UnencodableHandling::Entities:
        return std::snprintf(replacement.data(), replacement.size(), "&#%u;", static_cast<unsigned>(codePoint));
    case UnencodableHandling::URLEncodedEntities:
        return std::snprintf(replacement.data(), replacement.size(), "%%26%%23%u%%3B", static_cast<unsigned>(codePoint));
    }
    replacement.data()[0] = 0;
    return 0;
}

void AppendDecimal(char32_t codePoint, BytesVector& result) {
    char buffer[10];
    auto converted = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<std::uint32_t>(codePoint));
    if (converted.ec != std::errc {}) throw std::runtime_error("unencodable character formatting failed");
    result.AppendRange(reinterpret_cast<const std::uint8_t*>(buffer), static_cast<std::size_t>(converted.ptr - buffer));
}

void EntityUnencodableHandler(char32_t codePoint, BytesVector& result) {
    result.Reserve(result.Size() + 3 + 10);
    result.Append('&');
    result.Append('#');
    AppendDecimal(codePoint, result);
    result.Append(';');
}

void UrlEncodedEntityUnencodableHandler(char32_t codePoint, BytesVector& result) {
    result.Reserve(result.Size() + 9 + 10);
    result.Append('%');
    result.Append('2');
    result.Append('6');
    result.Append('%');
    result.Append('2');
    result.Append('3');
    AppendDecimal(codePoint, result);
    result.Append('%');
    result.Append('3');
    result.Append('B');
}

void Unencodable(char32_t codePoint, UnencodableHandling handling, BytesVector& result) {
    switch (handling) {
    case UnencodableHandling::Entities:
        return EntityUnencodableHandler(codePoint, result);
    case UnencodableHandling::URLEncodedEntities:
        return UrlEncodedEntityUnencodableHandler(codePoint, result);
    }
    return EntityUnencodableHandler(codePoint, result);
}

constexpr char16_t latin1ConversionTable[256] = {
    0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007,
    0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x000F,
    0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017,
    0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D, 0x001E, 0x001F,
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027,
    0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047,
    0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057,
    0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067,
    0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077,
    0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x007F,
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};
String DecodeLatin1(const char* bytes, std::size_t length) {
    if (!length) return MakeString({});
    std::u16string buffer(length, 0);
    for (std::size_t i = 0; i < length; ++i) buffer[i] = latin1ConversionTable[static_cast<unsigned char>(bytes[i])];
    return MakeString(buffer);
}

Vector<std::uint8_t> EncodeComplexWindowsLatin1(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto character = points.Current();
        std::uint8_t b = static_cast<std::uint8_t>(character);
        if (b != character || (character & 0xE0) == 0x80) {
            bool found = false;
            for (b = 0x80; b < 0xA0; ++b) {
                if (latin1ConversionTable[b] == character) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::array<char, 32> replacement;
                int replacementLength = GetUnencodableReplacement(character, handling, replacement);
                result.AppendRange(reinterpret_cast<const std::uint8_t*>(replacement.data()), static_cast<std::size_t>(replacementLength));
                continue;
            }
        }
        result.Append(b);
    }
    return result.Take();
}

Vector<std::uint8_t> EncodeLatin1(StringView string, UnencodableHandling handling) {
    Validate(string);
    {
        BytesVector result(string.length);
        auto* bytes = result.Data();
        unsigned ored = 0;
        for (unsigned i = 0; i < string.length; ++i) {
            auto character = ViewUnit(string, i);
            bytes[i] = static_cast<std::uint8_t>(character);
            ored |= character;
        }
        if (!(ored & 0xFF80)) return result.Take();
    }
    return EncodeComplexWindowsLatin1(string, handling);
}

const std::uint8_t nonASCIISequenceLengths[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

constexpr int nonCharacter = -1;

int NonASCIISequenceLength(std::uint8_t firstByte) {
    return nonASCIISequenceLengths[firstByte];
}

int DecodeNonASCIISequence(const std::uint8_t* sequence, int& length) {
    if (length == 2) {
        if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
        return ((sequence[0] << 6) + sequence[1]) - 0x00003080;
    }
    if (length == 3) {
        switch (sequence[0]) {
        case 0xE0:
            if (sequence[1] < 0xA0 || sequence[1] > 0xBF) {
                length = 1;
                return nonCharacter;
            }
            break;
        case 0xED:
            if (sequence[1] < 0x80 || sequence[1] > 0x9F) {
                length = 1;
                return nonCharacter;
            }
            break;
        default:
            if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
                length = 1;
                return nonCharacter;
            }
        }
        if (sequence[2] < 0x80 || sequence[2] > 0xBF) {
            length = 2;
            return nonCharacter;
        }
        return ((sequence[0] << 12) + (sequence[1] << 6) + sequence[2]) - 0x000E2080;
    }
    switch (sequence[0]) {
    case 0xF0:
        if (sequence[1] < 0x90 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
        break;
    case 0xF4:
        if (sequence[1] < 0x80 || sequence[1] > 0x8F) {
            length = 1;
            return nonCharacter;
        }
        break;
    default:
        if (sequence[1] < 0x80 || sequence[1] > 0xBF) {
            length = 1;
            return nonCharacter;
        }
    }
    if (sequence[2] < 0x80 || sequence[2] > 0xBF) {
        length = 2;
        return nonCharacter;
    }
    if (sequence[3] < 0x80 || sequence[3] > 0xBF) {
        length = 3;
        return nonCharacter;
    }
    return ((sequence[0] << 18) + (sequence[1] << 12) + (sequence[2] << 6) + sequence[3]) - 0x03C82080;
}

void AppendUTF16CodePoint(std::u16string& buffer, int character) {
    if (character <= 0xFFFF) {
        buffer.push_back(static_cast<char16_t>(character));
        return;
    }
    buffer.push_back(static_cast<char16_t>(0xD800 + ((character - 0x10000) >> 10)));
    buffer.push_back(static_cast<char16_t>(0xDC00 + ((character - 0x10000) & 0x3FF)));
}

String DecodeUTF8(const std::uint8_t* source, std::size_t length, bool stopOnError, bool& sawError) {
    const std::uint8_t* end = source + length;
    std::u16string buffer;
    bool wide = false;
    while (source < end) {
        if (*source < 0x80) {
            buffer.push_back(static_cast<char16_t>(*source++));
            continue;
        }
        int count = NonASCIISequenceLength(*source);
        if (!count) {
            sawError = true;
            if (stopOnError) break;
            wide = true;
            buffer.push_back(replacementCharacter);
            ++source;
            continue;
        }
        if (count > static_cast<int>(end - source)) {
            std::uint8_t sequence[4] = { 0, 0, 0, 0 };
            std::memcpy(sequence, source, static_cast<std::size_t>(end - source));
            int validated = count;
            int character = DecodeNonASCIISequence(sequence, validated);
            sawError = true;
            wide = true;
            if (stopOnError) break;
            buffer.push_back(replacementCharacter);
            source += static_cast<std::size_t>(validated);
            continue;
        }
        int consumed = count;
        int character = DecodeNonASCIISequence(source, consumed);
        if (character == nonCharacter) {
            sawError = true;
            if (stopOnError) break;
            wide = true;
            buffer.push_back(replacementCharacter);
            source += static_cast<std::size_t>(consumed);
            continue;
        }
        if (character > 0xFF) wide = true;
        source += static_cast<std::size_t>(consumed);
        AppendUTF16CodePoint(buffer, character);
    }
    return MakeString(buffer, wide);
}

Vector<std::uint8_t> EncodeUTF8(StringView string) {
    Validate(string);
    BytesVector bytes(static_cast<std::size_t>(string.length) * 3);
    std::size_t written = 0;
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto character = points.Current();
        auto* output = bytes.Data() + written;
        if (character < 0x80) {
            output[0] = static_cast<std::uint8_t>(character);
            written += 1;
        } else if (character < 0x800) {
            output[0] = static_cast<std::uint8_t>(0xC0 | (character >> 6));
            output[1] = static_cast<std::uint8_t>(0x80 | (character & 0x3F));
            written += 2;
        } else if (character < 0x10000) {
            output[0] = static_cast<std::uint8_t>(0xE0 | (character >> 12));
            output[1] = static_cast<std::uint8_t>(0x80 | ((character >> 6) & 0x3F));
            output[2] = static_cast<std::uint8_t>(0x80 | (character & 0x3F));
            written += 3;
        } else {
            output[0] = static_cast<std::uint8_t>(0xF0 | (character >> 18));
            output[1] = static_cast<std::uint8_t>(0x80 | ((character >> 12) & 0x3F));
            output[2] = static_cast<std::uint8_t>(0x80 | ((character >> 6) & 0x3F));
            output[3] = static_cast<std::uint8_t>(0x80 | (character & 0x3F));
            written += 4;
        }
    }
    bytes.Shrink(written);
    return bytes.Take();
}

String DecodeUTF16(const std::uint8_t* bytes, std::size_t length, bool flush, bool littleEndian, bool& sawError) {
    const auto* p = bytes;
    const auto* const end = p + length;
    const auto* const endMinusOneOrNull = end ? end - 1 : nullptr;

    Builder result;
    bool shouldStripByteOrderMark = false;
    std::optional<std::uint8_t> leadByte;
    std::optional<char16_t> leadSurrogate;

    auto processCodeUnit = [&](char16_t codeUnit) {
        if (std::exchange(shouldStripByteOrderMark, false) && codeUnit == 0xFEFF) return;
        if (leadSurrogate) {
            auto surrogate = *std::exchange(leadSurrogate, std::nullopt);
            if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
                result.AppendCharacter(0x10000 + ((surrogate - 0xD800) << 10) + (codeUnit - 0xDC00));
                return;
            }
            sawError = true;
            result.Append(replacementCharacter);
        }
        if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
            leadSurrogate = codeUnit;
            return;
        }
        if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF) {
            sawError = true;
            result.Append(replacementCharacter);
            return;
        }
        result.Append(codeUnit);
    };
    auto processBytesLE = [&](std::uint8_t first, std::uint8_t second) {
        processCodeUnit(static_cast<char16_t>(first | (second << 8)));
    };
    auto processBytesBE = [&](std::uint8_t first, std::uint8_t second) {
        processCodeUnit(static_cast<char16_t>((first << 8) | second));
    };

    if (leadByte && p < end) {
        auto byte = *std::exchange(leadByte, std::nullopt);
        if (littleEndian) processBytesLE(byte, p[0]);
        else processBytesBE(byte, p[0]);
        p++;
    }

    if (littleEndian) {
        while (p < endMinusOneOrNull) {
            processBytesLE(p[0], p[1]);
            p += 2;
        }
    } else {
        while (p < endMinusOneOrNull) {
            processBytesBE(p[0], p[1]);
            p += 2;
        }
    }

    if (p && p == endMinusOneOrNull) leadByte = p[0];

    if (flush) {
        shouldStripByteOrderMark = false;
        if (leadByte || leadSurrogate) {
            leadByte = std::nullopt;
            leadSurrogate = std::nullopt;
            sawError = true;
            result.Append(replacementCharacter);
        }
    }

    return result.ToString();
}

Vector<std::uint8_t> EncodeUTF16(StringView string, bool littleEndian) {
    Validate(string);
    BytesVector result(static_cast<std::size_t>(string.length) * 2);
    auto* bytes = result.Data();
    std::size_t index = 0;
    for (unsigned i = 0; i < string.length; ++i) {
        auto character = ViewUnit(string, i);
        if (littleEndian) {
            bytes[index++] = static_cast<std::uint8_t>(character);
            bytes[index++] = static_cast<std::uint8_t>(character >> 8);
        } else {
            bytes[index++] = static_cast<std::uint8_t>(character >> 8);
            bytes[index++] = static_cast<std::uint8_t>(character);
        }
    }
    return result.Take();
}

String DecodeUserDefined(const char* bytes, std::size_t length) {
    Builder result;
    for (std::size_t i = 0; i < length; ++i) {
        int c = static_cast<signed char>(bytes[i]);
        result.Append(static_cast<char16_t>(c & 0xF7FF));
    }
    return result.ToString();
}

Vector<std::uint8_t> EncodeComplexUserDefined(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto character = points.Current();
        std::int8_t signedByte = static_cast<std::int8_t>(character);
        if ((signedByte & 0xF7FF) == character) result.Append(static_cast<std::uint8_t>(signedByte));
        else {
            std::array<char, 32> replacement;
            int replacementLength = GetUnencodableReplacement(character, handling, replacement);
            result.AppendRange(reinterpret_cast<const std::uint8_t*>(replacement.data()), static_cast<std::size_t>(replacementLength));
        }
    }
    return result.Take();
}

Vector<std::uint8_t> EncodeUserDefined(StringView string, UnencodableHandling handling) {
    Validate(string);
    {
        BytesVector result(string.length);
        auto* bytes = result.Data();
        unsigned ored = 0;
        for (unsigned i = 0; i < string.length; ++i) {
            auto character = ViewUnit(string, i);
            bytes[i] = static_cast<std::uint8_t>(character);
            ored |= character;
        }
        if (!(ored & 0xFF80)) return result.Take();
    }
    return EncodeComplexUserDefined(string, handling);
}

String DecodeReplacement(bool& sawError) {
    sawError = true;
    return MakeString({ replacementCharacter });
}

enum class SingleByteEncoding : std::uint8_t {
    ISO_8859_3,
    ISO_8859_6,
    ISO_8859_7,
    ISO_8859_8,
    Windows_874,
    Windows_1253,
    Windows_1255,
    Windows_1257,
    IBM866,
    KOI8U
};

using SingleByteDecodeTable = std::array<char16_t, 128>;
using SingleByteEncodeEntry = std::pair<char16_t, std::uint8_t>;

static constexpr SingleByteDecodeTable iso88593 {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x0126, 0x02D8, 0x00A3, 0x00A4, 0xFFFD, 0x0124, 0x00A7, 0x00A8, 0x0130, 0x015E, 0x011E, 0x0134, 0x00AD, 0xFFFD, 0x017B,
    0x00B0, 0x0127, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x0125, 0x00B7, 0x00B8, 0x0131, 0x015F, 0x011F, 0x0135, 0x00BD, 0xFFFD, 0x017C,
    0x00C0, 0x00C1, 0x00C2, 0xFFFD, 0x00C4, 0x010A, 0x0108, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0xFFFD, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x0120, 0x00D6, 0x00D7, 0x011C, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x016C, 0x015C, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0xFFFD, 0x00E4, 0x010B, 0x0109, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0xFFFD, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x0121, 0x00F6, 0x00F7, 0x011D, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x016D, 0x015D, 0x02D9
};


static constexpr SingleByteDecodeTable iso88596 {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0xFFFD, 0xFFFD, 0xFFFD, 0x00A4, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x060C, 0x00AD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x061B, 0xFFFD, 0xFFFD, 0xFFFD, 0x061F,
    0xFFFD, 0x0621, 0x0622, 0x0623, 0x0624, 0x0625, 0x0626, 0x0627, 0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
    0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x0636, 0x0637, 0x0638, 0x0639, 0x063A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0x0640, 0x0641, 0x0642, 0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648, 0x0649, 0x064A, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F,
    0x0650, 0x0651, 0x0652, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD
};


static constexpr SingleByteDecodeTable iso88597 {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x2018, 0x2019, 0x00A3, 0x20AC, 0x20AF, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x037A, 0x00AB, 0x00AC, 0x00AD, 0xFFFD, 0x2015,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0384, 0x0385, 0x0386, 0x00B7, 0x0388, 0x0389, 0x038A, 0x00BB, 0x038C, 0x00BD, 0x038E, 0x038F,
    0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
    0x03A0, 0x03A1, 0xFFFD, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
    0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
    0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0xFFFD
};


static constexpr SingleByteDecodeTable iso88598 {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0xFFFD, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x2017,
    0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
    0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7, 0x05E8, 0x05E9, 0x05EA, 0xFFFD, 0xFFFD, 0x200E, 0x200F, 0xFFFD
};


static constexpr SingleByteDecodeTable windows874 {
    0x20AC, 0x0081, 0x0082, 0x0083, 0x0084, 0x2026, 0x0086, 0x0087, 0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x0E01, 0x0E02, 0x0E03, 0x0E04, 0x0E05, 0x0E06, 0x0E07, 0x0E08, 0x0E09, 0x0E0A, 0x0E0B, 0x0E0C, 0x0E0D, 0x0E0E, 0x0E0F,
    0x0E10, 0x0E11, 0x0E12, 0x0E13, 0x0E14, 0x0E15, 0x0E16, 0x0E17, 0x0E18, 0x0E19, 0x0E1A, 0x0E1B, 0x0E1C, 0x0E1D, 0x0E1E, 0x0E1F,
    0x0E20, 0x0E21, 0x0E22, 0x0E23, 0x0E24, 0x0E25, 0x0E26, 0x0E27, 0x0E28, 0x0E29, 0x0E2A, 0x0E2B, 0x0E2C, 0x0E2D, 0x0E2E, 0x0E2F,
    0x0E30, 0x0E31, 0x0E32, 0x0E33, 0x0E34, 0x0E35, 0x0E36, 0x0E37, 0x0E38, 0x0E39, 0x0E3A, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0x0E3F,
    0x0E40, 0x0E41, 0x0E42, 0x0E43, 0x0E44, 0x0E45, 0x0E46, 0x0E47, 0x0E48, 0x0E49, 0x0E4A, 0x0E4B, 0x0E4C, 0x0E4D, 0x0E4E, 0x0E4F,
    0x0E50, 0x0E51, 0x0E52, 0x0E53, 0x0E54, 0x0E55, 0x0E56, 0x0E57, 0x0E58, 0x0E59, 0x0E5A, 0x0E5B, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD
};


static constexpr SingleByteDecodeTable windows1253 {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x0088, 0x2030, 0x008A, 0x2039, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122, 0x009A, 0x203A, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x0385, 0x0386, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0xFFFD, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x2015,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x0384, 0x00B5, 0x00B6, 0x00B7, 0x0388, 0x0389, 0x038A, 0x00BB, 0x038C, 0x00BD, 0x038E, 0x038F,
    0x0390, 0x0391, 0x0392, 0x0393, 0x0394, 0x0395, 0x0396, 0x0397, 0x0398, 0x0399, 0x039A, 0x039B, 0x039C, 0x039D, 0x039E, 0x039F,
    0x03A0, 0x03A1, 0xFFFD, 0x03A3, 0x03A4, 0x03A5, 0x03A6, 0x03A7, 0x03A8, 0x03A9, 0x03AA, 0x03AB, 0x03AC, 0x03AD, 0x03AE, 0x03AF,
    0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5, 0x03B6, 0x03B7, 0x03B8, 0x03B9, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BE, 0x03BF,
    0x03C0, 0x03C1, 0x03C2, 0x03C3, 0x03C4, 0x03C5, 0x03C6, 0x03C7, 0x03C8, 0x03C9, 0x03CA, 0x03CB, 0x03CC, 0x03CD, 0x03CE, 0xFFFD
};

static constexpr SingleByteDecodeTable windows1255 {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x008A, 0x2039, 0x008C, 0x008D, 0x008E, 0x008F,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x009A, 0x203A, 0x009C, 0x009D, 0x009E, 0x009F,
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x20AA, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00D7, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00F7, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x05B0, 0x05B1, 0x05B2, 0x05B3, 0x05B4, 0x05B5, 0x05B6, 0x05B7, 0x05B8, 0x05B9, 0x05BA, 0x05BB, 0x05BC, 0x05BD, 0x05BE, 0x05BF,
    0x05C0, 0x05C1, 0x05C2, 0x05C3, 0x05F0, 0x05F1, 0x05F2, 0x05F3, 0x05F4, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD,
    0x05D0, 0x05D1, 0x05D2, 0x05D3, 0x05D4, 0x05D5, 0x05D6, 0x05D7, 0x05D8, 0x05D9, 0x05DA, 0x05DB, 0x05DC, 0x05DD, 0x05DE, 0x05DF,
    0x05E0, 0x05E1, 0x05E2, 0x05E3, 0x05E4, 0x05E5, 0x05E6, 0x05E7, 0x05E8, 0x05E9, 0x05EA, 0xFFFD, 0xFFFD, 0x200E, 0x200F, 0xFFFD
};


static constexpr SingleByteDecodeTable windows1257 {
    0x20AC, 0x0081, 0x201A, 0x0083, 0x201E, 0x2026, 0x2020, 0x2021, 0x0088, 0x2030, 0x008A, 0x2039, 0x008C, 0x00A8, 0x02C7, 0x00B8,
    0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x0098, 0x2122, 0x009A, 0x203A, 0x009C, 0x00AF, 0x02DB, 0x009F,
    0x00A0, 0xFFFD, 0x00A2, 0x00A3, 0x00A4, 0xFFFD, 0x00A6, 0x00A7, 0x00D8, 0x00A9, 0x0156, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00C6,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00F8, 0x00B9, 0x0157, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00E6,
    0x0104, 0x012E, 0x0100, 0x0106, 0x00C4, 0x00C5, 0x0118, 0x0112, 0x010C, 0x00C9, 0x0179, 0x0116, 0x0122, 0x0136, 0x012A, 0x013B,
    0x0160, 0x0143, 0x0145, 0x00D3, 0x014C, 0x00D5, 0x00D6, 0x00D7, 0x0172, 0x0141, 0x015A, 0x016A, 0x00DC, 0x017B, 0x017D, 0x00DF,
    0x0105, 0x012F, 0x0101, 0x0107, 0x00E4, 0x00E5, 0x0119, 0x0113, 0x010D, 0x00E9, 0x017A, 0x0117, 0x0123, 0x0137, 0x012B, 0x013C,
    0x0161, 0x0144, 0x0146, 0x00F3, 0x014D, 0x00F5, 0x00F6, 0x00F7, 0x0173, 0x0142, 0x015B, 0x016B, 0x00FC, 0x017C, 0x017E, 0x02D9
};


static constexpr SingleByteDecodeTable koi8u {
    0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524, 0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
    0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248, 0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
    0x2550, 0x2551, 0x2552, 0x0451, 0x0454, 0x2554, 0x0456, 0x0457, 0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x0491, 0x045E, 0x255E,
    0x255F, 0x2560, 0x2561, 0x0401, 0x0404, 0x2563, 0x0406, 0x0407, 0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x0490, 0x040E, 0x00A9,
    0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433, 0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
    0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432, 0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
    0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413, 0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
    0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412, 0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A
};


static constexpr SingleByteDecodeTable ibm866 {
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427, 0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447, 0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
    0x0401, 0x0451, 0x0404, 0x0454, 0x0407, 0x0457, 0x040E, 0x045E, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x2116, 0x00A4, 0x25A0, 0x00A0
};
template<const SingleByteDecodeTable& decodeTable>
const SingleByteEncodeEntry* BuildSingleByteEncodeTable(std::size_t& size) {
    static const SingleByteEncodeEntry* entries;
    static std::size_t count;
    static std::once_flag once;
    std::call_once(once, [&] {
        count = std::size(decodeTable) - static_cast<std::size_t>(std::count(std::begin(decodeTable), std::end(decodeTable), replacementCharacter));
        auto* mutableEntries = new SingleByteEncodeEntry[count];
        std::size_t j = 0;
        for (std::size_t i = 0; i < std::size(decodeTable); i++) {
            if (decodeTable[i] != replacementCharacter) mutableEntries[j++] = { decodeTable[i], static_cast<std::uint8_t>(i + 0x80) };
        }
        std::sort(mutableEntries, mutableEntries + count, [](const SingleByteEncodeEntry& a, const SingleByteEncodeEntry& b) {
            return a.first < b.first;
        });
        entries = mutableEntries;
    });
    size = count;
    return entries;
}

const SingleByteDecodeTable& SingleByteDecodeTableFor(SingleByteEncoding encoding) {
    switch (encoding) {
    case SingleByteEncoding::ISO_8859_3: return iso88593;
    case SingleByteEncoding::ISO_8859_6: return iso88596;
    case SingleByteEncoding::ISO_8859_7: return iso88597;
    case SingleByteEncoding::ISO_8859_8: return iso88598;
    case SingleByteEncoding::Windows_874: return windows874;
    case SingleByteEncoding::Windows_1253: return windows1253;
    case SingleByteEncoding::Windows_1255: return windows1255;
    case SingleByteEncoding::Windows_1257: return windows1257;
    case SingleByteEncoding::IBM866: return ibm866;
    case SingleByteEncoding::KOI8U: return koi8u;
    }
    throw std::logic_error("unknown single-byte text encoding");
}

const SingleByteEncodeEntry* SingleByteEncodeTableFor(SingleByteEncoding encoding, std::size_t& size) {
    switch (encoding) {
    case SingleByteEncoding::ISO_8859_3: return BuildSingleByteEncodeTable<iso88593>(size);
    case SingleByteEncoding::ISO_8859_6: return BuildSingleByteEncodeTable<iso88596>(size);
    case SingleByteEncoding::ISO_8859_7: return BuildSingleByteEncodeTable<iso88597>(size);
    case SingleByteEncoding::ISO_8859_8: return BuildSingleByteEncodeTable<iso88598>(size);
    case SingleByteEncoding::Windows_874: return BuildSingleByteEncodeTable<windows874>(size);
    case SingleByteEncoding::Windows_1253: return BuildSingleByteEncodeTable<windows1253>(size);
    case SingleByteEncoding::Windows_1255: return BuildSingleByteEncodeTable<windows1255>(size);
    case SingleByteEncoding::Windows_1257: return BuildSingleByteEncodeTable<windows1257>(size);
    case SingleByteEncoding::IBM866: return BuildSingleByteEncodeTable<ibm866>(size);
    case SingleByteEncoding::KOI8U: return BuildSingleByteEncodeTable<koi8u>(size);
    }
    throw std::logic_error("unknown single-byte text encoding");
}

String DecodeSingleByte(SingleByteEncoding encoding, const std::uint8_t* bytes, std::size_t length, bool stopOnError, bool& sawError) {
    const auto& table = SingleByteDecodeTableFor(encoding);
    Builder result;
    auto parseByte = [&](std::uint8_t byte) {
        if (IsASCII(byte)) {
            result.Append(static_cast<char16_t>(byte));
            return;
        }
        char16_t codePoint = table[byte - 0x80];
        if (codePoint == replacementCharacter) sawError = true;
        result.Append(codePoint);
    };
    if (stopOnError) {
        for (std::size_t i = 0; i < length; i++) {
            parseByte(bytes[i]);
            if (sawError) return result.ToString();
        }
    } else {
        for (std::size_t i = 0; i < length; i++) parseByte(bytes[i]);
    }
    return result.ToString();
}

Vector<std::uint8_t> EncodeSingleByte(SingleByteEncoding encoding, StringView string, UnencodableHandling handling) {
    Validate(string);
    std::size_t size;
    const auto* table = SingleByteEncodeTableFor(encoding, size);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        auto byte = PAL::findFirstInSortedPairs(std::span<const SingleByteEncodeEntry>(table, size), codePoint);
        if (!byte) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        result.Append(*byte);
    }
    return result.Take();
}

using PAL::jis0208;
using PAL::jis0212;
using PAL::big5;
using PAL::eucKR;
using PAL::gb18030;

enum class CJKEncoding : std::uint8_t {
    EUC_JP,
    ISO2022JP,
    Shift_JIS,
    EUC_KR,
    Big5,
    GBK,
    GB18030
};

enum class SawError : bool { No, Yes };

enum class ISO2022JPDecoderState : std::uint8_t { ASCII, Roman, Katakana, LeadByte, TrailByte, EscapeStart, Escape };

struct CJKState {
    std::uint8_t lead { 0x00 };
    std::optional<std::uint8_t> prependedByte;
    bool jis0212 { false };
    ISO2022JPDecoderState iso2022JPDecoderState { ISO2022JPDecoderState::ASCII };
    ISO2022JPDecoderState iso2022JPDecoderOutputState { ISO2022JPDecoderState::ASCII };
    bool iso2022JPOutput { false };
    std::optional<std::uint8_t> iso2022JPSecondPrependedByte;
    std::uint8_t gb18030First { 0x00 };
    std::uint8_t gb18030Second { 0x00 };
    std::uint8_t gb18030Third { 0x00 };
};

using JIS0208EncodeIndex = std::array<std::pair<char16_t, std::uint16_t>, 7724>;
using EUCKREncodingIndex = std::array<std::pair<char16_t, std::uint16_t>, 17048>;
using Big5EncodeIndex = std::array<std::pair<char32_t, std::uint16_t>, 18590 - 3904>;
using GB18030EncodeIndex = std::array<std::pair<char16_t, std::uint16_t>, 23940>;

const JIS0208EncodeIndex& JIS0208EncodeIndexTable() {
    static const JIS0208EncodeIndex table = [] {
        JIS0208EncodeIndex index;
        const auto& source = jis0208();
        for (std::size_t i = 0; i < source.size(); i++) index[i] = { source[i].second, source[i].first };
        PAL::stableSortByFirst(index);
        return index;
    }();
    return table;
}

const EUCKREncodingIndex& EUCKREncodingIndexTable() {
    static const EUCKREncodingIndex table = [] {
        EUCKREncodingIndex index;
        const auto& source = eucKR();
        for (std::size_t i = 0; i < source.size(); i++) index[i] = { source[i].second, source[i].first };
        PAL::sortByFirst(index);
        return index;
    }();
    return table;
}

const Big5EncodeIndex& Big5EncodeIndexTable() {
    static const Big5EncodeIndex table = [] {
        Big5EncodeIndex index;
        const auto& source = big5();
        for (std::size_t i = 3904; i < source.size(); i++) index[i - 3904] = { source[i].second, source[i].first };
        PAL::stableSortByFirst(index);
        return index;
    }();
    return table;
}

const GB18030EncodeIndex& GB18030EncodeIndexTable() {
    static const GB18030EncodeIndex table = [] {
        GB18030EncodeIndex index;
        const auto& source = gb18030();
        for (std::size_t i = 0; i < source.size(); i++) index[i] = { source[i], static_cast<std::uint16_t>(i) };
        PAL::stableSortByFirst(index);
        return index;
    }();
    return table;
}

template<typename Parser>
void DecodeCommonInto(CJKState& state, Builder& result, const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError, Parser parser) {
    if (state.prependedByte && parser(*std::exchange(state.prependedByte, std::nullopt), result) == SawError::Yes) {
        sawError = true;
        result.Append(replacementCharacter);
        if (stopOnError) {
            state.lead = 0x00;
            return;
        }
    }
    for (std::size_t i = 0; i < length; i++) {
        if (parser(bytes[i], result) == SawError::Yes) {
            sawError = true;
            result.Append(replacementCharacter);
            if (stopOnError) {
                state.lead = 0x00;
                return;
            }
        }
        if (state.prependedByte && parser(*std::exchange(state.prependedByte, std::nullopt), result) == SawError::Yes) {
            sawError = true;
            result.Append(replacementCharacter);
            if (stopOnError) {
                state.lead = 0x00;
                return;
            }
        }
    }
    if (flush && state.lead) {
        state.lead = 0x00;
        sawError = true;
        result.Append(replacementCharacter);
    }
}

String DecodeEUCJP(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    Builder result;
    auto parser = [&](std::uint8_t byte, Builder& builder) -> SawError {
        if (std::uint8_t lead = std::exchange(state.lead, 0x00)) {
            if (lead == 0x8E && byte >= 0xA1 && byte <= 0xDF) {
                builder.Append(static_cast<char16_t>(0xFF61 - 0xA1 + byte));
                return SawError::No;
            }
            if (lead == 0x8F && byte >= 0xA1 && byte <= 0xFE) {
                state.jis0212 = true;
                state.lead = byte;
                return SawError::No;
            }
            if (lead >= 0xA1 && lead <= 0xFE && byte >= 0xA1 && byte <= 0xFE) {
                std::uint16_t pointer = static_cast<std::uint16_t>((lead - 0xA1) * 94 + byte - 0xA1);
                auto codePoint = std::exchange(state.jis0212, false) ? PAL::findFirstInSortedPairs(jis0212(), pointer)
                                                                     : PAL::findFirstInSortedPairs(jis0208(), pointer);
                if (codePoint) {
                    builder.Append(*codePoint);
                    return SawError::No;
                }
            }
            if (IsASCII(byte)) state.prependedByte = byte;
            return SawError::Yes;
        }
        if (IsASCII(byte)) {
            builder.Append(static_cast<char16_t>(byte));
            return SawError::No;
        }
        if (byte == 0x8E || byte == 0x8F || (byte >= 0xA1 && byte <= 0xFE)) {
            state.lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    };
    DecodeCommonInto(state, result, bytes, length, flush, stopOnError, sawError, parser);
    return result.ToString();
}

String DecodeISO2022JP(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    auto byteParser = [&](std::uint8_t byte, Builder& result) -> SawError {
        switch (state.iso2022JPDecoderState) {
        case ISO2022JPDecoderState::ASCII:
            if (byte == 0x1B) {
                state.iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B) {
                state.iso2022JPOutput = false;
                result.Append(static_cast<char16_t>(byte));
                break;
            }
            state.iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::Roman:
            if (byte == 0x1B) {
                state.iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte == 0x5C) {
                state.iso2022JPOutput = false;
                result.Append(static_cast<char16_t>(0x00A5));
                break;
            }
            if (byte == 0x7E) {
                state.iso2022JPOutput = false;
                result.Append(static_cast<char16_t>(0x203E));
                break;
            }
            if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B && byte != 0x5C && byte != 0x7E) {
                state.iso2022JPOutput = false;
                result.Append(static_cast<char16_t>(byte));
                break;
            }
            state.iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::Katakana:
            if (byte == 0x1B) {
                state.iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte >= 0x21 && byte <= 0x5F) {
                state.iso2022JPOutput = false;
                result.Append(static_cast<char16_t>(0xFF61 - 0x21 + byte));
                break;
            }
            state.iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::LeadByte:
            if (byte == 0x1B) {
                state.iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                break;
            }
            if (byte >= 0x21 && byte <= 0x7E) {
                state.iso2022JPOutput = false;
                state.lead = byte;
                state.iso2022JPDecoderState = ISO2022JPDecoderState::TrailByte;
                break;
            }
            state.iso2022JPOutput = false;
            return SawError::Yes;
        case ISO2022JPDecoderState::TrailByte:
            if (byte == 0x1B) {
                state.iso2022JPDecoderState = ISO2022JPDecoderState::EscapeStart;
                return SawError::Yes;
            }
            state.iso2022JPDecoderState = ISO2022JPDecoderState::LeadByte;
            if (byte >= 0x21 && byte <= 0x7E) {
                std::uint16_t pointer = static_cast<std::uint16_t>((state.lead - 0x21) * 94 + byte - 0x21);
                if (auto codePoint = PAL::findFirstInSortedPairs(jis0208(), pointer)) {
                    result.Append(*codePoint);
                    break;
                }
                return SawError::Yes;
            }
            return SawError::Yes;
        case ISO2022JPDecoderState::EscapeStart:
            if (byte == 0x24 || byte == 0x28) {
                state.lead = byte;
                state.iso2022JPDecoderState = ISO2022JPDecoderState::Escape;
                break;
            }
            state.prependedByte = byte;
            state.iso2022JPOutput = false;
            state.iso2022JPDecoderState = state.iso2022JPDecoderOutputState;
            return SawError::Yes;
        case ISO2022JPDecoderState::Escape: {
            std::uint8_t lead = std::exchange(state.lead, 0x00);
            std::optional<ISO2022JPDecoderState> escapeState;
            if (lead == 0x28) {
                if (byte == 0x42) escapeState = ISO2022JPDecoderState::ASCII;
                else if (byte == 0x4A) escapeState = ISO2022JPDecoderState::Roman;
                else if (byte == 0x49) escapeState = ISO2022JPDecoderState::Katakana;
            } else if (lead == 0x24 && (byte == 0x40 || byte == 0x42)) escapeState = ISO2022JPDecoderState::LeadByte;
            if (escapeState) {
                state.iso2022JPDecoderState = *escapeState;
                state.iso2022JPDecoderOutputState = *escapeState;
                if (std::exchange(state.iso2022JPOutput, true)) return SawError::Yes;
                break;
            }
            state.prependedByte = lead;
            state.iso2022JPSecondPrependedByte = byte;
            state.iso2022JPOutput = false;
            state.iso2022JPDecoderState = state.iso2022JPDecoderOutputState;
            return SawError::Yes;
        }
        }
        return SawError::No;
    };

    Builder result;
    if (state.prependedByte && byteParser(*std::exchange(state.prependedByte, std::nullopt), result) == SawError::Yes) {
        sawError = true;
        result.Append(replacementCharacter);
        if (stopOnError) {
            state.lead = 0x00;
            return result.ToString();
        }
    }
    if (state.iso2022JPSecondPrependedByte && byteParser(*std::exchange(state.iso2022JPSecondPrependedByte, std::nullopt), result) == SawError::Yes && stopOnError) {
        sawError = true;
        result.Append(replacementCharacter);
        if (stopOnError) {
            state.lead = 0x00;
            return result.ToString();
        }
    }
    for (std::size_t i = 0; i < length; i++) {
        if (byteParser(bytes[i], result) == SawError::Yes) {
            sawError = true;
            result.Append(replacementCharacter);
            if (stopOnError) {
                state.lead = 0x00;
                return result.ToString();
            }
        }
        if (state.prependedByte && byteParser(*std::exchange(state.prependedByte, std::nullopt), result) == SawError::Yes) {
            sawError = true;
            result.Append(replacementCharacter);
            if (stopOnError) {
                state.lead = 0x00;
                return result.ToString();
            }
        }
        if (state.iso2022JPSecondPrependedByte && byteParser(*std::exchange(state.iso2022JPSecondPrependedByte, std::nullopt), result) == SawError::Yes && stopOnError) {
            sawError = true;
            result.Append(replacementCharacter);
            if (stopOnError) {
                state.lead = 0x00;
                return result.ToString();
            }
        }
    }

    if (flush) {
        switch (state.iso2022JPDecoderState) {
        case ISO2022JPDecoderState::ASCII:
        case ISO2022JPDecoderState::Roman:
        case ISO2022JPDecoderState::Katakana:
        case ISO2022JPDecoderState::LeadByte:
            break;
        case ISO2022JPDecoderState::TrailByte:
            state.iso2022JPDecoderState = ISO2022JPDecoderState::LeadByte;
            [[fallthrough]];
        case ISO2022JPDecoderState::EscapeStart:
            sawError = true;
            result.Append(replacementCharacter);
            break;
        case ISO2022JPDecoderState::Escape:
            sawError = true;
            result.Append(replacementCharacter);
            if (state.lead) {
                result.Append(static_cast<char16_t>(std::exchange(state.lead, 0x00)));
            }
            break;
        }
    }

    return result.ToString();
}

String DecodeShiftJIS(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    Builder result;
    auto parser = [&](std::uint8_t byte, Builder& builder) -> SawError {
        if (std::uint8_t lead = std::exchange(state.lead, 0x00)) {
            std::uint8_t offset = byte < 0x7F ? 0x40 : 0x41;
            std::uint8_t leadOffset = lead < 0xA0 ? 0x81 : 0xC1;
            if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFC)) {
                std::uint16_t pointer = static_cast<std::uint16_t>((lead - leadOffset) * 188 + byte - offset);
                if (pointer >= 8836 && pointer <= 10715) {
                    builder.Append(static_cast<char16_t>(0xE000 - 8836 + pointer));
                    return SawError::No;
                }
                if (auto codePoint = PAL::findFirstInSortedPairs(jis0208(), pointer)) {
                    builder.Append(*codePoint);
                    return SawError::No;
                }
            }
            if (IsASCII(byte)) state.prependedByte = byte;
            return SawError::Yes;
        }
        if (IsASCII(byte) || byte == 0x80) {
            builder.Append(static_cast<char16_t>(byte));
            return SawError::No;
        }
        if (byte >= 0xA1 && byte <= 0xDF) {
            builder.Append(static_cast<char16_t>(0xFF61 - 0xA1 + byte));
            return SawError::No;
        }
        if ((byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC)) {
            state.lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    };
    DecodeCommonInto(state, result, bytes, length, flush, stopOnError, sawError, parser);
    return result.ToString();
}

String DecodeEUCKR(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    Builder result;
    auto parser = [&](std::uint8_t byte, Builder& builder) -> SawError {
        if (std::uint8_t lead = std::exchange(state.lead, 0x00)) {
            if (byte >= 0x41 && byte <= 0xFE) {
                if (auto codePoint = PAL::findFirstInSortedPairs(eucKR(), static_cast<std::uint16_t>((lead - 0x81) * 190 + byte - 0x41))) {
                    builder.Append(*codePoint);
                    return SawError::No;
                }
            }
            if (IsASCII(byte)) state.prependedByte = byte;
            return SawError::Yes;
        }
        if (IsASCII(byte)) {
            builder.Append(static_cast<char16_t>(byte));
            return SawError::No;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            state.lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    };
    DecodeCommonInto(state, result, bytes, length, flush, stopOnError, sawError, parser);
    return result.ToString();
}

String DecodeBig5(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    Builder result;
    auto parser = [&](std::uint8_t byte, Builder& builder) -> SawError {
        if (std::uint8_t lead = std::exchange(state.lead, 0x00)) {
            std::uint8_t offset = byte < 0x7F ? 0x40 : 0x62;
            if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0xA1 && byte <= 0xFE)) {
                std::uint16_t pointer = static_cast<std::uint16_t>((lead - 0x81) * 157 + (byte - offset));
                if (pointer == 1133) {
                    builder.AppendCharacter(0x00CA);
                    builder.AppendCharacter(0x0304);
                } else if (pointer == 1135) {
                    builder.AppendCharacter(0x00CA);
                    builder.AppendCharacter(0x030C);
                } else if (pointer == 1164) {
                    builder.AppendCharacter(0x00EA);
                    builder.AppendCharacter(0x0304);
                } else if (pointer == 1166) {
                    builder.AppendCharacter(0x00EA);
                    builder.AppendCharacter(0x030C);
                } else {
                    if (auto codePoint = PAL::findFirstInSortedPairs(big5(), pointer)) builder.AppendCharacter(*codePoint);
                    else return SawError::Yes;
                }
                return SawError::No;
            }
            if (IsASCII(byte)) state.prependedByte = byte;
            return SawError::Yes;
        }
        if (IsASCII(byte)) {
            builder.Append(static_cast<char16_t>(byte));
            return SawError::No;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            state.lead = byte;
            return SawError::No;
        }
        return SawError::Yes;
    };
    DecodeCommonInto(state, result, bytes, length, flush, stopOnError, sawError, parser);
    return result.ToString();
}

const std::array<std::pair<std::uint32_t, char32_t>, 207>& gb18030Ranges() {
    static const std::array<std::pair<std::uint32_t, char32_t>, 207> ranges { {
        { 0, 0x0080 }, { 36, 0x00A5 }, { 38, 0x00A9 }, { 45, 0x00B2 }, { 50, 0x00B8 }, { 81, 0x00D8 }, { 89, 0x00E2 }, { 95, 0x00EB },
        { 96, 0x00EE }, { 100, 0x00F4 }, { 103, 0x00F8 }, { 104, 0x00FB }, { 105, 0x00FD }, { 109, 0x0102 }, { 126, 0x0114 }, { 133, 0x011C },
        { 148, 0x012C }, { 172, 0x0145 }, { 175, 0x0149 }, { 179, 0x014E }, { 208, 0x016C }, { 306, 0x01CF }, { 307, 0x01D1 }, { 308, 0x01D3 },
        { 309, 0x01D5 }, { 310, 0x01D7 }, { 311, 0x01D9 }, { 312, 0x01DB }, { 313, 0x01DD }, { 341, 0x01FA }, { 428, 0x0252 }, { 443, 0x0262 },
        { 544, 0x02C8 }, { 545, 0x02CC }, { 558, 0x02DA }, { 741, 0x03A2 }, { 742, 0x03AA }, { 749, 0x03C2 }, { 750, 0x03CA }, { 805, 0x0402 },
        { 819, 0x0450 }, { 820, 0x0452 }, { 7922, 0x2011 }, { 7924, 0x2017 }, { 7925, 0x201A }, { 7927, 0x201E }, { 7934, 0x2027 }, { 7943, 0x2031 },
        { 7944, 0x2034 }, { 7945, 0x2036 }, { 7950, 0x203C }, { 8062, 0x20AD }, { 8148, 0x2104 }, { 8149, 0x2106 }, { 8152, 0x210A }, { 8164, 0x2117 },
        { 8174, 0x2122 }, { 8236, 0x216C }, { 8240, 0x217A }, { 8262, 0x2194 }, { 8264, 0x219A }, { 8374, 0x2209 }, { 8380, 0x2210 }, { 8381, 0x2212 },
        { 8384, 0x2216 }, { 8388, 0x221B }, { 8390, 0x2221 }, { 8392, 0x2224 }, { 8393, 0x2226 }, { 8394, 0x222C }, { 8396, 0x222F }, { 8401, 0x2238 },
        { 8406, 0x223E }, { 8416, 0x2249 }, { 8419, 0x224D }, { 8424, 0x2253 }, { 8437, 0x2262 }, { 8439, 0x2268 }, { 8445, 0x2270 }, { 8482, 0x2296 },
        { 8485, 0x229A }, { 8496, 0x22A6 }, { 8521, 0x22C0 }, { 8603, 0x2313 }, { 8936, 0x246A }, { 8946, 0x249C }, { 9046, 0x254C }, { 9050, 0x2574 },
        { 9063, 0x2590 }, { 9066, 0x2596 }, { 9076, 0x25A2 }, { 9092, 0x25B4 }, { 9100, 0x25BE }, { 9108, 0x25C8 }, { 9111, 0x25CC }, { 9113, 0x25D0 },
        { 9131, 0x25E6 }, { 9162, 0x2607 }, { 9164, 0x260A }, { 9218, 0x2641 }, { 9219, 0x2643 }, { 11329, 0x2E82 }, { 11331, 0x2E85 }, { 11334, 0x2E89 },
        { 11336, 0x2E8D }, { 11346, 0x2E98 }, { 11361, 0x2EA8 }, { 11363, 0x2EAB }, { 11366, 0x2EAF }, { 11370, 0x2EB4 }, { 11372, 0x2EB8 }, { 11375, 0x2EBC },
        { 11389, 0x2ECB }, { 11682, 0x2FFC }, { 11686, 0x3004 }, { 11687, 0x3018 }, { 11692, 0x301F }, { 11694, 0x302A }, { 11714, 0x303F }, { 11716, 0x3094 },
        { 11723, 0x309F }, { 11725, 0x30F7 }, { 11730, 0x30FF }, { 11736, 0x312A }, { 11982, 0x322A }, { 11989, 0x3232 }, { 12102, 0x32A4 }, { 12336, 0x3390 },
        { 12348, 0x339F }, { 12350, 0x33A2 }, { 12384, 0x33C5 }, { 12393, 0x33CF }, { 12395, 0x33D3 }, { 12397, 0x33D6 }, { 12510, 0x3448 }, { 12553, 0x3474 },
        { 12851, 0x359F }, { 12962, 0x360F }, { 12973, 0x361B }, { 13738, 0x3919 }, { 13823, 0x396F }, { 13919, 0x39D1 }, { 13933, 0x39E0 }, { 14080, 0x3A74 },
        { 14298, 0x3B4F }, { 14585, 0x3C6F }, { 14698, 0x3CE1 }, { 15583, 0x4057 }, { 15847, 0x4160 }, { 16318, 0x4338 }, { 16434, 0x43AD }, { 16438, 0x43B2 },
        { 16481, 0x43DE }, { 16729, 0x44D7 }, { 17102, 0x464D }, { 17122, 0x4662 }, { 17315, 0x4724 }, { 17320, 0x472A }, { 17402, 0x477D }, { 17418, 0x478E },
        { 17859, 0x4948 }, { 17909, 0x497B }, { 17911, 0x497E }, { 17915, 0x4984 }, { 17916, 0x4987 }, { 17936, 0x499C }, { 17939, 0x49A0 }, { 17961, 0x49B8 },
        { 18664, 0x4C78 }, { 18703, 0x4CA4 }, { 18814, 0x4D1A }, { 18962, 0x4DAF }, { 19043, 0x9FA6 }, { 33469, 0xE76C }, { 33470, 0xE7C8 }, { 33471, 0xE7E7 },
        { 33484, 0xE815 }, { 33485, 0xE819 }, { 33490, 0xE81F }, { 33497, 0xE827 }, { 33501, 0xE82D }, { 33505, 0xE833 }, { 33513, 0xE83C }, { 33520, 0xE844 },
        { 33536, 0xE856 }, { 33550, 0xE865 }, { 37845, 0xF92D }, { 37921, 0xF97A }, { 37948, 0xF996 }, { 38029, 0xF9E8 }, { 38038, 0xF9F2 }, { 38064, 0xFA10 },
        { 38065, 0xFA12 }, { 38066, 0xFA15 }, { 38069, 0xFA19 }, { 38075, 0xFA22 }, { 38076, 0xFA25 }, { 38078, 0xFA2A }, { 39108, 0xFE32 }, { 39109, 0xFE45 },
        { 39113, 0xFE53 }, { 39114, 0xFE58 }, { 39115, 0xFE67 }, { 39116, 0xFE6C }, { 39265, 0xFF5F }, { 39394, 0xFFE6 }, { 189000, 0x10000 }
    } };
    return ranges;
}

std::optional<char32_t> gb18030RangesCodePoint(std::uint32_t pointer) {
    if ((pointer > 39419 && pointer < 189000) || pointer > 1237575) return std::nullopt;
    if (pointer == 7457) return 0xE7C7;
    const auto& ranges = gb18030Ranges();
    auto upperBound = std::upper_bound(ranges.begin(), ranges.end(), PAL::makeFirstAdapter(pointer), PAL::CompareFirst { });
    std::uint32_t offset = (upperBound - 1)->first;
    char32_t codePointOffset = (upperBound - 1)->second;
    return codePointOffset + pointer - offset;
}

std::uint32_t gb18030RangesPointer(char32_t codePoint) {
    if (codePoint == 0xE7C7) return 7457;
    const auto& ranges = gb18030Ranges();
    auto upperBound = std::upper_bound(ranges.begin(), ranges.end(), PAL::makeSecondAdapter(codePoint), PAL::CompareSecond { });
    std::uint32_t pointerOffset = (upperBound - 1)->first;
    char32_t offset = (upperBound - 1)->second;
    return pointerOffset + codePoint - offset;
}

std::optional<char32_t> gb18030_2022_Encode(char32_t codePoint) {
    switch (codePoint) {
    case 0xE81E: return 0x82359037;
    case 0xE826: return 0x82359038;
    case 0xE82B: return 0x82359039;
    case 0xE82C: return 0x82359130;
    case 0xE832: return 0x82359131;
    case 0xE843: return 0x82359132;
    case 0xE854: return 0x82359133;
    case 0xE864: return 0x82359134;
    case 0xE78D: return 0x84318236;
    case 0xE78F: return 0x84318237;
    case 0xE78E: return 0x84318238;
    case 0xE790: return 0x84318239;
    case 0xE791: return 0x84318330;
    case 0xE792: return 0x84318331;
    case 0xE793: return 0x84318332;
    case 0xE794: return 0x84318333;
    case 0xE795: return 0x84318334;
    case 0xE796: return 0x84318335;
    }
    return std::nullopt;
}

std::optional<char32_t> gb18030_2022_Decode(std::uint8_t first, std::uint8_t second, std::uint8_t third, std::uint8_t fourth) {
    switch (static_cast<std::uint32_t>(first) << 24 | static_cast<std::uint32_t>(second) << 16 | static_cast<std::uint32_t>(third) << 8 | fourth) {
    case 0x82359037: return 0xE81E;
    case 0x82359038: return 0xE826;
    case 0x82359039: return 0xE82B;
    case 0x82359130: return 0xE82C;
    case 0x82359131: return 0xE832;
    case 0x82359132: return 0xE843;
    case 0x82359133: return 0xE854;
    case 0x82359134: return 0xE864;
    case 0x84318236: return 0xE78D;
    case 0x84318237: return 0xE78F;
    case 0x84318238: return 0xE78E;
    case 0x84318239: return 0xE790;
    case 0x84318330: return 0xE791;
    case 0x84318331: return 0xE792;
    case 0x84318332: return 0xE793;
    case 0x84318333: return 0xE794;
    case 0x84318334: return 0xE795;
    case 0x84318335: return 0xE796;
    }
    return std::nullopt;
}

String DecodeGB18030(const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    CJKState state;
    Builder result;
    std::function<SawError(std::uint8_t, Builder&)> parseByte = [&](std::uint8_t byte, Builder& builder) -> SawError {
        if (state.gb18030Third) {
            if (byte < 0x30 || byte > 0x39) {
                sawError = true;
                builder.Append(replacementCharacter);
                state.gb18030First = 0x00;
                std::uint8_t second = std::exchange(state.gb18030Second, 0x00);
                std::uint8_t third = std::exchange(state.gb18030Third, 0x00);
                if (parseByte(second, builder) == SawError::Yes) {
                    sawError = true;
                    builder.Append(replacementCharacter);
                }
                if (parseByte(third, builder) == SawError::Yes) {
                    sawError = true;
                    builder.Append(replacementCharacter);
                }
                return parseByte(byte, builder);
            }
            std::uint8_t first = std::exchange(state.gb18030First, 0x00);
            std::uint8_t second = std::exchange(state.gb18030Second, 0x00);
            std::uint8_t third = std::exchange(state.gb18030Third, 0x00);
            if (auto codePoint = gb18030_2022_Decode(first, second, third, byte)) {
                builder.AppendCharacter(*codePoint);
                return SawError::No;
            }
            if (auto codePoint = gb18030RangesCodePoint(((first - 0x81) * 10 * 126 * 10) + ((second - 0x30) * 10 * 126) + ((third - 0x81) * 10) + byte - 0x30)) {
                builder.AppendCharacter(*codePoint);
                return SawError::No;
            }
            return SawError::Yes;
        }
        if (state.gb18030Second) {
            if (byte >= 0x81 && byte <= 0xFE) {
                state.gb18030Third = byte;
                return SawError::No;
            }
            sawError = true;
            builder.Append(replacementCharacter);
            state.gb18030First = 0x00;
            if (parseByte(std::exchange(state.gb18030Second, 0x00), builder) == SawError::Yes) {
                sawError = true;
                builder.Append(replacementCharacter);
            }
            return parseByte(byte, builder);
        }
        if (state.gb18030First) {
            if (byte >= 0x30 && byte <= 0x39) {
                state.gb18030Second = byte;
                return SawError::No;
            }
            std::uint8_t lead = std::exchange(state.gb18030First, 0x00);
            std::uint8_t offset = byte < 0x7F ? 0x40 : 0x41;
            if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFE)) {
                std::size_t pointer = (lead - 0x81) * 190 + byte - offset;
                if (pointer < gb18030().size()) {
                    builder.Append(gb18030()[pointer]);
                    return SawError::No;
                }
            }
            if (IsASCII(byte)) state.prependedByte = byte;
            return SawError::Yes;
        }
        if (IsASCII(byte)) {
            builder.Append(static_cast<char16_t>(byte));
            return SawError::No;
        }
        if (byte == 0x80) {
            builder.AppendCharacter(0x20AC);
            return SawError::No;
        }
        if (byte >= 0x81 && byte <= 0xFE) {
            state.gb18030First = byte;
            return SawError::No;
        }
        return SawError::Yes;
    };
    DecodeCommonInto(state, result, bytes, length, flush, stopOnError, sawError, parseByte);
    if (flush && (state.gb18030First || state.gb18030Second || state.gb18030Third)) {
        state.gb18030First = 0x00;
        state.gb18030Second = 0x00;
        state.gb18030Third = 0x00;
        sawError = true;
        result.Append(replacementCharacter);
    }
    return result.ToString();
}

enum class IsGBK : bool { No, Yes };

Vector<std::uint8_t> GbEncodeShared(StringView string, UnencodableHandling handling, IsGBK isGBK) {
    Validate(string);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        if (codePoint == 0xE5E5) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        if (isGBK == IsGBK::Yes) {
            if (codePoint == 0x20AC) {
                result.Append(0x80);
                continue;
            }
        } else if (auto encoded = gb18030_2022_Encode(codePoint)) {
            result.Append(static_cast<std::uint8_t>(*encoded >> 24));
            result.Append(static_cast<std::uint8_t>(*encoded >> 16));
            result.Append(static_cast<std::uint8_t>(*encoded >> 8));
            result.Append(static_cast<std::uint8_t>(*encoded));
            continue;
        }
        const auto& table = GB18030EncodeIndexTable();
        auto pointerRange = PAL::findInSortedPairs(table, codePoint);
        if (pointerRange.first != pointerRange.second) {
            std::uint16_t pointer = pointerRange.first->second;
            std::uint8_t lead = static_cast<std::uint8_t>(pointer / 190 + 0x81);
            std::uint8_t trail = static_cast<std::uint8_t>(pointer % 190);
            std::uint8_t offset = trail < 0x3F ? 0x40 : 0x41;
            result.Append(lead);
            result.Append(static_cast<std::uint8_t>(trail + offset));
            continue;
        }
        if (isGBK == IsGBK::Yes) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        std::uint32_t pointer = gb18030RangesPointer(codePoint);
        std::uint8_t byte1 = static_cast<std::uint8_t>(pointer / (10 * 126 * 10));
        pointer = pointer % (10 * 126 * 10);
        std::uint8_t byte2 = static_cast<std::uint8_t>(pointer / (10 * 126));
        pointer = pointer % (10 * 126);
        std::uint8_t byte3 = static_cast<std::uint8_t>(pointer / 10);
        std::uint8_t byte4 = static_cast<std::uint8_t>(pointer % 10);
        result.Append(static_cast<std::uint8_t>(byte1 + 0x81));
        result.Append(static_cast<std::uint8_t>(byte2 + 0x30));
        result.Append(static_cast<std::uint8_t>(byte3 + 0x81));
        result.Append(static_cast<std::uint8_t>(byte4 + 0x30));
    }
    return result.Take();
}

Vector<std::uint8_t> EucJPEncode(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        if (codePoint == 0x00A5) {
            result.Append(0x5C);
            continue;
        }
        if (codePoint == 0x203E) {
            result.Append(0x7E);
            continue;
        }
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            result.Append(0x8E);
            result.Append(static_cast<std::uint8_t>(codePoint - 0xFF61 + 0xA1));
            continue;
        }
        if (codePoint == 0x2212) codePoint = 0xFF0D;
        const auto& table = JIS0208EncodeIndexTable();
        auto pointer = PAL::findFirstInSortedPairs(table, codePoint);
        if (!pointer) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        result.Append(static_cast<std::uint8_t>(*pointer / 94 + 0xA1));
        result.Append(static_cast<std::uint8_t>(*pointer % 94 + 0xA1));
    }
    return result.Take();
}

Vector<std::uint8_t> Iso2022JPEncode(StringView string, UnencodableHandling handling) {
    Validate(string);
    enum class State : std::uint8_t { ASCII, Roman, Jis0208 };
    State state { State::ASCII };

    BytesVector result;
    result.ReserveInitialCapacity(string.length);

    auto changeStateToASCII = [&] {
        state = State::ASCII;
        result.Append(0x1B);
        result.Append(0x28);
        result.Append(0x42);
    };

    auto statefulUnencodableHandler = [&](char32_t codePoint) {
        if (state == State::Jis0208) changeStateToASCII();
        Unencodable(codePoint, handling, result);
    };

    std::function<void(char32_t)> parseCodePoint;
    parseCodePoint = [&](char32_t codePoint) {
        if ((state == State::ASCII || state == State::Roman) && (codePoint == 0x000E || codePoint == 0x000F || codePoint == 0x001B)) {
            statefulUnencodableHandler(replacementCharacter);
            return;
        }
        if (state == State::ASCII && IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            return;
        }
        if (state == State::Roman) {
            if (IsASCII(codePoint) && codePoint != 0x005C && codePoint != 0x007E) {
                result.Append(static_cast<std::uint8_t>(codePoint));
                return;
            }
            if (codePoint == 0x00A5) {
                result.Append(0x5C);
                return;
            }
            if (codePoint == 0x203E) {
                result.Append(0x7E);
                return;
            }
        }
        if (IsASCII(codePoint) && state != State::ASCII) {
            changeStateToASCII();
            parseCodePoint(codePoint);
            return;
        }
        if ((codePoint == 0x00A5 || codePoint == 0x203E) && state != State::Roman) {
            state = State::Roman;
            result.Append(0x1B);
            result.Append(0x28);
            result.Append(0x4A);
            parseCodePoint(codePoint);
            return;
        }
        if (codePoint == 0x2212) codePoint = 0xFF0D;
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            static constexpr std::array<char32_t, 63> iso2022JPKatakana {
                0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5, 0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC,
                0x30A2, 0x30A4, 0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5, 0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF,
                0x30C1, 0x30C4, 0x30C6, 0x30C8, 0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8, 0x30DB, 0x30DE, 0x30DF,
                0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8, 0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C
            };
            codePoint = iso2022JPKatakana[codePoint - 0xFF61];
        }

        const auto& table = JIS0208EncodeIndexTable();
        auto pointer = PAL::findFirstInSortedPairs(table, codePoint);
        if (!pointer) {
            statefulUnencodableHandler(codePoint);
            return;
        }
        if (state != State::Jis0208) {
            state = State::Jis0208;
            result.Append(0x1B);
            result.Append(0x24);
            result.Append(0x42);
            parseCodePoint(codePoint);
            return;
        }
        result.Append(static_cast<std::uint8_t>(*pointer / 94 + 0x21));
        result.Append(static_cast<std::uint8_t>(*pointer % 94 + 0x21));
    };

    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) parseCodePoint(points.Current());

    if (state != State::ASCII) changeStateToASCII();

    return result.Take();
}

Vector<std::uint8_t> ShiftJISEncode(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint) || codePoint == 0x0080) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        if (codePoint == 0x00A5) {
            result.Append(0x5C);
            continue;
        }
        if (codePoint == 0x203E) {
            result.Append(0x7E);
            continue;
        }
        if (codePoint >= 0xFF61 && codePoint <= 0xFF9F) {
            result.Append(static_cast<std::uint8_t>(codePoint - 0xFF61 + 0xA1));
            continue;
        }
        if (codePoint == 0x2212) codePoint = 0xFF0D;

        const auto& table = JIS0208EncodeIndexTable();
        auto range = PAL::findInSortedPairs(table, codePoint);
        if (range.first == range.second) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        for (auto pair = range.first; pair < range.second; pair++) {
            std::uint16_t pointer = pair->second;
            if (pointer >= 8272 && pointer <= 8835) continue;
            std::uint8_t lead = static_cast<std::uint8_t>(pointer / 188);
            std::uint8_t leadOffset = lead < 0x1F ? 0x81 : 0xC1;
            std::uint8_t trail = static_cast<std::uint8_t>(pointer % 188);
            std::uint8_t offset = trail < 0x3F ? 0x40 : 0x41;
            result.Append(static_cast<std::uint8_t>(lead + leadOffset));
            result.Append(static_cast<std::uint8_t>(trail + offset));
            break;
        }
    }
    return result.Take();
}

Vector<std::uint8_t> EucKREncode(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        const auto& table = EUCKREncodingIndexTable();
        auto pointer = PAL::findFirstInSortedPairs(table, codePoint);
        if (!pointer) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        result.Append(static_cast<std::uint8_t>(*pointer / 190 + 0x81));
        result.Append(static_cast<std::uint8_t>(*pointer % 190 + 0x41));
    }
    return result.Take();
}

Vector<std::uint8_t> Big5Encode(StringView string, UnencodableHandling handling) {
    Validate(string);
    BytesVector result;
    result.ReserveInitialCapacity(string.length);
    for (CodePointRange points(string); !points.AtEnd(); points.Advance()) {
        auto codePoint = points.Current();
        if (IsASCII(codePoint)) {
            result.Append(static_cast<std::uint8_t>(codePoint));
            continue;
        }
        const auto& table = Big5EncodeIndexTable();
        auto pointerRange = PAL::findInSortedPairs(table, codePoint);
        if (pointerRange.first == pointerRange.second) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        std::uint16_t pointer = 0;
        if (codePoint == 0x2550 || codePoint == 0x255E || codePoint == 0x2561 || codePoint == 0x256A || codePoint == 0x5341 || codePoint == 0x5345)
            pointer = (pointerRange.second - 1)->second;
        else
            pointer = pointerRange.first->second;
        if (pointer < 157 * (0xA1 - 0x81)) {
            Unencodable(codePoint, handling, result);
            continue;
        }
        std::uint8_t lead = static_cast<std::uint8_t>(pointer / 157 + 0x81);
        std::uint8_t trail = static_cast<std::uint8_t>(pointer % 157);
        std::uint8_t offset = trail < 0x3F ? 0x40 : 0x62;
        result.Append(lead);
        result.Append(static_cast<std::uint8_t>(trail + offset));
    }
    return result.Take();
}

String DecodeCJK(CJKEncoding encoding, const std::uint8_t* bytes, std::size_t length, bool flush, bool stopOnError, bool& sawError) {
    switch (encoding) {
    case CJKEncoding::EUC_JP:
        return DecodeEUCJP(bytes, length, flush, stopOnError, sawError);
    case CJKEncoding::Shift_JIS:
        return DecodeShiftJIS(bytes, length, flush, stopOnError, sawError);
    case CJKEncoding::ISO2022JP:
        return DecodeISO2022JP(bytes, length, flush, stopOnError, sawError);
    case CJKEncoding::EUC_KR:
        return DecodeEUCKR(bytes, length, flush, stopOnError, sawError);
    case CJKEncoding::Big5:
        return DecodeBig5(bytes, length, flush, stopOnError, sawError);
    case CJKEncoding::GBK:
    case CJKEncoding::GB18030:
        return DecodeGB18030(bytes, length, flush, stopOnError, sawError);
    }
    throw std::logic_error("unknown CJK text encoding");
}

Vector<std::uint8_t> EncodeCJK(CJKEncoding encoding, StringView string, UnencodableHandling handling) {
    switch (encoding) {
    case CJKEncoding::EUC_JP:
        return EucJPEncode(string, handling);
    case CJKEncoding::Shift_JIS:
        return ShiftJISEncode(string, handling);
    case CJKEncoding::ISO2022JP:
        return Iso2022JPEncode(string, handling);
    case CJKEncoding::EUC_KR:
        return EucKREncode(string, handling);
    case CJKEncoding::Big5:
        return Big5Encode(string, handling);
    case CJKEncoding::GBK:
        return GbEncodeShared(string, handling, IsGBK::Yes);
    case CJKEncoding::GB18030:
        return GbEncodeShared(string, handling, IsGBK::No);
    }
    throw std::logic_error("unknown CJK text encoding");
}

enum class CodecKind : std::uint8_t { Latin1, UTF8, UTF16LE, UTF16BE, UserDefined, Replacement, SingleByte, CJK, ICU };

struct CodecSpec {
    CodecKind kind;
    std::uint8_t encoding;
};

struct TextEncodingNameHash {
    std::size_t operator()(const char* string) const {
        unsigned hash = 0x9E3779B9U;
        for (;;) {
            char c = *string++;
            if (!c) {
                hash += (hash << 3);
                hash ^= (hash >> 11);
                hash += (hash << 15);
                return hash;
            }
            hash += static_cast<unsigned>(ToASCIILower(c));
            hash += (hash << 10);
            hash ^= (hash >> 6);
        }
    }
};

struct TextEncodingNameEqual {
    bool operator()(const char* a, const char* b) const {
        for (;;) {
            char c1 = *a++;
            char c2 = *b++;
            if (ToASCIILower(c1) != ToASCIILower(c2)) return false;
            if (!c1) return true;
        }
    }
};

using TextEncodingNameMap = std::unordered_map<const char*, const char*, TextEncodingNameHash, TextEncodingNameEqual>;
using TextCodecMap = std::unordered_map<const char*, CodecSpec, TextEncodingNameHash, TextEncodingNameEqual>;
using TextEncodingNameSet = std::unordered_set<const char*, TextEncodingNameHash, TextEncodingNameEqual>;

struct RegistryState {
    std::mutex mutex;
    TextEncodingNameMap names;
    TextCodecMap codecs;
    TextEncodingNameSet japaneseEncodings;
    TextEncodingNameSet nonBackslashEncodings;
    bool didBuildBase { false };
    bool didExtend { false };
};

RegistryState& Registry() {
    static RegistryState state;
    return state;
}

bool IsUndesiredAlias(const char* alias) {
    for (const char* p = alias; *p; ++p) {
        if (*p == ',') return true;
    }
    return !std::strcmp(alias, "8859_1");
}

void RegisterName(TextEncodingNameMap& map, const char* alias, const char* name) {
    if (IsUndesiredAlias(alias)) return;
    auto atom = map.find(name);
    const char* atomName = atom != map.end() ? atom->second : name;
    map.emplace(alias, atomName);
}

void RegisterCodec(RegistryState& state, const char* name, CodecSpec spec) {
    auto atom = state.names.find(name);
    if (atom == state.names.end()) throw std::logic_error("text codec registered before its name");
    state.codecs.emplace(atom->second, spec);
}

template<typename Collection>
void RegisterNames(RegistryState& state, const Collection& aliases, const char* name) {
    for (const char* alias : aliases) RegisterName(state.names, alias, name);
}

void BuildBaseTextCodecMaps(RegistryState& state) {
    RegisterNames(state, std::array<const char*, 16> { "windows-1252", "ansi_x3.4-1968", "ascii", "cp1252", "cp819", "csisolatin1", "ibm819", "iso-8859-1", "iso-ir-100", "iso8859-1", "iso88591", "iso_8859-1", "iso_8859-1:1987", "l1", "latin1", "us-ascii" }, "windows-1252");
    RegisterName(state.names, "x-cp1252", "windows-1252");
    RegisterCodec(state, "windows-1252", { CodecKind::Latin1, 0 });

    RegisterNames(state, std::array<const char*, 5> { "UTF-8", "utf8", "unicode-1-1-utf-8", "unicode11utf8", "unicode20utf8" }, "UTF-8");
    RegisterName(state.names, "x-unicode20utf8", "UTF-8");
    RegisterCodec(state, "UTF-8", { CodecKind::UTF8, 0 });

    RegisterName(state.names, "UTF-16LE", "UTF-16LE");
    RegisterName(state.names, "UTF-16BE", "UTF-16BE");
    RegisterNames(state, std::array<const char*, 6> { "ISO-10646-UCS-2", "UCS-2", "UTF-16", "Unicode", "csUnicode", "unicodeFEFF" }, "UTF-16LE");
    RegisterName(state.names, "unicodeFFFE", "UTF-16BE");
    RegisterCodec(state, "UTF-16LE", { CodecKind::UTF16LE, 0 });
    RegisterCodec(state, "UTF-16BE", { CodecKind::UTF16BE, 0 });

    RegisterName(state.names, "x-user-defined", "x-user-defined");
    RegisterCodec(state, "x-user-defined", { CodecKind::UserDefined, 0 });

    state.didBuildBase = true;
}

void AddEncodingName(RegistryState& state, TextEncodingNameSet& set, const char* name) {
    auto atom = state.names.find(name);
    if (atom != state.names.end()) set.insert(atom->second);
}

void BuildQuirksSets(RegistryState& state) {
    AddEncodingName(state, state.japaneseEncodings, "EUC-JP");
    AddEncodingName(state, state.japaneseEncodings, "ISO-2022-JP");
    AddEncodingName(state, state.japaneseEncodings, "ISO-2022-JP-1");
    AddEncodingName(state, state.japaneseEncodings, "ISO-2022-JP-2");
    AddEncodingName(state, state.japaneseEncodings, "ISO-2022-JP-3");
    AddEncodingName(state, state.japaneseEncodings, "JIS_C6226-1978");
    AddEncodingName(state, state.japaneseEncodings, "JIS_X0201");
    AddEncodingName(state, state.japaneseEncodings, "JIS_X0208-1983");
    AddEncodingName(state, state.japaneseEncodings, "JIS_X0208-1990");
    AddEncodingName(state, state.japaneseEncodings, "JIS_X0212-1990");
    AddEncodingName(state, state.japaneseEncodings, "Shift_JIS");
    AddEncodingName(state, state.japaneseEncodings, "Shift_JIS_X0213-2000");
    AddEncodingName(state, state.japaneseEncodings, "cp932");
    AddEncodingName(state, state.japaneseEncodings, "x-mac-japanese");

    AddEncodingName(state, state.nonBackslashEncodings, "x-mac-japanese");
    AddEncodingName(state, state.nonBackslashEncodings, "ISO-2022-JP");
    AddEncodingName(state, state.nonBackslashEncodings, "EUC-JP");
    AddEncodingName(state, state.nonBackslashEncodings, "Shift_JIS");
    AddEncodingName(state, state.nonBackslashEncodings, "Shift_JIS_X0213-2000");
}

void PruneBlocklistedCodecs(RegistryState& state) {
    static const char* const textEncodingNameBlocklist[] = { "UTF-7", "BOCU-1", "SCSU" };
    for (const char* nameFromBlocklist : textEncodingNameBlocklist) {
        auto atom = state.names.find(nameFromBlocklist);
        if (atom == state.names.end()) continue;
        const char* atomName = atom->second;
        std::vector<const char*> names;
        for (auto& entry : state.names) {
            if (entry.second == atomName) names.push_back(entry.first);
        }
        for (const char* name : names) state.names.erase(name);
        state.codecs.erase(atomName);
    }
}

void ExtendTextCodecMaps(RegistryState& state) {
    RegisterNames(state, std::array<const char*, 6> { "replacement", "csiso2022kr", "hz-gb-2312", "iso-2022-cn", "iso-2022-cn-ext", "iso-2022-kr" }, "replacement");
    RegisterCodec(state, "replacement", { CodecKind::Replacement, 0 });

    using EncodingNameEntry = std::pair<const char*, std::vector<const char*>>;
    static const std::vector<EncodingNameEntry> icuEncodingNames {
        { "ISO-8859-2", { "csisolatin2", "iso-ir-101", "iso8859-2", "iso88592", "iso_8859-2", "iso_8859-2:1987", "l2", "latin2" } },
        { "ISO-8859-4", { "csisolatin4", "iso-ir-110", "iso8859-4", "iso88594", "iso_8859-4", "iso_8859-4:1988", "l4", "latin4" } },
        { "ISO-8859-5", { "csisolatincyrillic", "cyrillic", "iso-ir-144", "iso8859-5", "iso88595", "iso_8859-5", "iso_8859-5:1988" } },
        { "ISO-8859-10", { "csisolatin6", "iso-ir-157", "iso8859-10", "iso885910", "l6", "latin6", "iso8859101992", "isoir157" } },
        { "ISO-8859-13", { "iso8859-13", "iso885913" } },
        { "ISO-8859-14", { "iso8859-14", "iso885914", "isoceltic", "iso8859141998", "isoir199", "latin8", "l8" } },
        { "ISO-8859-15", { "csisolatin9", "iso8859-15", "iso885915", "iso_8859-15", "l9" } },
        { "ISO-8859-16", { "isoir226", "iso8859162001", "l10", "latin10" } },
        { "KOI8-R", { "cskoi8r", "koi", "koi8", "koi8_r" } },
        { "macintosh", { "csmacintosh", "mac", "x-mac-roman", "macroman", "x-macroman" } },
        { "windows-1250", { "cp1250", "x-cp1250", "winlatin2" } },
        { "windows-1251", { "cp1251", "wincyrillic", "x-cp1251" } },
        { "windows-1254", { "winturkish", "cp1254", "csisolatin5", "iso-8859-9", "iso-ir-148", "iso8859-9", "iso88599", "iso_8859-9", "iso_8859-9:1989", "l5", "latin5", "x-cp1254" } },
        { "windows-1256", { "winarabic", "cp1256", "x-cp1256" } },
        { "windows-1258", { "winvietnamese", "cp1258", "x-cp1258" } },
        { "x-mac-cyrillic", { "maccyrillic", "x-mac-ukrainian", "windows-10007", "mac-cyrillic", "maccy", "x-MacCyrillic", "x-MacUkraine" } },
        { "x-mac-greek", { "windows-10006", "macgr", "x-MacGreek" } },
        { "x-mac-centraleurroman", { "windows-10029", "x-mac-ce", "macce", "maccentraleurope", "x-MacCentralEurope" } },
        { "x-mac-turkish", { "windows-10081", "mactr", "x-MacTurkish" } },
        { "EUC-TW", {} }
    };
    for (const auto& encodingName : icuEncodingNames) {
        RegisterName(state.names, encodingName.first, encodingName.first);
        for (const char* alias : encodingName.second) RegisterName(state.names, alias, encodingName.first);
        RegisterCodec(state, encodingName.first, { CodecKind::ICU, 0 });
    }

    RegisterNames(state, std::array<const char*, 5> { "Big5", "big5-hkscs", "cn-big5", "csbig5", "x-x-big5" }, "Big5");
    RegisterCodec(state, "Big5", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::Big5) });

    RegisterNames(state, std::array<const char*, 3> { "EUC-JP", "cseucpkdfmtjapanese", "x-euc-jp" }, "EUC-JP");
    RegisterCodec(state, "EUC-JP", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::EUC_JP) });

    RegisterNames(state, std::array<const char*, 8> { "Shift_JIS", "csshiftjis", "ms932", "ms_kanji", "shift-jis", "sjis", "windows-31j", "x-sjis" }, "Shift_JIS");
    RegisterCodec(state, "Shift_JIS", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::Shift_JIS) });

    RegisterNames(state, std::array<const char*, 12> { "EUC-KR", "cseuckr", "csksc56011987", "iso-ir-149", "korean", "ks_c_5601-1987", "ks_c_5601-1989", "ksc5601", "ksc_5601", "windows-949", "x-windows-949", "x-uhc" }, "EUC-KR");
    RegisterCodec(state, "EUC-KR", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::EUC_KR) });

    RegisterNames(state, std::array<const char*, 2> { "ISO-2022-JP", "csiso2022jp" }, "ISO-2022-JP");
    RegisterCodec(state, "ISO-2022-JP", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::ISO2022JP) });

    RegisterNames(state, std::array<const char*, 17> { "GBK", "chinese", "csgb2312", "csiso58gb231280", "gb2312", "gb_2312", "gb_2312-80", "iso-ir-58", "x-gbk", "cn-gb", "csgb231280", "x-euc-cn", "euc-cn", "cp936", "ms936", "gb2312-1980", "windows-936" }, "GBK");
    RegisterName(state.names, "windows-936-2000", "GBK");
    RegisterCodec(state, "GBK", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::GBK) });

    RegisterNames(state, std::array<const char*, 3> { "gb18030", "ibm-1392", "windows-54936" }, "gb18030");
    RegisterCodec(state, "gb18030", { CodecKind::CJK, static_cast<std::uint8_t>(CJKEncoding::GB18030) });

    RegisterNames(state, std::array<const char*, 9> { "ISO-8859-3", "csisolatin3", "iso-ir-109", "iso8859-3", "iso88593", "iso_8859-3", "iso_8859-3:1988", "l3", "latin3" }, "ISO-8859-3");
    RegisterCodec(state, "ISO-8859-3", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::ISO_8859_3) });

    RegisterNames(state, std::array<const char*, 14> { "ISO-8859-6", "arabic", "asmo-708", "csiso88596e", "csiso88596i", "csisolatinarabic", "ecma-114", "iso-8859-6-e", "iso-8859-6-i", "iso-ir-127", "iso8859-6", "iso88596", "iso_8859-6", "iso_8859-6:1987" }, "ISO-8859-6");
    RegisterCodec(state, "ISO-8859-6", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::ISO_8859_6) });

    RegisterNames(state, std::array<const char*, 12> { "ISO-8859-7", "csisolatingreek", "ecma-118", "elot_928", "greek", "greek8", "iso-ir-126", "iso8859-7", "iso88597", "iso_8859-7", "iso_8859-7:1987", "sun_eu_greek" }, "ISO-8859-7");
    RegisterCodec(state, "ISO-8859-7", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::ISO_8859_7) });

    RegisterNames(state, std::array<const char*, 10> { "ISO-8859-8", "csiso88598e", "csisolatinhebrew", "hebrew", "iso-8859-8-e", "iso-ir-138", "iso8859-8", "iso88598", "iso_8859-8", "iso_8859-8:1988" }, "ISO-8859-8");
    RegisterName(state.names, "visual", "ISO-8859-8");
    RegisterCodec(state, "ISO-8859-8", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::ISO_8859_8) });

    RegisterNames(state, std::array<const char*, 3> { "ISO-8859-8-I", "csiso88598i", "logical" }, "ISO-8859-8-I");
    RegisterCodec(state, "ISO-8859-8-I", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::ISO_8859_8) });

    RegisterNames(state, std::array<const char*, 6> { "windows-874", "dos-874", "iso-8859-11", "iso8859-11", "iso885911", "tis-620" }, "windows-874");
    RegisterCodec(state, "windows-874", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::Windows_874) });

    RegisterNames(state, std::array<const char*, 3> { "windows-1253", "cp1253", "x-cp1253" }, "windows-1253");
    RegisterCodec(state, "windows-1253", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::Windows_1253) });

    RegisterNames(state, std::array<const char*, 3> { "windows-1255", "cp1255", "x-cp1255" }, "windows-1255");
    RegisterCodec(state, "windows-1255", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::Windows_1255) });

    RegisterNames(state, std::array<const char*, 3> { "windows-1257", "cp1257", "x-cp1257" }, "windows-1257");
    RegisterCodec(state, "windows-1257", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::Windows_1257) });

    RegisterNames(state, std::array<const char*, 2> { "KOI8-U", "koi8-ru" }, "KOI8-U");
    RegisterCodec(state, "KOI8-U", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::KOI8U) });

    RegisterNames(state, std::array<const char*, 4> { "IBM866", "866", "cp866", "csibm866" }, "IBM866");
    RegisterCodec(state, "IBM866", { CodecKind::SingleByte, static_cast<std::uint8_t>(SingleByteEncoding::IBM866) });

    PruneBlocklistedCodecs(state);
    BuildQuirksSets(state);
}

const char* AtomCanonicalName(const char* name) {
    if (!name || !name[0]) return nullptr;
    auto& state = Registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.didBuildBase) BuildBaseTextCodecMaps(state);
    if (auto found = state.names.find(name); found != state.names.end()) return found->second;
    if (state.didExtend) return nullptr;
    ExtendTextCodecMaps(state);
    state.didExtend = true;
    auto found = state.names.find(name);
    return found != state.names.end() ? found->second : nullptr;
}

const char* AtomCanonicalName(StringView alias) {
    if (!alias.length) return nullptr;
    Validate(alias);
    char buffer[maxEncodingNameLength + 1];
    std::size_t j = 0;
    for (std::size_t i = 0; i < alias.length; ++i) {
        auto unit = ViewUnit(alias, static_cast<unsigned>(i));
        if (unit > 0x7F) return nullptr;
        if (j == maxEncodingNameLength) return nullptr;
        buffer[j++] = static_cast<char>(unit);
    }
    buffer[j] = 0;
    return AtomCanonicalName(buffer);
}

CodecSpec NewTextCodecFor(const char* name) {
    auto& state = Registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.didBuildBase) BuildBaseTextCodecMaps(state);
    auto found = state.codecs.find(name);
    if (found == state.codecs.end()) throw std::logic_error("no text codec for encoding");
    return found->second;
}

bool NoExtendedTextEncodingNameUsed() {
    return !Registry().didExtend;
}

bool ShouldShowBackslashAsCurrencySymbolIn(const char* name) {
    if (!name) return false;
    auto& state = Registry();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.didExtend && state.nonBackslashEncodings.count(name) != 0;
}

char16_t BackslashAsCurrencySymbol(const char* name) {
    return ShouldShowBackslashAsCurrencySymbolIn(name) ? 0x00A5 : '\\';
}

struct TextEncodingVtable {
    Vector<std::uint8_t> (APS5_VABI* encodeForURLParsing)(const void*, StringView);
    void (APS5_VABI* completeDestructor)(void*);
    void (APS5_VABI* deletingDestructor)(void*);
};

struct TextEncodingVtableStorage {
    std::ptrdiff_t offsetToTop;
    const void* typeInfo;
    TextEncodingVtable dispatch;
};

void APS5_VABI TextEncodingCompleteDestructor(void*) {}

void APS5_VABI TextEncodingDeletingDestructor(void* self) {
    _ZdlPvm_nid_postfix(self, sizeof(WebCore::TextEncoding));
}

Vector<std::uint8_t> EncodeTextEncoding(const TextEncoding& encoding, StringView string, UnencodableHandling handling, bool normalize, const char* caller);

Vector<std::uint8_t> APS5_VABI TextEncodingEncodeForURLParsing(const void* self, StringView string) {
    if (!self) throw std::invalid_argument("null TextEncoding");
    return EncodeTextEncoding(*static_cast<const TextEncoding*>(self), string, UnencodableHandling::URLEncodedEntities, false, "TextEncodingEncodeForURLParsing");
}

const TextEncodingVtableStorage TextEncodingVtableInstance = {
    0,
    nullptr,
    {
        &TextEncodingEncodeForURLParsing,
        &TextEncodingCompleteDestructor,
        &TextEncodingDeletingDestructor,
    },
};

void ConstructTextEncoding(TextEncoding& self, const char* name) {
    self.vptr = &TextEncodingVtableInstance.dispatch;
    self.name = AtomCanonicalName(name);
    self.backslashAsCurrencySymbol = BackslashAsCurrencySymbol(self.name);
}

void ConstructTextEncoding(TextEncoding& self, const String* name) {
    if (!name) throw std::invalid_argument("null WTF::String reference");
    ConstructTextEncoding(self, AtomCanonicalName(NKWebKit::WtfAbi::ViewOf(name->impl)));
}

String DecodeWithCodec(const CodecSpec& codec, const char* bytes, std::size_t length, bool stopOnError, bool& sawError) {
    switch (codec.kind) {
    case CodecKind::Latin1:
        return DecodeLatin1(bytes, length);
    case CodecKind::UTF8:
        return DecodeUTF8(reinterpret_cast<const std::uint8_t*>(bytes), length, stopOnError, sawError);
    case CodecKind::UTF16LE:
        return DecodeUTF16(reinterpret_cast<const std::uint8_t*>(bytes), length, true, true, sawError);
    case CodecKind::UTF16BE:
        return DecodeUTF16(reinterpret_cast<const std::uint8_t*>(bytes), length, true, false, sawError);
    case CodecKind::UserDefined:
        return DecodeUserDefined(bytes, length);
    case CodecKind::Replacement:
        return DecodeReplacement(sawError);
    case CodecKind::SingleByte:
        return DecodeSingleByte(static_cast<SingleByteEncoding>(codec.encoding), reinterpret_cast<const std::uint8_t*>(bytes), length, stopOnError, sawError);
    case CodecKind::CJK:
        return DecodeCJK(static_cast<CJKEncoding>(codec.encoding), reinterpret_cast<const std::uint8_t*>(bytes), length, true, stopOnError, sawError);
    case CodecKind::ICU:
        break;
    }
    throw std::logic_error("unreachable text codec kind");
}

Vector<std::uint8_t> EncodeWithCodec(const CodecSpec& codec, StringView string, UnencodableHandling handling) {
    switch (codec.kind) {
    case CodecKind::Latin1:
        return EncodeLatin1(string, handling);
    case CodecKind::UTF8:
    case CodecKind::Replacement:
        return EncodeUTF8(string);
    case CodecKind::UTF16LE:
        return EncodeUTF16(string, true);
    case CodecKind::UTF16BE:
        return EncodeUTF16(string, false);
    case CodecKind::UserDefined:
        return EncodeUserDefined(string, handling);
    case CodecKind::SingleByte:
        return EncodeSingleByte(static_cast<SingleByteEncoding>(codec.encoding), string, handling);
    case CodecKind::CJK:
        return EncodeCJK(static_cast<CJKEncoding>(codec.encoding), string, handling);
    case CodecKind::ICU:
        break;
    }
    throw std::logic_error("unreachable text codec kind");
}

Vector<std::uint8_t> EncodeTextEncoding(const TextEncoding& encoding, StringView string, UnencodableHandling handling, bool normalize, const char* caller) {
    if (!encoding.name || !string.length) return {};
    auto codec = NewTextCodecFor(encoding.name);
    if (codec.kind == CodecKind::ICU) NotImplemented_nid_no_patch(caller);
    if (normalize) {
        auto normalized = _ZN3WTF13normalizedNFCENS_10StringViewE(string);
        return EncodeWithCodec(codec, normalized.view, handling);
    }
    return EncodeWithCodec(codec, string, handling);
}

const TextEncoding& CheckedEncoding(const TextEncoding* self) {
    if (!self) throw std::invalid_argument("null TextEncoding");
    return *self;
}

TextEncoding& CheckedMutableEncoding(TextEncoding* self) {
    if (!self) throw std::invalid_argument("null TextEncoding");
    return *self;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore12TextEncodingC1EPKc(TextEncoding* self, const char* name) {
    ConstructTextEncoding(CheckedMutableEncoding(self), name);
}

void APS5_VABI _ZN7WebCore12TextEncodingC2EPKc(TextEncoding* self, const char* name) {
    ConstructTextEncoding(CheckedMutableEncoding(self), name);
}

void APS5_VABI _ZN7WebCore12TextEncodingC1ERKN3WTF6StringE(TextEncoding* self, const String* name) {
    ConstructTextEncoding(CheckedMutableEncoding(self), name);
}

void APS5_VABI _ZN7WebCore12TextEncodingC2ERKN3WTF6StringE(TextEncoding* self, const String* name) {
    ConstructTextEncoding(CheckedMutableEncoding(self), name);
}

String APS5_VABI _ZNK7WebCore12TextEncoding6decodeEPKcmbRb(const TextEncoding* self, const char* data, std::size_t length, bool stopOnError, bool* sawError) {
    const TextEncoding& encoding = CheckedEncoding(self);
    if (!encoding.name) return String();
    if (!sawError) throw std::invalid_argument("null sawError reference");
    if (!data && length) throw std::invalid_argument("null text buffer");
    auto codec = NewTextCodecFor(encoding.name);
    if (codec.kind == CodecKind::ICU) NotImplemented_nid_no_patch(__func__);
    return DecodeWithCodec(codec, data, length, stopOnError, *sawError);
}

Vector<std::uint8_t> APS5_VABI _ZNK7WebCore12TextEncoding6encodeEN3WTF10StringViewENS_19UnencodableHandlingE(const TextEncoding* self, StringView string, UnencodableHandling handling) {
    return EncodeTextEncoding(CheckedEncoding(self), string, handling, true, __func__);
}

const char* APS5_VABI _ZNK7WebCore12TextEncoding7domNameEv(const TextEncoding* self) {
    const TextEncoding& encoding = CheckedEncoding(self);
    if (!encoding.name) return nullptr;
    if (NoExtendedTextEncodingNameUsed()) return encoding.name;
    static const char* const windows949 = AtomCanonicalName("windows-949");
    if (encoding.name == windows949) return "EUC-KR";
    return encoding.name;
}

}
