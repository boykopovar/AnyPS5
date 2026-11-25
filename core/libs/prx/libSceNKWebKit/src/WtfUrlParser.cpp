// Derived from Source/WTF/wtf/URLParser.cpp@e12a212a531aeddc11259e8515671cf5188e038d
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

// Derived from Source/WTF/wtf/URL.cpp@e12a212a531aeddc11259e8515671cf5188e038d
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

// Derived from Source/WTF/wtf/text/CodePointIterator.h@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace NKWebKit::UrlParserPort {

using LChar = unsigned char;
using UChar = char16_t;
using UChar32 = std::int32_t;
constexpr std::size_t notFound = static_cast<std::size_t>(-1);

#define ALWAYS_INLINE inline
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define UNUSED_PARAM(variable) ((void)(variable))
#define RELEASE_ASSERT(...) do { if (!(__VA_ARGS__)) throw std::logic_error("URLParser: internal check failed"); } while (0)
#define RELEASE_ASSERT_NOT_REACHED() throw std::logic_error("URLParser: unreachable state")
#define URL_PARSER_LOG(...) ((void)0)
#define WTFMove(value) std::move(value)
#define FALLTHROUGH [[fallthrough]]

using UBool = bool;

struct RecordOverflow { };

template<typename T, typename Policy> class Checked {
public:
    Checked(T value) : m_value(value) { }
    Checked& operator*=(T factor)
    {
        if (__builtin_mul_overflow(m_value, factor, &m_value))
            m_overflowed = true;
        return *this;
    }
    Checked& operator+=(T addend)
    {
        if (__builtin_add_overflow(m_value, addend, &m_value))
            m_overflowed = true;
        return *this;
    }
    bool hasOverflowed() const { return m_overflowed; }
    T unsafeGet() const { return m_value; }

private:
    T m_value;
    bool m_overflowed { false };
};

template<typename T> using Optional = std::optional<T>;

template<typename E> struct Unexpected {
    E error;
};

template<typename E> Unexpected<E> makeUnexpected(E error)
{
    return { error };
}

template<typename T, typename E> class Expected {
public:
    Expected(T value) : m_value(std::move(value)), m_hasValue(true) { }
    Expected(Unexpected<E> unexpected) : m_error(unexpected.error), m_hasValue(false) { }
    bool has_value() const { return m_hasValue; }
    explicit operator bool() const { return m_hasValue; }
    const T& value() const { return m_value; }
    const T& operator*() const { return m_value; }
    E error() const { return m_error; }

private:
    T m_value { };
    E m_error { };
    bool m_hasValue;
};

// Derived from Source/WTF/wtf/ASCIICType.h@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2007-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

template<typename CharacterType> inline bool isASCII(CharacterType c) { return !(c & ~0x7F); }
template<typename CharacterType> inline bool isASCIIUpper(CharacterType c) { return c >= 'A' && c <= 'Z'; }
template<typename CharacterType> inline bool isASCIILower(CharacterType c) { return c >= 'a' && c <= 'z'; }
template<typename CharacterType> inline bool isASCIIAlpha(CharacterType c) { return isASCIIUpper(c) || isASCIILower(c); }
template<typename CharacterType> inline bool isASCIIDigit(CharacterType c) { return c >= '0' && c <= '9'; }
template<typename CharacterType> inline bool isASCIIAlphanumeric(CharacterType c) { return isASCIIDigit(c) || isASCIIAlpha(c); }
template<typename CharacterType> inline bool isASCIIHexDigit(CharacterType c) { return isASCIIDigit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'); }
template<typename CharacterType> inline CharacterType toASCIILower(CharacterType c) { return c | (isASCIIUpper(c) << 5); }
template<typename CharacterType> inline std::uint8_t toASCIIHexValue(CharacterType c) { return c < 'A' ? c - '0' : (c - 'A' + 10) & 0xF; }
template<typename CharacterType> inline std::uint8_t toASCIIHexValue(CharacterType upper, CharacterType lower) { return (toASCIIHexValue(upper) << 4) | toASCIIHexValue(lower); }
inline char upperNibbleToASCIIHexDigit(std::uint8_t value) { std::uint8_t nibble = value >> 4; return nibble + (nibble < 10 ? '0' : 'A' - 10); }
inline char lowerNibbleToASCIIHexDigit(std::uint8_t value) { std::uint8_t nibble = value & 0xF; return nibble + (nibble < 10 ? '0' : 'A' - 10); }
inline char lowerNibbleToLowercaseASCIIHexDigit(std::uint8_t value) { std::uint8_t nibble = value & 0xF; return nibble + (nibble < 10 ? '0' : 'a' - 10); }

#define U8_MAX_LENGTH 4
#define U_IS_BMP(c) ((std::uint32_t)(c) <= 0xFFFF)
#define U16_LEAD(supplementary) (UChar)(((supplementary) >> 10) + 0xD7C0)
#define U16_TRAIL(supplementary) (UChar)(((supplementary) & 0x3FF) | 0xDC00)
#define U8_APPEND(s, i, capacity, c, isError) do { \
    std::uint32_t __uc = (c); \
    if (__uc <= 0x7F) (s)[(i)++] = (std::uint8_t)__uc; \
    else if (__uc <= 0x7FF && (i) + 1 < (capacity)) { (s)[(i)++] = (std::uint8_t)((__uc >> 6) | 0xC0); (s)[(i)++] = (std::uint8_t)((__uc & 0x3F) | 0x80); } \
    else if ((__uc <= 0xD7FF || (0xE000 <= __uc && __uc <= 0xFFFF)) && (i) + 2 < (capacity)) { (s)[(i)++] = (std::uint8_t)((__uc >> 12) | 0xE0); (s)[(i)++] = (std::uint8_t)(((__uc >> 6) & 0x3F) | 0x80); (s)[(i)++] = (std::uint8_t)((__uc & 0x3F) | 0x80); } \
    else if (0xFFFF < __uc && __uc <= 0x10FFFF && (i) + 3 < (capacity)) { (s)[(i)++] = (std::uint8_t)((__uc >> 18) | 0xF0); (s)[(i)++] = (std::uint8_t)(((__uc >> 12) & 0x3F) | 0x80); (s)[(i)++] = (std::uint8_t)(((__uc >> 6) & 0x3F) | 0x80); (s)[(i)++] = (std::uint8_t)((__uc & 0x3F) | 0x80); } \
    else { (isError) = true; } \
} while (0)

using UErrorCode = std::int32_t;
constexpr UErrorCode U_ZERO_ERROR = 0;
inline bool U_SUCCESS(UErrorCode code) { return code <= U_ZERO_ERROR; }
inline bool U_FAILURE(UErrorCode code) { return code > U_ZERO_ERROR; }

struct IcuIdnaInfo {
    std::int16_t size;
    std::int8_t isTransitionalDifferent;
    std::int8_t reservedB3;
    std::uint32_t errors;
    std::int32_t reservedI2;
    std::int32_t reservedI3;
};
static_assert(sizeof(IcuIdnaInfo) == 16);
#define UIDNA_INFO_INITIALIZER { static_cast<std::int16_t>(sizeof(IcuIdnaInfo)), 0, 0, 0, 0, 0 }
using UIDNAInfo = IcuIdnaInfo;
constexpr std::uint32_t UIDNA_CHECK_BIDI = 4;
constexpr std::uint32_t UIDNA_CHECK_CONTEXTJ = 8;
constexpr std::uint32_t UIDNA_NONTRANSITIONAL_TO_ASCII = 0x10;
constexpr std::uint32_t UIDNA_NONTRANSITIONAL_TO_UNICODE = 0x20;

inline UIDNA* uidna_openUTS46(std::uint32_t options, UErrorCode* error)
{
    return uidna_openUTS46_nid_postfix(options, error);
}

inline std::int32_t uidna_nameToASCII(const UIDNA* idna, const UChar* name, std::int32_t length, UChar* destination, std::int32_t capacity, UIDNAInfo* info, UErrorCode* error)
{
    return uidna_nameToASCII_nid_postfix(idna, name, length, destination, capacity, reinterpret_cast<::UIDNAInfo*>(info), error);
}

template<typename T, std::size_t inlineCapacity = 0> class Vector {
public:
    Vector() = default;
    std::size_t size() const { return m_storage.size(); }
    bool isEmpty() const { return m_storage.empty(); }
    void clear() { m_storage.clear(); }
    T* data() { return m_storage.data(); }
    const T* data() const { return m_storage.data(); }
    T& operator[](std::size_t index) { return m_storage[index]; }
    const T& operator[](std::size_t index) const { return m_storage[index]; }
    T& last() { return m_storage.back(); }
    const T& last() const { return m_storage.back(); }
    void append(const T& value) { m_storage.push_back(value); }
    template<typename U> void append(const U* values, std::size_t count) { m_storage.insert(m_storage.end(), values, values + count); }
    void uncheckedAppend(const T& value) { m_storage.push_back(value); }
    void reserveCapacity(std::size_t capacity) { m_storage.reserve(capacity); }
    void reserveInitialCapacity(std::size_t capacity) { m_storage.reserve(capacity); }
    void shrink(std::size_t size) { m_storage.resize(std::min(size, m_storage.size())); }
    void resize(std::size_t size) { m_storage.resize(size); }
    void remove(std::size_t position, std::size_t count) { m_storage.erase(m_storage.begin() + position, m_storage.begin() + position + count); }
    const T* begin() const { return m_storage.data(); }
    const T* end() const { return m_storage.data() + m_storage.size(); }
    T takeLast() { T value = std::move(m_storage.back()); m_storage.pop_back(); return value; }
    std::vector<T>& storage() { return m_storage; }

private:
    std::vector<T> m_storage;
};

class StringView;

class String {
public:
    String() = default;
    explicit String(WtfAbi::String string) : m_string(std::move(string)) { }
    bool isNull() const { return !m_string.impl; }
    bool isEmpty() const { return !length(); }
    unsigned length() const { return m_string.impl ? m_string.impl->length : 0; }
    bool is8Bit() const { return !m_string.impl || (m_string.impl->hashAndFlags & 4); }
    const LChar* characters8() const { return m_string.impl ? static_cast<const LChar*>(m_string.impl->data) : nullptr; }
    const UChar* characters16() const { return m_string.impl ? static_cast<const UChar*>(m_string.impl->data) : nullptr; }
    UChar operator[](unsigned index) const { return is8Bit() ? characters8()[index] : characters16()[index]; }
    bool isAllASCII() const
    {
        for (unsigned i = 0; i < length(); ++i) {
            if (!isASCII((*this)[i]))
                return false;
        }
        return true;
    }
    String* impl() { return this; }
    void remove(unsigned start, unsigned length);
    std::string utf8() const;
    String left(unsigned length) const;
    String substring(unsigned start, unsigned length = 0xFFFFFFFFu) const;
    const WtfAbi::String& abi() const { return m_string; }

    static String fromUTF8(const LChar* characters, std::size_t length)
    {
        return String(_ZN3WTF6String8fromUTF8EPKhm(characters, length));
    }

    template<std::size_t inlineCapacity> static String adopt(Vector<LChar, inlineCapacity>&& buffer)
    {
        return fromLatin1(buffer.data(), buffer.size());
    }

    static String fromLatin1(const LChar* characters, std::size_t length)
    {
        void* destination = nullptr;
        String result(WtfAbi::String(WtfAbi::CreateUninitialized(static_cast<unsigned>(length), true, destination)));
        if (length)
            std::memcpy(destination, characters, length);
        return result;
    }

    static String fromUTF16(const UChar* characters, std::size_t length)
    {
        void* destination = nullptr;
        String result(WtfAbi::String(WtfAbi::CreateUninitialized(static_cast<unsigned>(length), false, destination)));
        if (length)
            std::memcpy(destination, characters, length * sizeof(UChar));
        return result;
    }

private:
    WtfAbi::String m_string;
};

using StringImpl = String;

class StringView {
public:
    StringView() = default;
    StringView(const String& string)
        : m_characters(string.is8Bit() ? static_cast<const void*>(string.characters8()) : static_cast<const void*>(string.characters16()))
        , m_length(string.length())
        , m_is8Bit(string.is8Bit())
    {
    }
    StringView(const LChar* characters, std::size_t length) : m_characters(characters), m_length(static_cast<unsigned>(length)), m_is8Bit(true) { }
    StringView(const UChar* characters, std::size_t length) : m_characters(characters), m_length(static_cast<unsigned>(length)), m_is8Bit(false) { }
    unsigned length() const { return m_length; }
    bool isEmpty() const { return !m_length; }
    bool isNull() const { return !m_characters; }
    bool is8Bit() const { return m_is8Bit; }
    const LChar* characters8() const { return static_cast<const LChar*>(m_characters); }
    const UChar* characters16() const { return static_cast<const UChar*>(m_characters); }
    UChar operator[](unsigned index) const { return m_is8Bit ? characters8()[index] : characters16()[index]; }
    StringView substring(unsigned start, unsigned length = 0xFFFFFFFFu) const
    {
        if (start >= m_length)
            return m_characters ? StringView(reinterpret_cast<const LChar*>(""), 0) : StringView();
        unsigned maxLength = m_length - start;
        if (length >= maxLength)
            length = maxLength;
        if (m_is8Bit)
            return StringView(characters8() + start, length);
        return StringView(characters16() + start, length);
    }

    class UpconvertedCharacters {
    public:
        explicit UpconvertedCharacters(const StringView& view)
        {
            m_characters.reserve(view.length());
            for (unsigned i = 0; i < view.length(); ++i)
                m_characters.push_back(view[i]);
        }
        operator const UChar*() const { return m_characters.data(); }

    private:
        std::u16string m_characters;
    };
    UpconvertedCharacters upconvertedCharacters() const { return UpconvertedCharacters(*this); }

    std::size_t find(UChar character, unsigned start = 0) const
    {
        for (unsigned i = start; i < m_length; ++i) {
            if ((*this)[i] == character)
                return i;
        }
        return notFound;
    }
    std::size_t find(bool (*matches)(UChar)) const
    {
        for (unsigned i = 0; i < m_length; ++i) {
            if (matches((*this)[i]))
                return i;
        }
        return notFound;
    }
    std::size_t reverseFind(UChar character) const
    {
        for (unsigned i = m_length; i; --i) {
            if ((*this)[i - 1] == character)
                return i - 1;
        }
        return notFound;
    }
    bool contains(UChar character) const { return find(character) != notFound; }
    bool startsWith(UChar character) const { return m_length && (*this)[0] == character; }
    StringView left(unsigned length) const { return substring(0, length); }
    bool isAllASCII() const
    {
        for (unsigned i = 0; i < m_length; ++i) {
            if (!isASCII((*this)[i]))
                return false;
        }
        return true;
    }
    String toStringWithoutCopying() const { return toString(); }

    String toString() const
    {
        if (!m_characters)
            return String();
        return m_is8Bit ? String::fromLatin1(characters8(), m_length) : String::fromUTF16(characters16(), m_length);
    }

private:
    const void* m_characters { nullptr };
    unsigned m_length { 0 };
    bool m_is8Bit { true };
};

inline String String::left(unsigned length) const
{
    if (length >= this->length())
        return *this;
    return StringView(*this).substring(0, length).toString();
}

inline String String::substring(unsigned start, unsigned length) const
{
    if (!start && length >= this->length())
        return *this;
    return StringView(*this).substring(start, length).toString();
}

inline bool operator==(StringView a, StringView b)
{
    if (a.length() != b.length())
        return false;
    for (unsigned i = 0; i < a.length(); ++i) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}

inline bool operator!=(StringView a, StringView b) { return !(a == b); }

template<std::size_t length> inline bool startsWithLettersIgnoringASCIICase(StringView string, const char (&lowercaseLetters)[length])
{
    if (string.length() < length - 1)
        return false;
    for (unsigned i = 0; i < length - 1; ++i) {
        if (toASCIILower(string[i]) != static_cast<UChar>(lowercaseLetters[i]))
            return false;
    }
    return true;
}

class MakeStringPart {
public:
    MakeStringPart(StringView view) : m_is8Bit(view.is8Bit()) { for (unsigned i = 0; i < view.length(); ++i) m_characters.push_back(view[i]); }
    MakeStringPart(const String& string) : MakeStringPart(StringView(string)) { }
    MakeStringPart(const char* characters) { for (; *characters; ++characters) m_characters.push_back(static_cast<LChar>(*characters)); }
    MakeStringPart(char character) { m_characters.push_back(static_cast<LChar>(character)); }
    MakeStringPart(unsigned number) { for (char digit : std::to_string(number)) m_characters.push_back(static_cast<LChar>(digit)); }
    bool is8Bit() const { return m_is8Bit; }
    const std::u16string& characters() const { return m_characters; }

private:
    std::u16string m_characters;
    bool m_is8Bit { true };
};

template<typename... Parts> String makeString(const Parts&... parts)
{
    const MakeStringPart converted[] = { MakeStringPart(parts)... };
    std::u16string characters;
    bool is8Bit = true;
    for (const auto& part : converted) {
        characters += part.characters();
        is8Bit &= part.is8Bit();
    }
    if (is8Bit) {
        std::string latin1(characters.begin(), characters.end());
        return String::fromLatin1(reinterpret_cast<const LChar*>(latin1.data()), latin1.size());
    }
    return String::fromUTF16(characters.data(), characters.size());
}

inline void String::remove(unsigned start, unsigned length)
{
    StringView view(*this);
    *this = makeString(view.left(start), view.substring(start + length));
}

inline std::string String::utf8() const
{
    std::string output;
    auto append = [&output](std::uint32_t c) {
        if (c < 0x80)
            output.push_back(static_cast<char>(c));
        else if (c < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (c >> 6)));
            output.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            output.push_back(static_cast<char>(0xE0 | (c >> 12)));
            output.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (c >> 18)));
            output.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    };
    for (unsigned i = 0; i < length(); ++i) {
        std::uint32_t c = (*this)[i];
        if ((c & 0xFC00) == 0xD800 && i + 1 < length() && ((*this)[i + 1] & 0xFC00) == 0xDC00) {
            c = (c << 10) + (*this)[i + 1] - ((0xD800u << 10) + 0xDC00u - 0x10000u);
            ++i;
        } else if ((c & 0xF800) == 0xD800 && !((c & 0xFC00) == 0xD800 && i + 1 == length()))
            c = 0xFFFD;
        append(c);
    }
    return output;
}

class URLTextEncoding {
public:
    const void* const* vtable;
};

struct EncodedBytes {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
};
static_assert(sizeof(EncodedBytes) == 16);

inline Vector<std::uint8_t> encodeForURLParsing(const URLTextEncoding& encoding, StringView view)
{
    using Function = void (APS5_VABI*)(EncodedBytes*, const URLTextEncoding*, WtfAbi::StringView);
    EncodedBytes bytes { nullptr, 0, 0 };
    reinterpret_cast<Function>(const_cast<void*>(encoding.vtable[0]))(&bytes, &encoding, { view.is8Bit() ? static_cast<const void*>(view.characters8()) : static_cast<const void*>(view.characters16()), view.length(), view.is8Bit() });
    Vector<std::uint8_t> result;
    result.append(bytes.buffer, bytes.size);
    if (bytes.buffer)
        _ZN3WTF8fastFreeEPv(bytes.buffer);
    return result;
}

class URL {
public:
    bool isValid() const { return m_isValid; }
    const String& string() const { return m_string; }
    bool protocolIs(const char* protocol) const
    {
        if (!m_isValid)
            return false;
        for (unsigned i = 0; i < m_schemeEnd; ++i) {
            if (!protocol[i] || (m_string[i] | 0x20) != static_cast<UChar>(protocol[i]))
                return false;
        }
        return !protocol[m_schemeEnd];
    }
    bool protocolIs(StringView protocol) const
    {
        if (!m_isValid || m_schemeEnd != protocol.length())
            return false;
        for (unsigned i = 0; i < m_schemeEnd; ++i) {
            if ((m_string[i] | 0x20) != protocol[i])
                return false;
        }
        return true;
    }
    static constexpr unsigned maxPortLength = (1 << 3) - 1;
    static constexpr unsigned maxSchemeLength = (1 << 26) - 1;
    unsigned hostStart() const { return (m_passwordEnd == m_userStart) ? m_passwordEnd : m_passwordEnd + 1; }
    StringView host() const
    {
        if (!m_isValid)
            return { };
        unsigned start = hostStart();
        return StringView(m_string).substring(start, m_hostEnd - start);
    }
    StringView path() const
    {
        if (!m_isValid)
            return { };
        return StringView(m_string).substring(pathStart(), m_pathEnd - pathStart());
    }
    unsigned pathStart() const
    {
        unsigned start = m_hostEnd + m_portLength;
        if (start == m_schemeEnd + 1U && start + 1 < m_string.length() && m_string[start] == '/' && m_string[start + 1] == '.')
            start += 2;
        return start;
    }
    void invalidate()
    {
        m_isValid = false;
        m_protocolIsInHTTPFamily = false;
        m_cannotBeABaseURL = false;
        m_schemeEnd = 0;
        m_userStart = 0;
        m_userEnd = 0;
        m_passwordEnd = 0;
        m_hostEnd = 0;
        m_portLength = 0;
        m_pathEnd = 0;
        m_pathAfterLastSlash = 0;
        m_queryEnd = 0;
    }
    bool hasSpecialScheme() const;
    Optional<std::uint16_t> port() const;
    bool isLocalFile() const { return protocolIs("file"); }
    unsigned credentialsEnd() const;
    void parse(const String&);
    void remove(unsigned start, unsigned length);
    bool setProtocol(StringView);
    void setHost(StringView);
    void setHostAndPort(StringView);
    void setUser(StringView);
    void setPassword(StringView);
    void removeCredentials();
    void setFragmentIdentifier(StringView);
    void setQuery(StringView);
    void setPath(StringView);
    URL truncatedForUseAsBase() const;
    static URL fileURLWithFileSystemPath(StringView);
    void removeFragmentIdentifier()
    {
        if (!m_isValid)
            return;
        m_string = m_string.left(m_queryEnd);
    }

    String m_string;
    unsigned m_isValid : 1 { 0 };
    unsigned m_protocolIsInHTTPFamily : 1 { 0 };
    unsigned m_cannotBeABaseURL : 1 { 0 };
    unsigned m_portLength : 3 { 0 };
    unsigned m_schemeEnd : 26 { 0 };
    unsigned m_userStart { 0 };
    unsigned m_userEnd { 0 };
    unsigned m_passwordEnd { 0 };
    unsigned m_hostEnd { 0 };
    unsigned m_pathAfterLastSlash { 0 };
    unsigned m_pathEnd { 0 };
    unsigned m_queryEnd { 0 };
};

template<typename CharacterType> class CodePointIterator {
public:
    CodePointIterator() { }
    CodePointIterator(const CharacterType* begin, const CharacterType* end)
        : m_begin(begin)
        , m_end(end)
    {
    }
    CodePointIterator(const CodePointIterator& begin, const CodePointIterator& end)
        : CodePointIterator(begin.m_begin, end.m_begin)
    {
    }
    UChar32 operator*() const;
    CodePointIterator& operator++();
    bool operator==(const CodePointIterator& other) const { return m_begin == other.m_begin && m_end == other.m_end; }
    bool operator!=(const CodePointIterator& other) const { return !(*this == other); }
    bool atEnd() const { return m_begin >= m_end; }
    std::size_t codeUnitsSince(const CharacterType* reference) const { return m_begin - reference; }
    std::size_t codeUnitsSince(const CodePointIterator& other) const { return codeUnitsSince(other.m_begin); }

private:
    const CharacterType* m_begin { nullptr };
    const CharacterType* m_end { nullptr };
};

template<> inline UChar32 CodePointIterator<LChar>::operator*() const
{
    return *m_begin;
}

template<> inline auto CodePointIterator<LChar>::operator++() -> CodePointIterator&
{
    m_begin++;
    return *this;
}

template<> inline UChar32 CodePointIterator<UChar>::operator*() const
{
    UChar32 c = m_begin[0];
    if ((c & 0xFC00) == 0xD800 && m_end - m_begin > 1 && (m_begin[1] & 0xFC00) == 0xDC00)
        c = (c << 10) + m_begin[1] - ((0xD800 << 10) + 0xDC00 - 0x10000);
    return c;
}

template<> inline auto CodePointIterator<UChar>::operator++() -> CodePointIterator&
{
    if ((m_begin[0] & 0xFC00) == 0xD800 && m_end - m_begin > 1 && (m_begin[1] & 0xFC00) == 0xDC00)
        m_begin += 2;
    else
        m_begin += 1;
    return *this;
}

class URLParser {
public:

    static const UIDNA& internationalDomainNameTranscoder();
    static bool isInUserInfoEncodeSet(UChar);

    static Optional<uint16_t> defaultPortForProtocol(StringView);

public:
    URLParser(const String&, const URL& = { }, const URLTextEncoding* = nullptr);
    URL result() { return m_url; }

    URL m_url;
    Vector<LChar> m_asciiBuffer;
    bool m_urlIsSpecial { false };
    bool m_urlIsFile { false };
    bool m_hostHasPercentOrNonASCII { false };
    String m_inputString;
    const void* m_inputBegin { nullptr };

    bool m_didSeeSyntaxViolation { false };
    static constexpr size_t defaultInlineBufferSize = 2048;
    using LCharBuffer = Vector<LChar, defaultInlineBufferSize>;

    template<typename CharacterType> void parse(const CharacterType*, const unsigned length, const URL&, const URLTextEncoding*);
    template<typename CharacterType> void parseAuthority(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parseHostAndPort(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool parsePort(CodePointIterator<CharacterType>&);

    void failure();
    enum class ReportSyntaxViolation { No, Yes };
    template<typename CharacterType, ReportSyntaxViolation reportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>& iterator) { advance<CharacterType, reportSyntaxViolation>(iterator, iterator); }
    template<typename CharacterType, ReportSyntaxViolation = ReportSyntaxViolation::Yes>
    void advance(CodePointIterator<CharacterType>&, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    template<typename CharacterType> bool takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>);
    template<typename CharacterType> void syntaxViolation(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> bool isPercentEncodedDot(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isWindowsDriveLetter(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isSingleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool isDoubleDotPathSegment(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool shouldCopyFileURL(CodePointIterator<CharacterType>);
    template<typename CharacterType> bool checkLocalhostCodePoint(CodePointIterator<CharacterType>&, UChar32);
    template<typename CharacterType> bool isAtLocalhost(CodePointIterator<CharacterType>);
    bool isLocalhost(StringView);
    template<typename CharacterType> void consumeSingleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void consumeDoubleDotPathSegment(CodePointIterator<CharacterType>&);
    template<typename CharacterType> void appendWindowsDriveLetter(CodePointIterator<CharacterType>&);
    template<typename CharacterType> size_t currentPosition(const CodePointIterator<CharacterType>&);
    template<typename UnsignedIntegerType> void appendNumberToASCIIBuffer(UnsignedIntegerType);
    template<bool(*isInCodeSet)(UChar32), typename CharacterType> void utf8PercentEncode(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> void utf8QueryEncode(const CodePointIterator<CharacterType>&);
    template<typename CharacterType> Optional<LCharBuffer> domainToASCII(StringImpl&, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    template<typename CharacterType> LCharBuffer percentDecode(const LChar*, size_t, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition);
    static LCharBuffer percentDecode(const LChar*, size_t);
    static bool hasForbiddenHostCodePoint(const LCharBuffer&);
    void percentEncodeByte(uint8_t);
    void appendToASCIIBuffer(UChar32);
    void appendToASCIIBuffer(const char*, size_t);
    void appendToASCIIBuffer(const LChar* characters, size_t size) { appendToASCIIBuffer(reinterpret_cast<const char*>(characters), size); }
    template<typename CharacterType> void encodeNonUTF8Query(const Vector<UChar>& source, const URLTextEncoding&, CodePointIterator<CharacterType>);
    void copyASCIIStringUntil(const String&, size_t length);
    bool copyBaseWindowsDriveLetter(const URL&);
    StringView parsedDataView(size_t start, size_t length);
    UChar parsedDataView(size_t position);
    template<typename CharacterType> bool startsWithXNDashDash(CodePointIterator<CharacterType>);

    bool needsNonSpecialDotSlash() const;
    void addNonSpecialDotSlash();

    using IPv4Address = uint32_t;
    void serializeIPv4(IPv4Address);
    enum class IPv4ParsingError;
    enum class IPv4PieceParsingError;
    template<typename CharacterTypeForSyntaxViolation, typename CharacterType> Expected<IPv4Address, IPv4ParsingError> parseIPv4Host(const CodePointIterator<CharacterTypeForSyntaxViolation>&, CodePointIterator<CharacterType>);
    template<typename CharacterType> Expected<uint32_t, URLParser::IPv4PieceParsingError> parseIPv4Piece(CodePointIterator<CharacterType>&, bool& syntaxViolation);
    using IPv6Address = std::array<uint16_t, 8>;
    template<typename CharacterType> Optional<IPv6Address> parseIPv6Host(CodePointIterator<CharacterType>);
    template<typename CharacterType> Optional<uint32_t> parseIPv4PieceInsideIPv6(CodePointIterator<CharacterType>&);
    template<typename CharacterType> Optional<IPv4Address> parseIPv4AddressInsideIPv6(CodePointIterator<CharacterType>);
    void serializeIPv6Piece(uint16_t piece);
    void serializeIPv6(IPv6Address);

    enum class URLPart;
    template<typename CharacterType> void copyURLPartsUntil(const URL& base, URLPart, const CodePointIterator<CharacterType>&, const URLTextEncoding*&);
    static size_t urlLengthUntilPart(const URL&, URLPart);
    void popPath();
    bool shouldPopPath(unsigned);
};

ALWAYS_INLINE static void appendCodePoint(Vector<UChar>& destination, UChar32 codePoint)
{
    if (U_IS_BMP(codePoint)) {
        destination.append(static_cast<UChar>(codePoint));
        return;
    }
    destination.reserveCapacity(destination.size() + 2);
    destination.uncheckedAppend(U16_LEAD(codePoint));
    destination.uncheckedAppend(U16_TRAIL(codePoint));
}

enum URLCharacterClass {
    UserInfo = 0x1,
    Default = 0x2,
    ForbiddenHost = 0x4,
    QueryPercent = 0x8,
    SlashQuestionOrHash = 0x10,
    ValidScheme = 0x20,
};

static const uint8_t characterClassTable[256] = {
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    0,
    UserInfo | Default | QueryPercent,
    UserInfo | Default | QueryPercent | SlashQuestionOrHash | ForbiddenHost,
    0,
    ForbiddenHost,
    0,
    0,
    0,
    0,
    0,
    ValidScheme,
    0,
    ValidScheme,
    ValidScheme,
    UserInfo | SlashQuestionOrHash | ForbiddenHost,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    UserInfo | ForbiddenHost,
    UserInfo,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo,
    UserInfo | Default | QueryPercent | ForbiddenHost,
    UserInfo | Default | SlashQuestionOrHash | ForbiddenHost,
    UserInfo | ForbiddenHost,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    UserInfo | ForbiddenHost,
    UserInfo | SlashQuestionOrHash | ForbiddenHost,
    UserInfo | ForbiddenHost,
    UserInfo | ForbiddenHost,
    0,
    UserInfo | Default,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    ValidScheme,
    UserInfo | Default,
    UserInfo | ForbiddenHost,
    UserInfo | Default,
    0,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
    QueryPercent,
};

template<typename CharacterType> ALWAYS_INLINE static bool isC0Control(CharacterType character) { return character <= 0x1F; }
template<typename CharacterType> ALWAYS_INLINE static bool isC0ControlOrSpace(CharacterType character) { return character <= 0x20; }
template<typename CharacterType> ALWAYS_INLINE static bool isTabOrNewline(CharacterType character) { return character <= 0xD && character >= 0x9 && character != 0xB && character != 0xC; }
template<typename CharacterType> ALWAYS_INLINE static bool isInSimpleEncodeSet(CharacterType character) { return character > 0x7E || isC0Control(character); }
template<typename CharacterType> ALWAYS_INLINE static bool isInFragmentEncodeSet(CharacterType character) { return character > 0x7E || character == '`' || ((characterClassTable[character] & QueryPercent) && character != '#'); }
template<typename CharacterType> ALWAYS_INLINE static bool isInDefaultEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & Default; }
template<typename CharacterType> ALWAYS_INLINE static bool isInUserInfoEncodeSet(CharacterType character) { return character > 0x7E || characterClassTable[character] & UserInfo; }
template<typename CharacterType> ALWAYS_INLINE static bool isPercentOrNonASCII(CharacterType character) { return !isASCII(character) || character == '%'; }
template<typename CharacterType> ALWAYS_INLINE static bool isSlashQuestionOrHash(CharacterType character) { return character <= '\\' && characterClassTable[character] & SlashQuestionOrHash; }
template<typename CharacterType> ALWAYS_INLINE static bool isValidSchemeCharacter(CharacterType character) { return character <= 'z' && characterClassTable[character] & ValidScheme; }
template<typename CharacterType> ALWAYS_INLINE static bool isForbiddenHostCodePoint(CharacterType character) { return character <= '|' && characterClassTable[character] & ForbiddenHost; }
ALWAYS_INLINE static bool shouldPercentEncodeQueryByte(uint8_t byte, const bool& urlIsSpecial)
{
    if (characterClassTable[byte] & QueryPercent)
        return true;
    if (byte == '\'' && urlIsSpecial)
        return true;
    return false;
}

bool URLParser::isInUserInfoEncodeSet(UChar c)
{
    return ::NKWebKit::UrlParserPort::isInUserInfoEncodeSet<UChar>(c);
}

template<typename CharacterType, URLParser::ReportSyntaxViolation reportSyntaxViolation>
ALWAYS_INLINE void URLParser::advance(CodePointIterator<CharacterType>& iterator, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    ++iterator;
    while (UNLIKELY(!iterator.atEnd() && isTabOrNewline(*iterator))) {
        if (reportSyntaxViolation == ReportSyntaxViolation::Yes)
            syntaxViolation(iteratorForSyntaxViolationPosition);
        ++iterator;
    }
}

template<typename CharacterType>
bool URLParser::takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd())
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd())
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return iterator.atEnd();
}

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isWindowsDriveLetter(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd() || !isASCIIAlpha(*iterator))
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd())
        return false;
    if (*iterator != ':' && *iterator != '|')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return iterator.atEnd() || *iterator == '/' || *iterator == '\\' || *iterator == '?' || *iterator == '#';
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(UChar32 codePoint)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(codePoint);
}

ALWAYS_INLINE void URLParser::appendToASCIIBuffer(const char* characters, size_t length)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        m_asciiBuffer.append(characters, length);
}

template<typename CharacterType>
void URLParser::appendWindowsDriveLetter(CodePointIterator<CharacterType>& iterator)
{
    auto lengthWithOnlyOneSlashInPath = m_url.m_hostEnd + m_url.m_portLength + 1;
    if (m_url.m_pathAfterLastSlash > lengthWithOnlyOneSlashInPath) {
        syntaxViolation(iterator);
        m_url.m_pathAfterLastSlash = lengthWithOnlyOneSlashInPath;
        m_asciiBuffer.resize(lengthWithOnlyOneSlashInPath);
    }
    appendToASCIIBuffer(*iterator);
    advance(iterator);
    if (*iterator == '|')
        syntaxViolation(iterator);
    appendToASCIIBuffer(':');
    advance(iterator);
}

bool URLParser::copyBaseWindowsDriveLetter(const URL& base)
{
    if (base.protocolIs("file")) {
        RELEASE_ASSERT(base.m_hostEnd + base.m_portLength < base.m_string.length());
        if (base.m_string.is8Bit()) {
            const LChar* begin = base.m_string.characters8();
            CodePointIterator<LChar> c(begin + base.m_hostEnd + base.m_portLength + 1, begin + base.m_string.length());
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                return true;
            }
        } else {
            const UChar* begin = base.m_string.characters16();
            CodePointIterator<UChar> c(begin + base.m_hostEnd + base.m_portLength + 1, begin + base.m_string.length());
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                return true;
            }
        }
    }
    return false;
}

template<typename CharacterType>
bool URLParser::shouldCopyFileURL(CodePointIterator<CharacterType> iterator)
{
    if (!isWindowsDriveLetter(iterator))
        return true;
    if (iterator.atEnd())
        return false;
    advance(iterator);
    if (iterator.atEnd())
        return true;
    advance(iterator);
    if (iterator.atEnd())
        return true;
    return !isSlashQuestionOrHash(*iterator);
}

static void percentEncodeByte(uint8_t byte, Vector<LChar>& buffer)
{
    buffer.append('%');
    buffer.append(upperNibbleToASCIIHexDigit(byte));
    buffer.append(lowerNibbleToASCIIHexDigit(byte));
}

void URLParser::percentEncodeByte(uint8_t byte)
{
    appendToASCIIBuffer('%');
    appendToASCIIBuffer(upperNibbleToASCIIHexDigit(byte));
    appendToASCIIBuffer(lowerNibbleToASCIIHexDigit(byte));
}

const char replacementCharacterUTF8PercentEncoded[10] = "%EF%BF%BD";
const size_t replacementCharacterUTF8PercentEncodedLength = sizeof(replacementCharacterUTF8PercentEncoded) - 1;

template<bool(*isInCodeSet)(UChar32), typename CharacterType>
ALWAYS_INLINE void URLParser::utf8PercentEncode(const CodePointIterator<CharacterType>& iterator)
{
    UChar32 codePoint = *iterator;
    if (LIKELY(isASCII(codePoint))) {
        if (UNLIKELY(isInCodeSet(codePoint))) {
            syntaxViolation(iterator);
            percentEncodeByte(codePoint);
        } else
            appendToASCIIBuffer(codePoint);
        return;
    }
    syntaxViolation(iterator);

    uint8_t buffer[U8_MAX_LENGTH];
    int32_t offset = 0;
    UBool isError = false;
    U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, isError);
    if (isError) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded, replacementCharacterUTF8PercentEncodedLength);
        return;
    }
    for (int32_t i = 0; i < offset; ++i)
        percentEncodeByte(buffer[i]);
}

template<typename CharacterType>
ALWAYS_INLINE void URLParser::utf8QueryEncode(const CodePointIterator<CharacterType>& iterator)
{
    UChar32 codePoint = *iterator;
    if (LIKELY(isASCII(codePoint))) {
        if (UNLIKELY(shouldPercentEncodeQueryByte(codePoint, m_urlIsSpecial))) {
            syntaxViolation(iterator);
            percentEncodeByte(codePoint);
        } else
            appendToASCIIBuffer(codePoint);
        return;
    }

    syntaxViolation(iterator);

    uint8_t buffer[U8_MAX_LENGTH];
    int32_t offset = 0;
    UBool isError = false;
    U8_APPEND(buffer, offset, U8_MAX_LENGTH, codePoint, isError);
    if (isError) {
        appendToASCIIBuffer(replacementCharacterUTF8PercentEncoded, replacementCharacterUTF8PercentEncodedLength);
        return;
    }
    for (int32_t i = 0; i < offset; ++i) {
        auto byte = buffer[i];
        if (shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))
            percentEncodeByte(byte);
        else
            appendToASCIIBuffer(byte);
    }
}

template<typename CharacterType>
void URLParser::encodeNonUTF8Query(const Vector<UChar>& source, const URLTextEncoding& encoding, CodePointIterator<CharacterType> iterator)
{
    auto encoded = encodeForURLParsing(encoding, StringView(source.data(), source.size()));
    auto* data = encoded.data();
    size_t length = encoded.size();

    if (!length == !iterator.atEnd()) {
        syntaxViolation(iterator);
        return;
    }

    size_t i = 0;
    for (; i < length; ++i) {
        uint8_t byte = data[i];
        if (UNLIKELY(byte != *iterator)) {
            syntaxViolation(iterator);
            break;
        }
        if (UNLIKELY(shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))) {
            syntaxViolation(iterator);
            break;
        }
        appendToASCIIBuffer(byte);
        ++iterator;
    }
    while (!iterator.atEnd() && isTabOrNewline(*iterator))
        ++iterator;
    for (; i < length; ++i) {
        uint8_t byte = data[i];
        if (shouldPercentEncodeQueryByte(byte, m_urlIsSpecial))
            percentEncodeByte(byte);
        else
            appendToASCIIBuffer(byte);
    }
}

Optional<uint16_t> URLParser::defaultPortForProtocol(StringView scheme)
{
    static constexpr uint16_t ftpPort = 21;
    static constexpr uint16_t httpPort = 80;
    static constexpr uint16_t httpsPort = 443;
    static constexpr uint16_t wsPort = 80;
    static constexpr uint16_t wssPort = 443;

    auto length = scheme.length();
    if (!length)
        return std::nullopt;
    switch (scheme[0]) {
    case 'w':
        switch (length) {
        case 2:
            if (scheme[1] == 's')
                return wsPort;
            return std::nullopt;
        case 3:
            if (scheme[1] == 's'
                && scheme[2] == 's')
                return wssPort;
            return std::nullopt;
        default:
            return false;
        }
    case 'h':
        switch (length) {
        case 4:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p')
                return httpPort;
            return std::nullopt;
        case 5:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's')
                return httpsPort;
            return std::nullopt;
        default:
            return std::nullopt;
        }
    case 'f':
        if (length == 3
            && scheme[1] == 't'
            && scheme[2] == 'p')
            return ftpPort;
        return std::nullopt;
    default:
        return std::nullopt;
    }
}

enum class Scheme {
    WS,
    WSS,
    File,
    FTP,
    HTTP,
    HTTPS,
    NonSpecial
};

ALWAYS_INLINE static Scheme scheme(StringView scheme)
{
    auto length = scheme.length();
    if (!length)
        return Scheme::NonSpecial;
    switch (scheme[0]) {
    case 'f':
        switch (length) {
        case 3:
            if (scheme[1] == 't'
                && scheme[2] == 'p')
                return Scheme::FTP;
            return Scheme::NonSpecial;
        case 4:
            if (scheme[1] == 'i'
                && scheme[2] == 'l'
                && scheme[3] == 'e')
                return Scheme::File;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    case 'h':
        switch (length) {
        case 4:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p')
                return Scheme::HTTP;
            return Scheme::NonSpecial;
        case 5:
            if (scheme[1] == 't'
                && scheme[2] == 't'
                && scheme[3] == 'p'
                && scheme[4] == 's')
                return Scheme::HTTPS;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    case 'w':
        switch (length) {
        case 2:
            if (scheme[1] == 's')
                return Scheme::WS;
            return Scheme::NonSpecial;
        case 3:
            if (scheme[1] == 's'
                && scheme[2] == 's')
                return Scheme::WSS;
            return Scheme::NonSpecial;
        default:
            return Scheme::NonSpecial;
        }
    default:
        return Scheme::NonSpecial;
    }
}

enum class URLParser::URLPart {
    SchemeEnd,
    UserStart,
    UserEnd,
    PasswordEnd,
    HostEnd,
    PortEnd,
    PathAfterLastSlash,
    PathEnd,
    QueryEnd,
};

size_t URLParser::urlLengthUntilPart(const URL& url, URLPart part)
{
    switch (part) {
    case URLPart::QueryEnd:
        return url.m_queryEnd;
    case URLPart::PathEnd:
        return url.m_pathEnd;
    case URLPart::PathAfterLastSlash:
        return url.m_pathAfterLastSlash;
    case URLPart::PortEnd:
        return url.m_hostEnd + url.m_portLength;
    case URLPart::HostEnd:
        return url.m_hostEnd;
    case URLPart::PasswordEnd:
        return url.m_passwordEnd;
    case URLPart::UserEnd:
        return url.m_userEnd;
    case URLPart::UserStart:
        return url.m_userStart;
    case URLPart::SchemeEnd:
        return url.m_schemeEnd;
    }
    return 0;
}

void URLParser::copyASCIIStringUntil(const String& string, size_t length)
{
    RELEASE_ASSERT(length <= string.length());
    if (string.isNull())
        return;
    if (string.is8Bit())
        appendToASCIIBuffer(string.characters8(), length);
    else {
        const UChar* characters = string.characters16();
        for (size_t i = 0; i < length; ++i) {
            UChar c = characters[i];
            appendToASCIIBuffer(c);
        }
    }
}

template<typename CharacterType>
void URLParser::copyURLPartsUntil(const URL& base, URLPart part, const CodePointIterator<CharacterType>& iterator, const URLTextEncoding*& nonUTF8QueryEncoding)
{
    syntaxViolation(iterator);

    m_asciiBuffer.clear();
    copyASCIIStringUntil(base.m_string, urlLengthUntilPart(base, part));
    switch (part) {
    case URLPart::QueryEnd:
        m_url.m_queryEnd = base.m_queryEnd;
        FALLTHROUGH;
    case URLPart::PathEnd:
        m_url.m_pathEnd = base.m_pathEnd;
        FALLTHROUGH;
    case URLPart::PathAfterLastSlash:
        m_url.m_pathAfterLastSlash = base.m_pathAfterLastSlash;
        FALLTHROUGH;
    case URLPart::PortEnd:
        m_url.m_portLength = base.m_portLength;
        FALLTHROUGH;
    case URLPart::HostEnd:
        m_url.m_hostEnd = base.m_hostEnd;
        FALLTHROUGH;
    case URLPart::PasswordEnd:
        m_url.m_passwordEnd = base.m_passwordEnd;
        FALLTHROUGH;
    case URLPart::UserEnd:
        m_url.m_userEnd = base.m_userEnd;
        FALLTHROUGH;
    case URLPart::UserStart:
        m_url.m_userStart = base.m_userStart;
        FALLTHROUGH;
    case URLPart::SchemeEnd:
        m_url.m_isValid = base.m_isValid;
        m_url.m_protocolIsInHTTPFamily = base.m_protocolIsInHTTPFamily;
        m_url.m_schemeEnd = base.m_schemeEnd;
    }

    switch (scheme(StringView(m_asciiBuffer.data(), m_url.m_schemeEnd))) {
    case Scheme::WS:
    case Scheme::WSS:
        nonUTF8QueryEncoding = nullptr;
        m_urlIsSpecial = true;
        return;
    case Scheme::File:
        m_urlIsFile = true;
        FALLTHROUGH;
    case Scheme::FTP:
    case Scheme::HTTP:
    case Scheme::HTTPS:
        m_urlIsSpecial = true;
        return;
    case Scheme::NonSpecial:
        m_urlIsSpecial = false;
        nonUTF8QueryEncoding = nullptr;
        auto pathStart = m_url.m_hostEnd + m_url.m_portLength;
        if (pathStart + 2 < m_asciiBuffer.size()
            && m_asciiBuffer[pathStart] == '/'
            && m_asciiBuffer[pathStart + 1] == '.'
            && m_asciiBuffer[pathStart + 2] == '/') {
            m_asciiBuffer.remove(pathStart + 1, 2);
            m_url.m_pathAfterLastSlash = std::max(2u, m_url.m_pathAfterLastSlash) - 2;
            m_url.m_pathEnd = std::max(2u, m_url.m_pathEnd) - 2;
            m_url.m_queryEnd = std::max(2u, m_url.m_queryEnd) - 2;
        }
        return;
    }
}

static const char dotASCIICode[2] = {'2', 'e'};

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isSingleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    if (*c != '%')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd())
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return c.atEnd() || isSlashQuestionOrHash(*c);
    }
    return false;
}

template<typename CharacterType>
ALWAYS_INLINE bool URLParser::isDoubleDotPathSegment(CodePointIterator<CharacterType> c)
{
    if (c.atEnd())
        return false;
    if (*c == '.') {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return isSingleDotPathSegment(c);
    }
    if (*c != '%')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd() || *c != dotASCIICode[0])
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(c);
    if (c.atEnd())
        return false;
    if (toASCIILower(*c) == dotASCIICode[1]) {
        advance<CharacterType, ReportSyntaxViolation::No>(c);
        return isSingleDotPathSegment(c);
    }
    return false;
}

template<typename CharacterType>
void URLParser::consumeSingleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    if (*c == '.') {
        advance(c);
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                advance(c);
        }
    } else {
        advance(c);
        advance(c);
        advance(c);
        if (!c.atEnd()) {
            if (*c == '/' || *c == '\\')
                advance(c);
        }
    }
}

template<typename CharacterType>
void URLParser::consumeDoubleDotPathSegment(CodePointIterator<CharacterType>& c)
{
    if (*c == '.')
        advance(c);
    else {
        advance(c);
        advance(c);
        advance(c);
    }
    consumeSingleDotPathSegment(c);
}

bool URLParser::shouldPopPath(unsigned newPathAfterLastSlash)
{
    if (!m_urlIsFile)
        return true;

    CodePointIterator<LChar> componentToPop(&m_asciiBuffer[newPathAfterLastSlash], &m_asciiBuffer[0] + m_url.m_pathAfterLastSlash);
    if (newPathAfterLastSlash == m_url.m_hostEnd + m_url.m_portLength + 1 && isWindowsDriveLetter(componentToPop))
        return false;
    return true;
}

void URLParser::popPath()
{
    if (m_url.m_pathAfterLastSlash > m_url.m_hostEnd + m_url.m_portLength + 1) {
        auto newPathAfterLastSlash = m_url.m_pathAfterLastSlash - 1;
        if (m_asciiBuffer[newPathAfterLastSlash] == '/')
            newPathAfterLastSlash--;
        while (newPathAfterLastSlash > m_url.m_hostEnd + m_url.m_portLength && m_asciiBuffer[newPathAfterLastSlash] != '/')
            newPathAfterLastSlash--;
        newPathAfterLastSlash++;
        if (shouldPopPath(newPathAfterLastSlash))
            m_url.m_pathAfterLastSlash = newPathAfterLastSlash;
    }
    m_asciiBuffer.resize(m_url.m_pathAfterLastSlash);
}

template<typename CharacterType>
void URLParser::syntaxViolation(const CodePointIterator<CharacterType>& iterator)
{
    if (m_didSeeSyntaxViolation)
        return;
    m_didSeeSyntaxViolation = true;

    size_t codeUnitsToCopy = iterator.codeUnitsSince(reinterpret_cast<const CharacterType*>(m_inputBegin));
    RELEASE_ASSERT(codeUnitsToCopy <= m_inputString.length());
    m_asciiBuffer.reserveCapacity(m_inputString.length());
    for (size_t i = 0; i < codeUnitsToCopy; ++i) {
        m_asciiBuffer.uncheckedAppend(m_inputString[i]);
    }
}

void URLParser::failure()
{
    m_url.invalidate();
    m_url.m_string = m_inputString;
}

template<typename CharacterType>
bool URLParser::checkLocalhostCodePoint(CodePointIterator<CharacterType>& iterator, UChar32 codePoint)
{
    if (iterator.atEnd() || toASCIILower(*iterator) != codePoint)
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    return true;
}

template<typename CharacterType>
bool URLParser::isAtLocalhost(CodePointIterator<CharacterType> iterator)
{
    if (!checkLocalhostCodePoint(iterator, 'l'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'o'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'c'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'a'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'l'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'h'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 'o'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 's'))
        return false;
    if (!checkLocalhostCodePoint(iterator, 't'))
        return false;
    return iterator.atEnd();
}

bool URLParser::isLocalhost(StringView view)
{
    if (view.is8Bit())
        return isAtLocalhost(CodePointIterator<LChar>(view.characters8(), view.characters8() + view.length()));
    return isAtLocalhost(CodePointIterator<UChar>(view.characters16(), view.characters16() + view.length()));
}

ALWAYS_INLINE StringView URLParser::parsedDataView(size_t start, size_t length)
{
    if (UNLIKELY(m_didSeeSyntaxViolation)) {
        return StringView(m_asciiBuffer.data() + start, length);
    }
    return StringView(m_inputString).substring(start, length);
}

ALWAYS_INLINE UChar URLParser::parsedDataView(size_t position)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        return m_asciiBuffer[position];
    return m_inputString[position];
}

template<typename CharacterType>
ALWAYS_INLINE size_t URLParser::currentPosition(const CodePointIterator<CharacterType>& iterator)
{
    if (UNLIKELY(m_didSeeSyntaxViolation))
        return m_asciiBuffer.size();

    return iterator.codeUnitsSince(reinterpret_cast<const CharacterType*>(m_inputBegin));
}

URLParser::URLParser(const String& input, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
    : m_inputString(input)
{
    if (input.isNull()) {
        if (base.isValid() && !base.m_cannotBeABaseURL) {
            m_url = base;
            m_url.removeFragmentIdentifier();
        }
        return;
    }

    if (input.is8Bit()) {
        m_inputBegin = input.characters8();
        parse(input.characters8(), input.length(), base, nonUTF8QueryEncoding);
    } else {
        m_inputBegin = input.characters16();
        parse(input.characters16(), input.length(), base, nonUTF8QueryEncoding);
    }

    if (UNLIKELY(needsNonSpecialDotSlash()))
        addNonSpecialDotSlash();
}

template<typename CharacterType>
void URLParser::parse(const CharacterType* input, const unsigned length, const URL& base, const URLTextEncoding* nonUTF8QueryEncoding)
{
    URL_PARSER_LOG("Parsing URL <%s> base <%s>", String(input, length).utf8().data(), base.string().utf8().data());
    m_url = { };

    Vector<UChar> queryBuffer;

    unsigned endIndex = length;
    while (UNLIKELY(endIndex && isC0ControlOrSpace(input[endIndex - 1]))) {
        syntaxViolation(CodePointIterator<CharacterType>(input, input));
        endIndex--;
    }
    CodePointIterator<CharacterType> c(input, input + endIndex);
    CodePointIterator<CharacterType> authorityOrHostBegin;
    CodePointIterator<CharacterType> queryBegin;
    while (UNLIKELY(!c.atEnd() && isC0ControlOrSpace(*c))) {
        syntaxViolation(c);
        ++c;
    }
    auto beginAfterControlAndSpace = c;

    enum class State : uint8_t {
        SchemeStart,
        Scheme,
        NoScheme,
        SpecialRelativeOrAuthority,
        PathOrAuthority,
        Relative,
        RelativeSlash,
        SpecialAuthoritySlashes,
        SpecialAuthorityIgnoreSlashes,
        AuthorityOrHost,
        Host,
        File,
        FileSlash,
        FileHost,
        PathStart,
        Path,
        CannotBeABaseURLPath,
        UTF8Query,
        NonUTF8Query,
        Fragment,
    };

#define LOG_STATE(x) URL_PARSER_LOG("State %s, code point %c, parsed data <%s> size %zu", x, *c, parsedDataView(0, currentPosition(c)).utf8().data(), currentPosition(c))
#define LOG_FINAL_STATE(x) URL_PARSER_LOG("Final State: %s", x)

    State state = State::SchemeStart;
    while (!c.atEnd()) {
        if (UNLIKELY(isTabOrNewline(*c))) {
            syntaxViolation(c);
            ++c;
            continue;
        }

        switch (state) {
        case State::SchemeStart:
            LOG_STATE("SchemeStart");
            if (isASCIIAlpha(*c)) {
                if (UNLIKELY(isASCIIUpper(*c)))
                    syntaxViolation(c);
                appendToASCIIBuffer(toASCIILower(*c));
                advance(c);
                if (c.atEnd()) {
                    m_asciiBuffer.clear();
                    state = State::NoScheme;
                    c = beginAfterControlAndSpace;
                    break;
                }
                state = State::Scheme;
            } else
                state = State::NoScheme;
            break;
        case State::Scheme:
            LOG_STATE("Scheme");
            if (isValidSchemeCharacter(*c)) {
                if (UNLIKELY(isASCIIUpper(*c)))
                    syntaxViolation(c);
                appendToASCIIBuffer(toASCIILower(*c));
            } else if (*c == ':') {
                unsigned schemeEnd = currentPosition(c);
                if (schemeEnd > URL::maxSchemeLength) {
                    failure();
                    return;
                }
                m_url.m_schemeEnd = schemeEnd;
                StringView urlScheme = parsedDataView(0, m_url.m_schemeEnd);
                appendToASCIIBuffer(':');
                switch (scheme(urlScheme)) {
                case Scheme::File:
                    m_urlIsSpecial = true;
                    m_urlIsFile = true;
                    state = State::File;
                    ++c;
                    break;
                case Scheme::WS:
                case Scheme::WSS:
                    nonUTF8QueryEncoding = nullptr;
                    m_urlIsSpecial = true;
                    if (base.protocolIs(urlScheme))
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                    ++c;
                    break;
                case Scheme::HTTP:
                case Scheme::HTTPS:
                    m_url.m_protocolIsInHTTPFamily = true;
                    FALLTHROUGH;
                case Scheme::FTP:
                    m_urlIsSpecial = true;
                    if (base.protocolIs(urlScheme))
                        state = State::SpecialRelativeOrAuthority;
                    else
                        state = State::SpecialAuthoritySlashes;
                    ++c;
                    break;
                case Scheme::NonSpecial:
                    nonUTF8QueryEncoding = nullptr;
                    auto maybeSlash = c;
                    advance(maybeSlash);
                    if (!maybeSlash.atEnd() && *maybeSlash == '/') {
                        appendToASCIIBuffer('/');
                        c = maybeSlash;
                        state = State::PathOrAuthority;
                        ++c;
                        m_url.m_userStart = currentPosition(c);
                    } else {
                        ++c;
                        m_url.m_userStart = currentPosition(c);
                        m_url.m_userEnd = m_url.m_userStart;
                        m_url.m_passwordEnd = m_url.m_userStart;
                        m_url.m_hostEnd = m_url.m_userStart;
                        m_url.m_portLength = 0;
                        m_url.m_pathAfterLastSlash = m_url.m_userStart;
                        m_url.m_cannotBeABaseURL = true;
                        state = State::CannotBeABaseURLPath;
                    }
                    break;
                }
                break;
            } else {
                m_asciiBuffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
                break;
            }
            advance(c);
            if (c.atEnd()) {
                m_asciiBuffer.clear();
                state = State::NoScheme;
                c = beginAfterControlAndSpace;
            }
            break;
        case State::NoScheme:
            LOG_STATE("NoScheme");
            if (!base.isValid() || (base.m_cannotBeABaseURL && *c != '#')) {
                failure();
                return;
            }
            if (base.m_cannotBeABaseURL && *c == '#') {
                copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                state = State::Fragment;
                appendToASCIIBuffer('#');
                ++c;
                break;
            }
            if (!base.protocolIs("file")) {
                state = State::Relative;
                break;
            }
            state = State::File;
            break;
        case State::SpecialRelativeOrAuthority:
            LOG_STATE("SpecialRelativeOrAuthority");
            if (*c == '/') {
                appendToASCIIBuffer('/');
                advance(c);
                if (c.atEnd()) {
                    failure();
                    return;
                }
                if (*c == '/') {
                    appendToASCIIBuffer('/');
                    state = State::SpecialAuthorityIgnoreSlashes;
                    ++c;
                } else
                    state = State::RelativeSlash;
            } else
                state = State::Relative;
            break;
        case State::PathOrAuthority:
            LOG_STATE("PathOrAuthority");
            if (*c == '/') {
                appendToASCIIBuffer('/');
                state = State::AuthorityOrHost;
                advance(c);
                m_url.m_userStart = currentPosition(c);
                authorityOrHostBegin = c;
            } else {
                m_url.m_userStart = currentPosition(c) - 1;
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
                m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                state = State::Path;
            }
            break;
        case State::Relative:
            LOG_STATE("Relative");
            switch (*c) {
            case '/':
            case '\\':
                state = State::RelativeSlash;
                ++c;
                break;
            case '?':
                copyURLPartsUntil(base, URLPart::PathEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            case '#':
                copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('#');
                state = State::Fragment;
                ++c;
                break;
            default:
                copyURLPartsUntil(base, URLPart::PathAfterLastSlash, c, nonUTF8QueryEncoding);
                if ((currentPosition(c) && parsedDataView(currentPosition(c) - 1) != '/')
                    || (base.host().isEmpty() && base.path().isEmpty())) {
                    appendToASCIIBuffer('/');
                    m_url.m_pathAfterLastSlash = currentPosition(c);
                }
                state = State::Path;
                break;
            }
            break;
        case State::RelativeSlash:
            LOG_STATE("RelativeSlash");
            if (*c == '/' || *c == '\\') {
                ++c;
                copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer("://", 3);
                if (m_urlIsSpecial)
                    state = State::SpecialAuthorityIgnoreSlashes;
                else {
                    m_url.m_userStart = currentPosition(c);
                    state = State::AuthorityOrHost;
                    authorityOrHostBegin = c;
                }
            } else {
                copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                appendToASCIIBuffer('/');
                m_url.m_pathAfterLastSlash = base.m_hostEnd + base.m_portLength + 1;
                state = State::Path;
            }
            break;
        case State::SpecialAuthoritySlashes:
            LOG_STATE("SpecialAuthoritySlashes");
            if (LIKELY(*c == '/' || *c == '\\')) {
                if (UNLIKELY(*c == '\\'))
                    syntaxViolation(c);
                appendToASCIIBuffer('/');
                advance(c);
                if (LIKELY(!c.atEnd() && (*c == '/' || *c == '\\'))) {
                    if (UNLIKELY(*c == '\\'))
                        syntaxViolation(c);
                    ++c;
                    appendToASCIIBuffer('/');
                } else {
                    syntaxViolation(c);
                    appendToASCIIBuffer('/');
                }
            } else {
                syntaxViolation(c);
                appendToASCIIBuffer("//", 2);
            }
            state = State::SpecialAuthorityIgnoreSlashes;
            break;
        case State::SpecialAuthorityIgnoreSlashes:
            LOG_STATE("SpecialAuthorityIgnoreSlashes");
            if (*c == '/' || *c == '\\') {
                syntaxViolation(c);
                ++c;
            } else {
                m_url.m_userStart = currentPosition(c);
                state = State::AuthorityOrHost;
                authorityOrHostBegin = c;
            }
            break;
        case State::AuthorityOrHost:
            do {
                LOG_STATE("AuthorityOrHost");
                if (*c == '@') {
                    auto lastAt = c;
                    auto findLastAt = c;
                    while (!findLastAt.atEnd()) {
                        URL_PARSER_LOG("Finding last @: %c", *findLastAt);
                        if (*findLastAt == '@')
                            lastAt = findLastAt;
                        bool isSlash = *findLastAt == '/' || (m_urlIsSpecial && *findLastAt == '\\');
                        if (isSlash || *findLastAt == '?' || *findLastAt == '#')
                            break;
                        ++findLastAt;
                    }
                    parseAuthority(CodePointIterator<CharacterType>(authorityOrHostBegin, lastAt));
                    c = lastAt;
                    advance(c);
                    authorityOrHostBegin = c;
                    state = State::Host;
                    m_hostHasPercentOrNonASCII = false;
                    break;
                }
                bool isSlash = *c == '/' || (m_urlIsSpecial && *c == '\\');
                if (isSlash || *c == '?' || *c == '#') {
                    auto iterator = CodePointIterator<CharacterType>(authorityOrHostBegin, c);
                    if (iterator.atEnd()) {
                        if (m_urlIsSpecial)
                            return failure();
                        m_url.m_userEnd = currentPosition(c);
                        m_url.m_passwordEnd = m_url.m_userEnd;
                        m_url.m_hostEnd = m_url.m_userEnd;
                        m_url.m_portLength = 0;
                        m_url.m_pathAfterLastSlash = m_url.m_userEnd;
                    } else {
                        m_url.m_userEnd = currentPosition(authorityOrHostBegin);
                        m_url.m_passwordEnd = m_url.m_userEnd;
                        if (!parseHostAndPort(iterator)) {
                            failure();
                            return;
                        }
                        if (UNLIKELY(!isSlash)) {
                            if (m_urlIsSpecial) {
                                syntaxViolation(c);
                                appendToASCIIBuffer('/');
                            }
                            m_url.m_pathAfterLastSlash = currentPosition(c);
                        }
                    }
                    state = State::Path;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::Host:
            do {
                LOG_STATE("Host");
                if (*c == '/' || *c == '?' || *c == '#') {
                    if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
                        failure();
                        return;
                    }
                    if (*c == '?' || *c == '#') {
                        syntaxViolation(c);
                        appendToASCIIBuffer('/');
                        m_url.m_pathAfterLastSlash = currentPosition(c);
                    }
                    state = State::Path;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::File:
            LOG_STATE("File");
            switch (*c) {
            case '\\':
                syntaxViolation(c);
                FALLTHROUGH;
            case '/':
                appendToASCIIBuffer('/');
                state = State::FileSlash;
                ++c;
                break;
            case '?':
                syntaxViolation(c);
                if (base.isValid() && base.protocolIs("file")) {
                    copyURLPartsUntil(base, URLPart::PathEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('?');
                    ++c;
                } else {
                    appendToASCIIBuffer("///?", 4);
                    ++c;
                    m_url.m_userStart = currentPosition(c) - 2;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                    m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                }
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            case '#':
                syntaxViolation(c);
                if (base.isValid() && base.protocolIs("file")) {
                    copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('#');
                } else {
                    appendToASCIIBuffer("///#", 4);
                    m_url.m_userStart = currentPosition(c) - 2;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                    m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
                    m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                    m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                }
                state = State::Fragment;
                ++c;
                break;
            default:
                syntaxViolation(c);
                if (base.isValid() && base.protocolIs("file") && shouldCopyFileURL(c))
                    copyURLPartsUntil(base, URLPart::PathAfterLastSlash, c, nonUTF8QueryEncoding);
                else {
                    bool copiedHost = false;
                    if (base.isValid() && base.protocolIs("file")) {
                        if (base.host().isEmpty()) {
                            copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                            appendToASCIIBuffer(":///", 4);
                        } else {
                            copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                            appendToASCIIBuffer('/');
                            copiedHost = true;
                        }
                    } else
                        appendToASCIIBuffer("///", 3);
                    if (!copiedHost) {
                        m_url.m_userStart = currentPosition(c) - 1;
                        m_url.m_userEnd = m_url.m_userStart;
                        m_url.m_passwordEnd = m_url.m_userStart;
                        m_url.m_hostEnd = m_url.m_userStart;
                        m_url.m_portLength = 0;
                    }
                    m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
                }
                if (isWindowsDriveLetter(c))
                    appendWindowsDriveLetter(c);
                state = State::Path;
                break;
            }
            break;
        case State::FileSlash:
            LOG_STATE("FileSlash");
            if (LIKELY(*c == '/' || *c == '\\')) {
                if (UNLIKELY(*c == '\\'))
                    syntaxViolation(c);
                if (base.isValid() && base.protocolIs("file")) {
                    copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer(":/", 2);
                }
                appendToASCIIBuffer('/');
                advance(c);
                m_url.m_userStart = currentPosition(c);
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
                authorityOrHostBegin = c;
                state = State::FileHost;
                break;
            }
            {
                bool copiedHost = false;
                if (base.isValid() && base.protocolIs("file")) {
                    if (base.host().isEmpty()) {
                        copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                        appendToASCIIBuffer(":///", 4);
                    } else {
                        copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                        appendToASCIIBuffer('/');
                        copiedHost = true;
                    }
                } else {
                    syntaxViolation(c);
                    appendToASCIIBuffer("//", 2);
                }
                if (!copiedHost) {
                    m_url.m_userStart = currentPosition(c) - 1;
                    m_url.m_userEnd = m_url.m_userStart;
                    m_url.m_passwordEnd = m_url.m_userStart;
                    m_url.m_hostEnd = m_url.m_userStart;
                    m_url.m_portLength = 0;
                }
            }
            if (isWindowsDriveLetter(c)) {
                appendWindowsDriveLetter(c);
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
            } else if (copyBaseWindowsDriveLetter(base)) {
                appendToASCIIBuffer('/');
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 4;
            } else
                m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
            state = State::Path;
            break;
        case State::FileHost:
            do {
                LOG_STATE("FileHost");
                if (isSlashQuestionOrHash(*c)) {
                    bool windowsQuirk = takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>(authorityOrHostBegin, c))
                        && isWindowsDriveLetter(authorityOrHostBegin);
                    if (windowsQuirk) {
                        syntaxViolation(authorityOrHostBegin);
                        appendToASCIIBuffer('/');
                        appendWindowsDriveLetter(authorityOrHostBegin);
                    }
                    if (windowsQuirk || authorityOrHostBegin == c) {
                        if (UNLIKELY(*c == '?')) {
                            syntaxViolation(c);
                            appendToASCIIBuffer("/?", 2);
                            ++c;
                            if (nonUTF8QueryEncoding) {
                                queryBegin = c;
                                state = State::NonUTF8Query;
                            } else
                                state = State::UTF8Query;
                            m_url.m_pathAfterLastSlash = currentPosition(c) - 1;
                            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                            break;
                        }
                        if (UNLIKELY(*c == '#')) {
                            syntaxViolation(c);
                            appendToASCIIBuffer("/#", 2);
                            ++c;
                            m_url.m_pathAfterLastSlash = currentPosition(c) - 1;
                            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
                            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
                            state = State::Fragment;
                            break;
                        }
                        state = State::Path;
                        break;
                    }
                    if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
                        failure();
                        return;
                    }
                    if (UNLIKELY(isLocalhost(parsedDataView(m_url.m_passwordEnd, currentPosition(c) - m_url.m_passwordEnd)))) {
                        syntaxViolation(c);
                        m_asciiBuffer.shrink(m_url.m_passwordEnd);
                        m_url.m_hostEnd = currentPosition(c);
                        m_url.m_portLength = 0;
                    }

                    state = State::PathStart;
                    break;
                }
                if (isPercentOrNonASCII(*c))
                    m_hostHasPercentOrNonASCII = true;
                ++c;
            } while (!c.atEnd());
            break;
        case State::PathStart:
            LOG_STATE("PathStart");
            if (*c != '/' && *c != '\\') {
                syntaxViolation(c);
                appendToASCIIBuffer('/');
            }
            m_url.m_pathAfterLastSlash = currentPosition(c);
            state = State::Path;
            break;
        case State::Path:
            LOG_STATE("Path");
            if (*c == '/' || (m_urlIsSpecial && *c == '\\')) {
                if (UNLIKELY(m_urlIsSpecial && *c == '\\'))
                    syntaxViolation(c);
                appendToASCIIBuffer('/');
                ++c;
                m_url.m_pathAfterLastSlash = currentPosition(c);
                break;
            }
            if (UNLIKELY(currentPosition(c) && parsedDataView(currentPosition(c) - 1) == '/')) {
                if (UNLIKELY(isDoubleDotPathSegment(c))) {
                    syntaxViolation(c);
                    consumeDoubleDotPathSegment(c);
                    popPath();
                    break;
                }
                if (UNLIKELY(isSingleDotPathSegment(c))) {
                    syntaxViolation(c);
                    consumeSingleDotPathSegment(c);
                    break;
                }
            }
            if (*c == '?') {
                m_url.m_pathEnd = currentPosition(c);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
                break;
            }
            if (*c == '#') {
                m_url.m_pathEnd = currentPosition(c);
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
                break;
            }
            utf8PercentEncode<isInDefaultEncodeSet>(c);
            ++c;
            break;
        case State::CannotBeABaseURLPath:
            LOG_STATE("CannotBeABaseURLPath");
            if (*c == '?') {
                m_url.m_pathEnd = currentPosition(c);
                appendToASCIIBuffer('?');
                ++c;
                if (nonUTF8QueryEncoding) {
                    queryBegin = c;
                    state = State::NonUTF8Query;
                } else
                    state = State::UTF8Query;
            } else if (*c == '#') {
                m_url.m_pathEnd = currentPosition(c);
                m_url.m_queryEnd = m_url.m_pathEnd;
                state = State::Fragment;
            } else if (*c == '/') {
                appendToASCIIBuffer('/');
                ++c;
                m_url.m_pathAfterLastSlash = currentPosition(c);
            } else {
                utf8PercentEncode<isInSimpleEncodeSet>(c);
                ++c;
            }
            break;
        case State::UTF8Query:
            LOG_STATE("UTF8Query");
            if (*c == '#') {
                m_url.m_queryEnd = currentPosition(c);
                state = State::Fragment;
                break;
            }
            utf8QueryEncode(c);
            ++c;
            break;
        case State::NonUTF8Query:
            do {
                LOG_STATE("NonUTF8Query");
                if (*c == '#') {
                    encodeNonUTF8Query(queryBuffer, *nonUTF8QueryEncoding, CodePointIterator<CharacterType>(queryBegin, c));
                    m_url.m_queryEnd = currentPosition(c);
                    state = State::Fragment;
                    break;
                }
                appendCodePoint(queryBuffer, *c);
                advance(c, queryBegin);
            } while (!c.atEnd());
            break;
        case State::Fragment:
            URL_PARSER_LOG("State Fragment");
            utf8PercentEncode<isInFragmentEncodeSet>(c);
            ++c;
            break;
        }
    }

    switch (state) {
    case State::SchemeStart:
        LOG_FINAL_STATE("SchemeStart");
        if (!currentPosition(c) && base.isValid() && !base.m_cannotBeABaseURL) {
            m_url = base;
            m_url.removeFragmentIdentifier();
            return;
        }
        failure();
        return;
    case State::Scheme:
        LOG_FINAL_STATE("Scheme");
        failure();
        return;
    case State::NoScheme:
        LOG_FINAL_STATE("NoScheme");
        RELEASE_ASSERT_NOT_REACHED();
    case State::SpecialRelativeOrAuthority:
        LOG_FINAL_STATE("SpecialRelativeOrAuthority");
        copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
        break;
    case State::PathOrAuthority:
        LOG_FINAL_STATE("PathOrAuthority");
        m_url.m_userStart--;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::Relative:
        LOG_FINAL_STATE("Relative");
        RELEASE_ASSERT_NOT_REACHED();
    case State::RelativeSlash:
        LOG_FINAL_STATE("RelativeSlash");
        copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
        appendToASCIIBuffer('/');
        m_url.m_pathAfterLastSlash = m_url.m_hostEnd + m_url.m_portLength + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::SpecialAuthoritySlashes:
        LOG_FINAL_STATE("SpecialAuthoritySlashes");
        failure();
        return;
    case State::SpecialAuthorityIgnoreSlashes:
        LOG_FINAL_STATE("SpecialAuthorityIgnoreSlashes");
        failure();
        return;
    case State::AuthorityOrHost:
        LOG_FINAL_STATE("AuthorityOrHost");
        m_url.m_userEnd = currentPosition(authorityOrHostBegin);
        m_url.m_passwordEnd = m_url.m_userEnd;
        if (authorityOrHostBegin.atEnd()) {
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portLength = 0;
            m_url.m_pathEnd = m_url.m_userStart;
        } else if (!parseHostAndPort(authorityOrHostBegin)) {
            failure();
            return;
        } else {
            if (m_urlIsSpecial) {
                syntaxViolation(c);
                appendToASCIIBuffer('/');
                m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength + 1;
            } else
                m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength;
        }
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::Host:
        LOG_FINAL_STATE("Host");
        if (!parseHostAndPort(authorityOrHostBegin)) {
            failure();
            return;
        }
        if (m_urlIsSpecial) {
            syntaxViolation(c);
            appendToASCIIBuffer('/');
            m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength + 1;
        } else
            m_url.m_pathEnd = m_url.m_hostEnd + m_url.m_portLength;
        m_url.m_pathAfterLastSlash = m_url.m_pathEnd;
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::File:
        LOG_FINAL_STATE("File");
        if (base.isValid() && base.protocolIs("file")) {
            copyURLPartsUntil(base, URLPart::QueryEnd, c, nonUTF8QueryEncoding);
            break;
        }
        syntaxViolation(c);
        appendToASCIIBuffer("///", 3);
        m_url.m_userStart = currentPosition(c) - 1;
        m_url.m_userEnd = m_url.m_userStart;
        m_url.m_passwordEnd = m_url.m_userStart;
        m_url.m_hostEnd = m_url.m_userStart;
        m_url.m_portLength = 0;
        m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileSlash:
        LOG_FINAL_STATE("FileSlash");
        syntaxViolation(c);
        {
            bool copiedHost = false;
            if (base.isValid() && base.protocolIs("file")) {
                if (base.host().isEmpty()) {
                    copyURLPartsUntil(base, URLPart::SchemeEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer(":/", 2);
                } else {
                    copyURLPartsUntil(base, URLPart::PortEnd, c, nonUTF8QueryEncoding);
                    appendToASCIIBuffer('/');
                    copiedHost = true;
                }
            }
            if (!copiedHost) {
                m_url.m_userStart = currentPosition(c) + 1;
                appendToASCIIBuffer("//", 2);
                m_url.m_userEnd = m_url.m_userStart;
                m_url.m_passwordEnd = m_url.m_userStart;
                m_url.m_hostEnd = m_url.m_userStart;
                m_url.m_portLength = 0;
            }
        }
        if (copyBaseWindowsDriveLetter(base)) {
            appendToASCIIBuffer('/');
            m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 4;
        } else
            m_url.m_pathAfterLastSlash = m_url.m_hostEnd + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::FileHost:
        LOG_FINAL_STATE("FileHost");
        if (takesTwoAdvancesUntilEnd(CodePointIterator<CharacterType>(authorityOrHostBegin, c))
            && isWindowsDriveLetter(authorityOrHostBegin)) {
            syntaxViolation(authorityOrHostBegin);
            appendToASCIIBuffer('/');
            appendWindowsDriveLetter(authorityOrHostBegin);
            m_url.m_pathAfterLastSlash = currentPosition(c);
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            break;
        }

        if (authorityOrHostBegin == c) {
            syntaxViolation(c);
            appendToASCIIBuffer('/');
            m_url.m_userStart = currentPosition(c) - 1;
            m_url.m_userEnd = m_url.m_userStart;
            m_url.m_passwordEnd = m_url.m_userStart;
            m_url.m_hostEnd = m_url.m_userStart;
            m_url.m_portLength = 0;
            m_url.m_pathAfterLastSlash = m_url.m_userStart + 1;
            m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
            m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
            break;
        }

        if (!parseHostAndPort(CodePointIterator<CharacterType>(authorityOrHostBegin, c))) {
            failure();
            return;
        }

        syntaxViolation(c);
        if (isLocalhost(parsedDataView(m_url.m_passwordEnd, currentPosition(c) - m_url.m_passwordEnd))) {
            m_asciiBuffer.shrink(m_url.m_passwordEnd);
            m_url.m_hostEnd = currentPosition(c);
            m_url.m_portLength = 0;
        }
        appendToASCIIBuffer('/');
        m_url.m_pathAfterLastSlash = m_url.m_hostEnd + m_url.m_portLength + 1;
        m_url.m_pathEnd = m_url.m_pathAfterLastSlash;
        m_url.m_queryEnd = m_url.m_pathAfterLastSlash;
        break;
    case State::PathStart:
        LOG_FINAL_STATE("PathStart");
        RELEASE_ASSERT_NOT_REACHED();
    case State::Path:
        LOG_FINAL_STATE("Path");
        m_url.m_pathEnd = currentPosition(c);
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::CannotBeABaseURLPath:
        LOG_FINAL_STATE("CannotBeABaseURLPath");
        m_url.m_pathEnd = currentPosition(c);
        m_url.m_queryEnd = m_url.m_pathEnd;
        break;
    case State::UTF8Query:
        LOG_FINAL_STATE("UTF8Query");
        m_url.m_queryEnd = currentPosition(c);
        break;
    case State::NonUTF8Query:
        LOG_FINAL_STATE("NonUTF8Query");
        encodeNonUTF8Query(queryBuffer, *nonUTF8QueryEncoding, CodePointIterator<CharacterType>(queryBegin, c));
        m_url.m_queryEnd = currentPosition(c);
        break;
    case State::Fragment:
        LOG_FINAL_STATE("Fragment");
        break;
    }

    if (LIKELY(!m_didSeeSyntaxViolation)) {
        m_url.m_string = m_inputString;
    } else
        m_url.m_string = String::adopt(WTFMove(m_asciiBuffer));
    m_url.m_isValid = true;
    URL_PARSER_LOG("Parsed URL <%s>\n\n", m_url.m_string.utf8().data());
}

template<typename CharacterType>
void URLParser::parseAuthority(CodePointIterator<CharacterType> iterator)
{
    if (UNLIKELY(iterator.atEnd())) {
        syntaxViolation(iterator);
        m_url.m_userEnd = currentPosition(iterator);
        m_url.m_passwordEnd = m_url.m_userEnd;
        return;
    }
    for (; !iterator.atEnd(); advance(iterator)) {
        if (*iterator == ':') {
            m_url.m_userEnd = currentPosition(iterator);
            auto iteratorAtColon = iterator;
            ++iterator;
            bool tabOrNewlineAfterColon = false;
            while (UNLIKELY(!iterator.atEnd() && isTabOrNewline(*iterator))) {
                tabOrNewlineAfterColon = true;
                ++iterator;
            }
            if (UNLIKELY(iterator.atEnd())) {
                syntaxViolation(iteratorAtColon);
                m_url.m_passwordEnd = m_url.m_userEnd;
                if (m_url.m_userEnd > m_url.m_userStart)
                    appendToASCIIBuffer('@');
                return;
            }
            if (tabOrNewlineAfterColon)
                syntaxViolation(iteratorAtColon);
            appendToASCIIBuffer(':');
            break;
        }
        utf8PercentEncode<::NKWebKit::UrlParserPort::isInUserInfoEncodeSet<UChar32>>(iterator);
    }
    for (; !iterator.atEnd(); advance(iterator))
        utf8PercentEncode<::NKWebKit::UrlParserPort::isInUserInfoEncodeSet<UChar32>>(iterator);
    m_url.m_passwordEnd = currentPosition(iterator);
    if (!m_url.m_userEnd)
        m_url.m_userEnd = m_url.m_passwordEnd;
    appendToASCIIBuffer('@');
}

template<typename UnsignedIntegerType>
void URLParser::appendNumberToASCIIBuffer(UnsignedIntegerType number)
{
    LChar buf[sizeof(UnsignedIntegerType) * 3 + 1];
    LChar* end = std::end(buf);
    LChar* p = end;
    do {
        *--p = (number % 10) + '0';
        number /= 10;
    } while (number);
    appendToASCIIBuffer(p, end - p);
}

void URLParser::serializeIPv4(IPv4Address address)
{
    appendNumberToASCIIBuffer<uint8_t>(address >> 24);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address >> 16);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address >> 8);
    appendToASCIIBuffer('.');
    appendNumberToASCIIBuffer<uint8_t>(address);
}

static size_t zeroSequenceLength(const std::array<uint16_t, 8>& address, size_t begin)
{
    size_t end = begin;
    for (; end < 8; end++) {
        if (address[end])
            break;
    }
    return end - begin;
}

static Optional<size_t> findLongestZeroSequence(const std::array<uint16_t, 8>& address)
{
    Optional<size_t> longest;
    size_t longestLength = 0;
    for (size_t i = 0; i < 8; i++) {
        size_t length = zeroSequenceLength(address, i);
        if (length) {
            if (length > 1 && (!longest || longestLength < length)) {
                longest = i;
                longestLength = length;
            }
            i += length;
        }
    }
    return longest;
}

void URLParser::serializeIPv6Piece(uint16_t piece)
{
    bool printed = false;
    if (auto nibble0 = piece >> 12) {
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble0));
        printed = true;
    }
    auto nibble1 = piece >> 8 & 0xF;
    if (printed || nibble1) {
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble1));
        printed = true;
    }
    auto nibble2 = piece >> 4 & 0xF;
    if (printed || nibble2)
        appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(nibble2));
    appendToASCIIBuffer(lowerNibbleToLowercaseASCIIHexDigit(piece & 0xF));
}

void URLParser::serializeIPv6(URLParser::IPv6Address address)
{
    appendToASCIIBuffer('[');
    auto compressPointer = findLongestZeroSequence(address);
    for (size_t piece = 0; piece < 8; piece++) {
        if (compressPointer && compressPointer.value() == piece) {
            if (piece)
                appendToASCIIBuffer(':');
            else
                appendToASCIIBuffer("::", 2);
            while (piece < 8 && !address[piece])
                piece++;
            if (piece == 8)
                break;
        }
        serializeIPv6Piece(address[piece]);
        if (piece < 7)
            appendToASCIIBuffer(':');
    }
    appendToASCIIBuffer(']');
}

enum class URLParser::IPv4PieceParsingError {
    Failure,
    Overflow,
};

template<typename CharacterType>
Expected<uint32_t, URLParser::IPv4PieceParsingError> URLParser::parseIPv4Piece(CodePointIterator<CharacterType>& iterator, bool& didSeeSyntaxViolation)
{
    enum class State : uint8_t {
        UnknownBase,
        Decimal,
        OctalOrHex,
        Octal,
        Hex,
    };
    State state = State::UnknownBase;
    Checked<uint32_t, RecordOverflow> value = 0;
    if (!iterator.atEnd() && *iterator == '.')
        return makeUnexpected(IPv4PieceParsingError::Failure);
    while (!iterator.atEnd()) {
        if (isTabOrNewline(*iterator)) {
            didSeeSyntaxViolation = true;
            ++iterator;
            continue;
        }
        if (*iterator == '.') {
            return value.unsafeGet();
        }
        switch (state) {
        case State::UnknownBase:
            if (UNLIKELY(*iterator == '0')) {
                ++iterator;
                state = State::OctalOrHex;
                break;
            }
            state = State::Decimal;
            break;
        case State::OctalOrHex:
            didSeeSyntaxViolation = true;
            if (*iterator == 'x' || *iterator == 'X') {
                ++iterator;
                state = State::Hex;
                break;
            }
            state = State::Octal;
            break;
        case State::Decimal:
            if (!isASCIIDigit(*iterator))
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 10;
            value += *iterator - '0';
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        case State::Octal:
            if (*iterator < '0' || *iterator > '7')
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 8;
            value += *iterator - '0';
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        case State::Hex:
            if (!isASCIIHexDigit(*iterator))
                return makeUnexpected(IPv4PieceParsingError::Failure);
            value *= 16;
            value += toASCIIHexValue(*iterator);
            if (UNLIKELY(value.hasOverflowed()))
                return makeUnexpected(IPv4PieceParsingError::Overflow);
            ++iterator;
            break;
        }
    }
    return value.unsafeGet();
}

ALWAYS_INLINE static uint64_t pow256(size_t exponent)
{
    RELEASE_ASSERT(exponent <= 4);
    uint64_t values[5] = {1, 256, 256 * 256, 256 * 256 * 256, 256ull * 256 * 256 * 256 };
    return values[exponent];
}

enum class URLParser::IPv4ParsingError {
    Failure,
    NotIPv4,
};

template<typename CharacterTypeForSyntaxViolation, typename CharacterType>
Expected<URLParser::IPv4Address, URLParser::IPv4ParsingError> URLParser::parseIPv4Host(const CodePointIterator<CharacterTypeForSyntaxViolation>& iteratorForSyntaxViolationPosition, CodePointIterator<CharacterType> iterator)
{
    Vector<Expected<uint32_t, URLParser::IPv4PieceParsingError>, 4> items;
    bool didSeeSyntaxViolation = false;
    if (!iterator.atEnd() && *iterator == '.')
        return makeUnexpected(IPv4ParsingError::NotIPv4);
    while (!iterator.atEnd()) {
        if (isTabOrNewline(*iterator)) {
            didSeeSyntaxViolation = true;
            ++iterator;
            continue;
        }
        if (items.size() >= 4)
            return makeUnexpected(IPv4ParsingError::NotIPv4);
        items.append(parseIPv4Piece(iterator, didSeeSyntaxViolation));
        if (!iterator.atEnd() && *iterator == '.') {
            ++iterator;
            if (iterator.atEnd())
                didSeeSyntaxViolation = true;
            else if (*iterator == '.')
                return makeUnexpected(IPv4ParsingError::NotIPv4);
        }
    }
    if (!iterator.atEnd() || !items.size() || items.size() > 4)
        return makeUnexpected(IPv4ParsingError::NotIPv4);
    for (const auto& item : items) {
        if (!item.has_value() && item.error() == IPv4PieceParsingError::Failure)
            return makeUnexpected(IPv4ParsingError::NotIPv4);
    }
    for (const auto& item : items) {
        if (!item.has_value() && item.error() == IPv4PieceParsingError::Overflow)
            return makeUnexpected(IPv4ParsingError::Failure);
    }
    if (items.size() > 1) {
        for (size_t i = 0; i < items.size() - 1; i++) {
            if (items[i].value() > 255)
                return makeUnexpected(IPv4ParsingError::Failure);
        }
    }
    if (items[items.size() - 1].value() >= pow256(5 - items.size()))
        return makeUnexpected(IPv4ParsingError::Failure);

    if (didSeeSyntaxViolation)
        syntaxViolation(iteratorForSyntaxViolationPosition);
    for (const auto& item : items) {
        if (item.value() > 255)
            syntaxViolation(iteratorForSyntaxViolationPosition);
    }

    if (UNLIKELY(items.size() != 4))
        syntaxViolation(iteratorForSyntaxViolationPosition);

    IPv4Address ipv4 = items.takeLast().value();
    for (size_t counter = 0; counter < items.size(); ++counter)
        ipv4 += items[counter].value() * pow256(3 - counter);
    return ipv4;
}

template<typename CharacterType>
Optional<uint32_t> URLParser::parseIPv4PieceInsideIPv6(CodePointIterator<CharacterType>& iterator)
{
    if (iterator.atEnd())
        return std::nullopt;
    uint32_t piece = 0;
    bool leadingZeros = false;
    size_t digitCount = 0;
    while (!iterator.atEnd()) {
        if (!isASCIIDigit(*iterator))
            return std::nullopt;
        ++digitCount;
        if (!piece && *iterator == '0') {
            if (leadingZeros)
                return std::nullopt;
            leadingZeros = true;
        }
        if (!piece && *iterator == '0')
            leadingZeros = true;
        piece = piece * 10 + *iterator - '0';
        if (piece > 255)
            return std::nullopt;
        advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        if (iterator.atEnd())
            break;
        if (*iterator == '.')
            break;
    }
    if (piece && leadingZeros)
        return std::nullopt;
    return piece;
}

template<typename CharacterType>
Optional<URLParser::IPv4Address> URLParser::parseIPv4AddressInsideIPv6(CodePointIterator<CharacterType> iterator)
{
    IPv4Address address = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (Optional<uint32_t> piece = parseIPv4PieceInsideIPv6(iterator))
            address = (address << 8) + piece.value();
        else
            return std::nullopt;
        if (i < 3) {
            if (iterator.atEnd())
                return std::nullopt;
            if (*iterator != '.')
                return std::nullopt;
            advance<CharacterType, ReportSyntaxViolation::No>(iterator);
        } else if (!iterator.atEnd())
            return std::nullopt;
    }
    return address;
}

template<typename CharacterType>
Optional<URLParser::IPv6Address> URLParser::parseIPv6Host(CodePointIterator<CharacterType> c)
{
    const auto hostBegin = c;
    advance(c, hostBegin);
    if (c.atEnd())
        return std::nullopt;

    IPv6Address address = {{0, 0, 0, 0, 0, 0, 0, 0}};
    size_t piecePointer = 0;
    Optional<size_t> compressPointer;
    bool previousValueWasZero = false;
    bool immediatelyAfterCompress = false;

    if (*c == ':') {
        advance(c, hostBegin);
        if (c.atEnd())
            return std::nullopt;
        if (*c != ':')
            return std::nullopt;
        advance(c, hostBegin);
        ++piecePointer;
        compressPointer = piecePointer;
        immediatelyAfterCompress = true;
    }

    while (!c.atEnd()) {
        if (piecePointer == 8)
            return std::nullopt;
        if (*c == ':') {
            if (compressPointer)
                return std::nullopt;
            advance(c, hostBegin);
            ++piecePointer;
            compressPointer = piecePointer;
            immediatelyAfterCompress = true;
            if (previousValueWasZero)
                syntaxViolation(hostBegin);
            continue;
        }
        if (piecePointer == 6 || (compressPointer && piecePointer < 6)) {
            if (Optional<IPv4Address> ipv4Address = parseIPv4AddressInsideIPv6(c)) {
                if (compressPointer && piecePointer == 5)
                    return std::nullopt;
                syntaxViolation(hostBegin);
                address[piecePointer++] = ipv4Address.value() >> 16;
                address[piecePointer++] = ipv4Address.value() & 0xFFFF;
                c = { };
                break;
            }
        }
        uint16_t value = 0;
        size_t length = 0;
        bool leadingZeros = false;
        for (; length < 4; length++) {
            if (c.atEnd())
                break;
            if (!isASCIIHexDigit(*c))
                break;
            if (isASCIIUpper(*c))
                syntaxViolation(hostBegin);
            if (*c == '0' && !length)
                leadingZeros = true;
            value = value * 0x10 + toASCIIHexValue(*c);
            advance(c, hostBegin);
        }

        previousValueWasZero = !value;
        if (UNLIKELY((value && leadingZeros) || (previousValueWasZero && (length > 1 || immediatelyAfterCompress))))
            syntaxViolation(hostBegin);

        address[piecePointer++] = value;
        if (c.atEnd())
            break;
        if (piecePointer == 8 || *c != ':')
            return std::nullopt;
        advance(c, hostBegin);
        if (c.atEnd())
            syntaxViolation(hostBegin);

        immediatelyAfterCompress = false;
    }

    if (!c.atEnd())
        return std::nullopt;

    if (compressPointer) {
        size_t swaps = piecePointer - compressPointer.value();
        piecePointer = 7;
        while (swaps)
            std::swap(address[piecePointer--], address[compressPointer.value() + swaps-- - 1]);
    } else if (piecePointer != 8)
        return std::nullopt;

    Optional<size_t> possibleCompressPointer = findLongestZeroSequence(address);
    if (possibleCompressPointer)
        possibleCompressPointer.value()++;
    if (UNLIKELY(compressPointer != possibleCompressPointer))
        syntaxViolation(hostBegin);

    return address;
}

template<typename CharacterType>
URLParser::LCharBuffer URLParser::percentDecode(const LChar* input, size_t length, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer output;
    output.reserveInitialCapacity(length);

    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.uncheckedAppend(byte);
        else if (length > 2 && i < length - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                syntaxViolation(iteratorForSyntaxViolationPosition);
                output.uncheckedAppend(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.uncheckedAppend(byte);
        } else
            output.uncheckedAppend(byte);
    }
    return output;
}

URLParser::LCharBuffer URLParser::percentDecode(const LChar* input, size_t length)
{
    LCharBuffer output;
    output.reserveInitialCapacity(length);

    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = input[i];
        if (byte != '%')
            output.uncheckedAppend(byte);
        else if (length > 2 && i < length - 2) {
            if (isASCIIHexDigit(input[i + 1]) && isASCIIHexDigit(input[i + 2])) {
                output.uncheckedAppend(toASCIIHexValue(input[i + 1], input[i + 2]));
                i += 2;
            } else
                output.uncheckedAppend(byte);
        } else
            output.uncheckedAppend(byte);
    }
    return output;
}

bool URLParser::needsNonSpecialDotSlash() const
{
    auto pathStart = m_url.m_hostEnd + m_url.m_portLength;
    return !m_urlIsSpecial
        && pathStart == m_url.m_schemeEnd + 1U
        && pathStart + 1 < m_url.m_string.length()
        && m_url.m_string[pathStart] == '/'
        && m_url.m_string[pathStart + 1] == '/';
}

void URLParser::addNonSpecialDotSlash()
{
    auto oldPathStart = m_url.m_hostEnd + m_url.m_portLength;
    auto& oldString = m_url.m_string;
    m_url.m_string = makeString(oldString.substring(0, oldPathStart + 1), "./", oldString.substring(oldPathStart + 1));
    m_url.m_pathAfterLastSlash += 2;
    m_url.m_pathEnd += 2;
    m_url.m_queryEnd += 2;
}

template<typename CharacterType> Optional<URLParser::LCharBuffer> URLParser::domainToASCII(StringImpl& domain, const CodePointIterator<CharacterType>& iteratorForSyntaxViolationPosition)
{
    LCharBuffer ascii;
    if (domain.isAllASCII() && !startsWithLettersIgnoringASCIICase(domain, "xn--")) {
        size_t length = domain.length();
        if (domain.is8Bit()) {
            const LChar* characters = domain.characters8();
            ascii.reserveInitialCapacity(length);
            for (size_t i = 0; i < length; ++i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                ascii.uncheckedAppend(toASCIILower(characters[i]));
            }
        } else {
            const UChar* characters = domain.characters16();
            ascii.reserveInitialCapacity(length);
            for (size_t i = 0; i < length; ++i) {
                if (UNLIKELY(isASCIIUpper(characters[i])))
                    syntaxViolation(iteratorForSyntaxViolationPosition);
                ascii.uncheckedAppend(toASCIILower(characters[i]));
            }
        }
        return ascii;
    }

    const size_t maxDomainLength = 64;
    UChar hostnameBuffer[maxDomainLength];
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    int32_t numCharactersConverted = uidna_nameToASCII(&internationalDomainNameTranscoder(), StringView(domain).upconvertedCharacters(), domain.length(), hostnameBuffer, maxDomainLength, &processingDetails, &error);

    if (U_SUCCESS(error) && !processingDetails.errors) {
        UNUSED_PARAM(numCharactersConverted);
        ascii.append(hostnameBuffer, numCharactersConverted);
        if (domain != StringView(ascii.data(), ascii.size()))
            syntaxViolation(iteratorForSyntaxViolationPosition);
        return ascii;
    }
    return std::nullopt;
}

bool URLParser::hasForbiddenHostCodePoint(const URLParser::LCharBuffer& asciiDomain)
{
    for (size_t i = 0; i < asciiDomain.size(); ++i) {
        if (isForbiddenHostCodePoint(asciiDomain[i]))
            return true;
    }
    return false;
}

template<typename CharacterType>
bool URLParser::parsePort(CodePointIterator<CharacterType>& iterator)
{
    if (UNLIKELY(m_urlIsFile))
        return false;

    auto colonIterator = iterator;
    advance(iterator, colonIterator);
    uint32_t port = 0;
    if (UNLIKELY(iterator.atEnd())) {
        unsigned portLength = currentPosition(colonIterator) - m_url.m_hostEnd;
        RELEASE_ASSERT(portLength <= URL::maxPortLength);
        m_url.m_portLength = portLength;
        syntaxViolation(colonIterator);
        return true;
    }
    size_t digitCount = 0;
    bool leadingZeros = false;
    for (; !iterator.atEnd(); ++iterator) {
        if (UNLIKELY(isTabOrNewline(*iterator))) {
            syntaxViolation(colonIterator);
            continue;
        }
        if (isASCIIDigit(*iterator)) {
            if (*iterator == '0' && !digitCount)
                leadingZeros = true;
            ++digitCount;
            port = port * 10 + *iterator - '0';
            if (port > std::numeric_limits<uint16_t>::max())
                return false;
        } else
            return false;
    }

    if (port && leadingZeros)
        syntaxViolation(colonIterator);

    if (!port && digitCount > 1)
        syntaxViolation(colonIterator);

    if (UNLIKELY(defaultPortForProtocol(parsedDataView(0, m_url.m_schemeEnd)) == static_cast<uint16_t>(port)))
        syntaxViolation(colonIterator);
    else {
        appendToASCIIBuffer(':');
        appendNumberToASCIIBuffer<uint16_t>(static_cast<uint16_t>(port));
    }

    unsigned portLength = currentPosition(iterator) - m_url.m_hostEnd;
    RELEASE_ASSERT(portLength <= URL::maxPortLength);
    m_url.m_portLength = portLength;
    return true;
}

template<typename CharacterType>
bool URLParser::startsWithXNDashDash(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd() || (*iterator != 'x' && *iterator != 'X'))
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd() || (*iterator != 'n' && *iterator != 'N'))
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd() || *iterator != '-')
        return false;
    advance<CharacterType, ReportSyntaxViolation::No>(iterator);
    if (iterator.atEnd() || *iterator != '-')
        return false;
    return true;
}

template<typename CharacterType>
bool URLParser::parseHostAndPort(CodePointIterator<CharacterType> iterator)
{
    if (iterator.atEnd())
        return false;
    if (*iterator == ':')
        return false;
    if (*iterator == '[') {
        auto ipv6End = iterator;
        while (!ipv6End.atEnd() && *ipv6End != ']')
            ++ipv6End;
        if (ipv6End.atEnd())
            return false;
        if (auto address = parseIPv6Host(CodePointIterator<CharacterType>(iterator, ipv6End))) {
            serializeIPv6(address.value());
            if (!ipv6End.atEnd()) {
                advance(ipv6End);
                m_url.m_hostEnd = currentPosition(ipv6End);
                if (!ipv6End.atEnd() && *ipv6End == ':')
                    return parsePort(ipv6End);
                m_url.m_portLength = 0;
                return ipv6End.atEnd();
            }
            m_url.m_hostEnd = currentPosition(ipv6End);
            return true;
        }
        return false;
    }

    if (!m_urlIsSpecial) {
        for (; !iterator.atEnd(); ++iterator) {
            if (UNLIKELY(isTabOrNewline(*iterator))) {
                syntaxViolation(iterator);
                continue;
            }
            if (*iterator == ':')
                break;
            if (UNLIKELY(isForbiddenHostCodePoint(*iterator) && *iterator != '%'))
                return false;
            utf8PercentEncode<isInSimpleEncodeSet>(iterator);
        }
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return true;
        }
        return parsePort(iterator);
    }

    if (LIKELY(!m_hostHasPercentOrNonASCII && !startsWithXNDashDash(iterator))) {
        auto hostIterator = iterator;
        for (; !iterator.atEnd(); ++iterator) {
            if (isTabOrNewline(*iterator))
                continue;
            if (*iterator == ':')
                break;
            if (isForbiddenHostCodePoint(*iterator))
                return false;
        }
        auto address = parseIPv4Host(hostIterator, CodePointIterator<CharacterType>(hostIterator, iterator));
        if (address) {
            serializeIPv4(address.value());
            m_url.m_hostEnd = currentPosition(iterator);
            if (iterator.atEnd()) {
                m_url.m_portLength = 0;
                return true;
            }
            return parsePort(iterator);
        }
        if (address.error() == IPv4ParsingError::Failure)
            return false;
        for (; hostIterator != iterator; ++hostIterator) {
            if (UNLIKELY(isTabOrNewline(*hostIterator))) {
                syntaxViolation(hostIterator);
                continue;
            }
            if (UNLIKELY(isASCIIUpper(*hostIterator)))
                syntaxViolation(hostIterator);
            appendToASCIIBuffer(toASCIILower(*hostIterator));
        }
        m_url.m_hostEnd = currentPosition(iterator);
        if (!hostIterator.atEnd())
            return parsePort(hostIterator);
        unsigned portLength = currentPosition(iterator) - m_url.m_hostEnd;
        RELEASE_ASSERT(portLength <= URL::maxPortLength);
        m_url.m_portLength = portLength;
        return true;
    }

    const auto hostBegin = iterator;

    LCharBuffer utf8Encoded;
    for (; !iterator.atEnd(); ++iterator) {
        if (UNLIKELY(isTabOrNewline(*iterator))) {
            syntaxViolation(hostBegin);
            continue;
        }
        if (*iterator == ':')
            break;
        if (UNLIKELY(!isASCII(*iterator)))
            syntaxViolation(hostBegin);

        uint8_t buffer[U8_MAX_LENGTH];
        int32_t offset = 0;
        UBool isError = false;
        U8_APPEND(buffer, offset, U8_MAX_LENGTH, *iterator, isError);
        if (isError)
            return false;
        utf8Encoded.append(buffer, offset);
    }
    LCharBuffer percentDecoded = percentDecode(utf8Encoded.data(), utf8Encoded.size(), hostBegin);
    String domain = String::fromUTF8(percentDecoded.data(), percentDecoded.size());
    if (domain.isNull())
        return false;
    if (domain != StringView(percentDecoded.data(), percentDecoded.size()))
        syntaxViolation(hostBegin);
    auto asciiDomain = domainToASCII(*domain.impl(), hostBegin);
    if (!asciiDomain || hasForbiddenHostCodePoint(asciiDomain.value()))
        return false;
    LCharBuffer& asciiDomainValue = asciiDomain.value();
    const LChar* asciiDomainCharacters = asciiDomainValue.data();

    auto address = parseIPv4Host(hostBegin, CodePointIterator<LChar>(asciiDomainValue.begin(), asciiDomainValue.end()));
    if (address) {
        serializeIPv4(address.value());
        m_url.m_hostEnd = currentPosition(iterator);
        if (iterator.atEnd()) {
            m_url.m_portLength = 0;
            return true;
        }
        return parsePort(iterator);
    }
    if (address.error() == IPv4ParsingError::Failure)
        return false;

    appendToASCIIBuffer(asciiDomainCharacters, asciiDomainValue.size());
    m_url.m_hostEnd = currentPosition(iterator);
    if (!iterator.atEnd())
        return parsePort(iterator);
    m_url.m_portLength = 0;
    return true;
}

const UIDNA& URLParser::internationalDomainNameTranscoder()
{
    static UIDNA* encoder = [] {
        UErrorCode error = U_ZERO_ERROR;
        UIDNA* result = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_UNICODE | UIDNA_NONTRANSITIONAL_TO_ASCII, &error);
        RELEASE_ASSERT(!U_FAILURE(error) && result);
        return result;
    }();
    return *encoder;
}

// Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@e12a212a531aeddc11259e8515671cf5188e038d
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

inline bool isASCIISpace(UChar c) { return c <= ' ' && (c == ' ' || (c <= 0xD && c >= 0x9)); }

template<typename IntegralType> Optional<IntegralType> parseInteger(StringView string)
{
    unsigned length = string.length();
    unsigned position = 0;
    if (string.isNull())
        return std::nullopt;
    while (position < length && isASCIISpace(string[position]))
        ++position;
    if (position < length && string[position] == '+')
        ++position;
    if (!(position < length && isASCIIDigit(string[position])))
        return std::nullopt;
    Checked<IntegralType, RecordOverflow> value = 0;
    do {
        value *= static_cast<IntegralType>(10);
        value += static_cast<IntegralType>(string[position] - '0');
    } while (++position < length && isASCIIDigit(string[position]));
    if (UNLIKELY(value.hasOverflowed()))
        return std::nullopt;
    while (position < length && isASCIISpace(string[position]))
        ++position;
    if (position < length)
        return std::nullopt;
    return value.unsafeGet();
}

inline Optional<String> maybeCanonicalizeScheme(const String& scheme)
{
    auto result = _ZN3WTF9URLParser23maybeCanonicalizeSchemeERKNS_6StringE(scheme.abi());
    if (!result.hasValue)
        return std::nullopt;
    return String(std::move(result.value));
}

bool URL::hasSpecialScheme() const
{
    return protocolIs("ftp")
        || protocolIs("file")
        || protocolIs("http")
        || protocolIs("https")
        || protocolIs("ws")
        || protocolIs("wss");
}

Optional<std::uint16_t> URL::port() const
{
    return m_portLength ? parseInteger<std::uint16_t>(StringView(m_string).substring(m_hostEnd + 1, m_portLength - 1)) : std::nullopt;
}

bool URL::setProtocol(StringView newProtocol)
{
    auto newProtocolPrefix = newProtocol.substring(0, newProtocol.find(':'));
    auto newProtocolCanonicalized = maybeCanonicalizeScheme(newProtocolPrefix.toStringWithoutCopying());
    if (!newProtocolCanonicalized)
        return false;

    if (!m_isValid) {
        parse(makeString(*newProtocolCanonicalized, ':', m_string));
        return true;
    }

    if ((m_passwordEnd != m_userStart || port()) && StringView(*newProtocolCanonicalized) == StringView(reinterpret_cast<const LChar*>("file"), 4))
        return true;

    if (isLocalFile() && host().isEmpty())
        return true;

    parse(makeString(*newProtocolCanonicalized, StringView(m_string).substring(m_schemeEnd)));
    return true;
}

static bool appendEncodedHostname(Vector<UChar, 512>& buffer, StringView string)
{
    const unsigned hostnameBufferLength = 2048;

    if (string.length() > hostnameBufferLength || string.isAllASCII()) {
        for (unsigned i = 0; i < string.length(); ++i)
            buffer.append(string[i]);
        return true;
    }

    UChar hostnameBuffer[hostnameBufferLength];
    UErrorCode error = U_ZERO_ERROR;
    UIDNAInfo processingDetails = UIDNA_INFO_INITIALIZER;
    std::int32_t numCharactersConverted = uidna_nameToASCII(&URLParser::internationalDomainNameTranscoder(),
        string.upconvertedCharacters(), string.length(), hostnameBuffer, hostnameBufferLength, &processingDetails, &error);

    if (U_SUCCESS(error) && !processingDetails.errors) {
        buffer.append(hostnameBuffer, numCharactersConverted);
        return true;
    }
    return false;
}

unsigned URL::credentialsEnd() const
{
    unsigned end = m_passwordEnd;
    if (end != m_hostEnd && m_string[end] == '@')
        end += 1;
    return end;
}

static bool forwardSlashHashOrQuestionMark(UChar c)
{
    return c == '/'
        || c == '#'
        || c == '?';
}

static bool slashHashOrQuestionMark(UChar c)
{
    return forwardSlashHashOrQuestionMark(c) || c == '\\';
}

void URL::setHost(StringView newHost)
{
    if (!m_isValid)
        return;

    if (newHost.contains(':') && !newHost.startsWith('['))
        return;

    if (auto index = newHost.find(hasSpecialScheme() ? slashHashOrQuestionMark : forwardSlashHashOrQuestionMark); index != notFound)
        newHost = newHost.substring(0, index);

    Vector<UChar, 512> encodedHostName;
    if (!appendEncodedHostname(encodedHostName, newHost))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//" : "",
        StringView(encodedHostName.data(), encodedHostName.size()),
        StringView(m_string).substring(m_hostEnd)
    ));
}

void URL::setHostAndPort(StringView hostAndPort)
{
    if (!m_isValid)
        return;

    auto hostName = hostAndPort;
    StringView portString;
    auto colonIndex = hostName.reverseFind(':');
    if (colonIndex != notFound) {
        portString = hostName.substring(colonIndex + 1);
        hostName = hostName.substring(0, colonIndex);
        if (hostName.contains(':') && !hostName.startsWith('['))
            return;
        if (!parseInteger<std::uint16_t>(portString))
            portString = { };
    }
    if (hostName.isEmpty()) {
        remove(hostStart(), pathStart() - hostStart());
        return;
    }

    Vector<UChar, 512> encodedHostName;
    if (!appendEncodedHostname(encodedHostName, hostName))
        return;

    bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
    parse(makeString(
        StringView(m_string).left(hostStart()),
        slashSlashNeeded ? "//" : "",
        StringView(encodedHostName.data(), encodedHostName.size()),
        portString.isEmpty() ? "" : ":",
        portString,
        StringView(m_string).substring(pathStart())
    ));
}

static String percentEncodeCharacters(const String& input, bool(*shouldEncode)(UChar))
{
    auto encode = [shouldEncode] (const String& input) {
        std::string utf8 = input.utf8();
        auto* data = utf8.data();
        std::string builder;
        auto length = utf8.length();
        for (unsigned j = 0; j < length; j++) {
            auto c = data[j];
            if (shouldEncode(c)) {
                builder.push_back('%');
                builder.push_back(upperNibbleToASCIIHexDigit(c));
                builder.push_back(lowerNibbleToASCIIHexDigit(c));
            } else
                builder.push_back(c);
        }
        return String::fromLatin1(reinterpret_cast<const LChar*>(builder.data()), builder.size());
    };

    for (std::size_t i = 0; i < input.length(); ++i) {
        if (UNLIKELY(shouldEncode(input[i])))
            return encode(input);
    }
    return input;
}

void URL::parse(const String& string)
{
    *this = URLParser(string).result();
}

void URL::remove(unsigned start, unsigned length)
{
    if (!length)
        return;

    auto stringAfterRemoval = WTFMove(m_string);
    stringAfterRemoval.remove(start, length);
    parse(stringAfterRemoval);
}

void URL::setUser(StringView newUser)
{
    if (!m_isValid)
        return;

    unsigned end = m_userEnd;
    if (!newUser.isEmpty()) {
        bool slashSlashNeeded = m_userStart == m_schemeEnd + 1U;
        bool needSeparator = end == m_hostEnd || (end == m_passwordEnd && m_string[end] != '@');
        parse(makeString(
            StringView(m_string).left(m_userStart),
            slashSlashNeeded ? "//" : "",
            percentEncodeCharacters(newUser.toStringWithoutCopying(), URLParser::isInUserInfoEncodeSet),
            needSeparator ? "@" : "",
            StringView(m_string).substring(end)
        ));
    } else {
        if (m_userEnd == m_passwordEnd && end != m_hostEnd && m_string[end] == '@')
            end += 1;
        remove(m_userStart, end - m_userStart);
    }
}

void URL::setPassword(StringView newPassword)
{
    if (!m_isValid)
        return;

    if (!newPassword.isEmpty()) {
        bool needLeadingSlashes = m_userEnd == m_schemeEnd + 1U;
        parse(makeString(
            StringView(m_string).left(m_userEnd),
            needLeadingSlashes ? "//:" : ":",
            percentEncodeCharacters(newPassword.toStringWithoutCopying(), URLParser::isInUserInfoEncodeSet),
            '@',
            StringView(m_string).substring(credentialsEnd())
        ));
    } else {
        unsigned end = m_userStart == m_userEnd ? credentialsEnd() : m_passwordEnd;
        remove(m_userEnd, end - m_userEnd);
    }
}

void URL::removeCredentials()
{
    if (!m_isValid)
        return;

    remove(m_userStart, credentialsEnd() - m_userStart);
}

void URL::setFragmentIdentifier(StringView identifier)
{
    if (!m_isValid)
        return;

    parse(makeString(StringView(m_string).left(m_queryEnd), '#', identifier));
}

void URL::setQuery(StringView newQuery)
{
    if (!m_isValid)
        return;

    parse(makeString(
        StringView(m_string).left(m_pathEnd),
        (!newQuery.startsWith('?') && !newQuery.isNull()) ? "?" : "",
        newQuery,
        StringView(m_string).substring(m_queryEnd)
    ));
}

static String escapePathWithoutCopying(StringView path)
{
    auto questionMarkOrNumberSign = [] (UChar character) {
        return character == '?' || character == '#';
    };
    return percentEncodeCharacters(path.toStringWithoutCopying(), questionMarkOrNumberSign);
}

void URL::setPath(StringView path)
{
    if (!m_isValid)
        return;

    parse(makeString(
        StringView(m_string).left(pathStart()),
        path.startsWith('/') || (path.startsWith('\\') && (hasSpecialScheme() || protocolIs("file"))) ? "" : "/",
        escapePathWithoutCopying(path),
        StringView(m_string).substring(m_pathEnd)
    ));
}

URL URL::truncatedForUseAsBase() const
{
    return URLParser(m_string.left(m_pathAfterLastSlash), URL()).result();
}

URL URL::fileURLWithFileSystemPath(StringView path)
{
    return URLParser(makeString(
        "file://",
        path.startsWith('/') ? "" : "/",
        escapePathWithoutCopying(path)
    ), URL()).result();
}

}

namespace {

using NKWebKit::UrlParserPort::URLParser;
using NKWebKit::UrlParserPort::URLTextEncoding;
namespace Port = NKWebKit::UrlParserPort;

Port::URL FromAbi(const NKWebKit::WtfAbi::URL& url)
{
    Port::URL result;
    result.m_string = Port::String(NKWebKit::WtfAbi::String(url.impl ? (url.impl->refCount += 2, url.impl) : nullptr));
    result.m_isValid = url.bits & 1;
    result.m_protocolIsInHTTPFamily = (url.bits >> 1) & 1;
    result.m_cannotBeABaseURL = (url.bits >> 2) & 1;
    result.m_portLength = (url.bits >> 3) & 7;
    result.m_schemeEnd = url.bits >> 6;
    result.m_userStart = url.userStart;
    result.m_userEnd = url.userEnd;
    result.m_passwordEnd = url.passwordEnd;
    result.m_hostEnd = url.hostEnd;
    result.m_pathAfterLastSlash = url.pathAfterLastSlash;
    result.m_pathEnd = url.pathEnd;
    result.m_queryEnd = url.queryEnd;
    return result;
}

void ToAbi(const Port::URL& url, NKWebKit::WtfAbi::URL* result)
{
    NKWebKit::WtfAbi::String string = url.m_string.abi();
    result->impl = std::exchange(string.impl, nullptr);
    result->bits = url.m_isValid | (url.m_protocolIsInHTTPFamily << 1) | (url.m_cannotBeABaseURL << 2) | (url.m_portLength << 3) | (url.m_schemeEnd << 6);
    result->userStart = url.m_userStart;
    result->userEnd = url.m_userEnd;
    result->passwordEnd = url.m_passwordEnd;
    result->hostEnd = url.m_hostEnd;
    result->pathAfterLastSlash = url.m_pathAfterLastSlash;
    result->pathEnd = url.m_pathEnd;
    result->queryEnd = url.m_queryEnd;
}

Port::StringView ViewFromAbi(NKWebKit::WtfAbi::StringView view, const char* caller)
{
    if (!view.data && view.length)
        throw std::invalid_argument(std::string("URL::") + caller + " requires characters for a non-empty view");
    if (!view.data)
        return { };
    if (view.is8Bit)
        return Port::StringView(static_cast<const Port::LChar*>(view.data), view.length);
    return Port::StringView(static_cast<const Port::UChar*>(view.data), view.length);
}

template<typename Function> auto Mutate(NKWebKit::WtfAbi::URL* self, const char* caller, Function function)
{
    if (!self)
        throw std::invalid_argument(std::string("URL::") + caller + " requires an object");
    Port::URL url = FromAbi(*self);
    if constexpr (std::is_void_v<decltype(function(url))>) {
        function(url);
        NKWebKit::WtfAbi::Release(self->impl);
        ToAbi(url, self);
    } else {
        auto result = function(url);
        NKWebKit::WtfAbi::Release(self->impl);
        ToAbi(url, self);
        return result;
    }
}

NKWebKit::WtfAbi::URL ToAbiResult(const Port::URL& url)
{
    NKWebKit::WtfAbi::URL result {};
    ToAbi(url, &result);
    return result;
}

}

namespace NKWebKit::WtfAbi {

void SetUrlPassword(URL* self, StringView password)
{
    auto view = ViewFromAbi(password, "setPassword");
    Mutate(self, "setPassword", [&](Port::URL& url) { url.setPassword(view); });
}

}

extern "C" {

bool APS5_VABI _ZN3WTF3URL11setProtocolENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView protocol)
{
    auto view = ViewFromAbi(protocol, "setProtocol");
    return Mutate(self, "setProtocol", [&](Port::URL& url) { return url.setProtocol(view); });
}

void APS5_VABI _ZN3WTF3URL7setHostENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView host)
{
    auto view = ViewFromAbi(host, "setHost");
    Mutate(self, "setHost", [&](Port::URL& url) { url.setHost(view); });
}

void APS5_VABI _ZN3WTF3URL14setHostAndPortENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView hostAndPort)
{
    auto view = ViewFromAbi(hostAndPort, "setHostAndPort");
    Mutate(self, "setHostAndPort", [&](Port::URL& url) { url.setHostAndPort(view); });
}

void APS5_VABI _ZN3WTF3URL7setUserENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView user)
{
    auto view = ViewFromAbi(user, "setUser");
    Mutate(self, "setUser", [&](Port::URL& url) { url.setUser(view); });
}

void APS5_VABI _ZN3WTF3URL17removeCredentialsEv(NKWebKit::WtfAbi::URL* self)
{
    Mutate(self, "removeCredentials", [](Port::URL& url) { url.removeCredentials(); });
}

void APS5_VABI _ZN3WTF3URL21setFragmentIdentifierENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView identifier)
{
    auto view = ViewFromAbi(identifier, "setFragmentIdentifier");
    Mutate(self, "setFragmentIdentifier", [&](Port::URL& url) { url.setFragmentIdentifier(view); });
}

void APS5_VABI _ZN3WTF3URL8setQueryENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView query)
{
    auto view = ViewFromAbi(query, "setQuery");
    Mutate(self, "setQuery", [&](Port::URL& url) { url.setQuery(view); });
}

void APS5_VABI _ZN3WTF3URL7setPathENS_10StringViewE(NKWebKit::WtfAbi::URL* self, NKWebKit::WtfAbi::StringView path)
{
    auto view = ViewFromAbi(path, "setPath");
    Mutate(self, "setPath", [&](Port::URL& url) { url.setPath(view); });
}

NKWebKit::WtfAbi::URL APS5_VABI _ZNK3WTF3URL21truncatedForUseAsBaseEv(const NKWebKit::WtfAbi::URL* self)
{
    if (!self)
        throw std::invalid_argument("URL::truncatedForUseAsBase requires an object");
    return ToAbiResult(FromAbi(*self).truncatedForUseAsBase());
}

NKWebKit::WtfAbi::URL APS5_VABI _ZN3WTF3URL25fileURLWithFileSystemPathENS_10StringViewE(NKWebKit::WtfAbi::StringView path)
{
    return ToAbiResult(Port::URL::fileURLWithFileSystemPath(ViewFromAbi(path, "fileURLWithFileSystemPath")));
}

}

extern "C" {

void APS5_VABI _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(NKWebKit::WtfAbi::URL* self, const NKWebKit::WtfAbi::URL* base, const NKWebKit::WtfAbi::String* relative, const void* encoding)
{
    if (!self || !base || !relative)
        throw std::invalid_argument("URL::URL requires an object, a base URL and a relative string");
    const auto* textEncoding = static_cast<const URLTextEncoding*>(encoding);
    if (textEncoding && !textEncoding->vtable)
        throw std::invalid_argument("URL::URL requires a text encoding with a vtable");
    Port::String input(*relative);
    ToAbi(URLParser(input, FromAbi(*base), textEncoding).result(), self);
}

}
