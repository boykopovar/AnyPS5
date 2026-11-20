// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/AtomStringImpl.cpp
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/AtomString.cpp
// Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/AtomStringTable.cpp
/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Yusuke Suzuki<utatane.tea@gmail.com>. All rights reserved.
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
 *
 */
/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 */
/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 *
 */

#include "prx/libSceNKWebKit/include/WtfAtomStringExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace NKWebKit {
void* Allocate(std::size_t size, std::size_t alignment = 16);
}

namespace {

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

constexpr unsigned BufferOwnershipMask = 3;
constexpr unsigned Is8BitFlag = 4;
constexpr unsigned IsAtomFlag = 16;
constexpr unsigned IsSymbolFlag = 32;
constexpr unsigned FlagMask = 255;
constexpr std::size_t LocalBufferSize = 100;
constexpr unsigned BufferSubstring = 2;
constexpr std::size_t SubstringOwnerOffset = 24;
constexpr std::size_t SubstringAllocationSize = 32;
constexpr std::size_t ImplHeaderSize = 20;

struct Characters {
    const void* data;
    unsigned length;
    bool is8Bit;

    char16_t operator[](unsigned index) const {
        return is8Bit ? static_cast<const unsigned char*>(data)[index] : static_cast<const char16_t*>(data)[index];
    }
};

Characters CharactersOf(const StringImpl& string) {
    return {string.data, string.length, (string.hashAndFlags & Is8BitFlag) != 0};
}

std::uint64_t ContentHash(const Characters& characters) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned i = 0; i < characters.length; ++i) {
        hash ^= characters[i];
        hash *= 1099511628211ULL;
    }
    return hash ^ characters.length;
}

bool SameContent(const StringImpl& string, const Characters& characters) {
    if (string.length != characters.length) return false;
    const Characters existing = CharactersOf(string);
    for (unsigned i = 0; i < characters.length; ++i) {
        if (existing[i] != characters[i]) return false;
    }
    return true;
}

StringImpl* Retained(StringImpl* string) {
    string->refCount += 2;
    return string;
}

bool IsStatic(const StringImpl& string) {
    return string.refCount & 1;
}

void SetHashIfMissing(StringImpl& string) {
    if (!(string.hashAndFlags >> 8)) _ZNK3WTF10StringImpl12hashSlowCaseEv(&string);
}

class AtomStringTable {
public:
    AtomStringTable() = default;
    AtomStringTable(const AtomStringTable&) = delete;
    AtomStringTable& operator=(const AtomStringTable&) = delete;

    ~AtomStringTable() {
        for (auto& entry : m_table) {
            if (!IsStatic(*entry.second)) entry.second->hashAndFlags &= ~IsAtomFlag;
        }
    }

    StringImpl* Find(const Characters& characters) const {
        auto range = m_table.equal_range(ContentHash(characters));
        for (auto it = range.first; it != range.second; ++it) {
            if (SameContent(*it->second, characters)) return it->second;
        }
        return nullptr;
    }

    void Insert(StringImpl& string) {
        m_table.emplace(ContentHash(CharactersOf(string)), &string);
    }

    bool Remove(const StringImpl& string) {
        auto range = m_table.equal_range(ContentHash(CharactersOf(string)));
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == &string) {
                m_table.erase(it);
                return true;
            }
        }
        return false;
    }

private:
    std::unordered_multimap<std::uint64_t, StringImpl*> m_table;
};

AtomStringTable& CurrentTable() {
    thread_local AtomStringTable table;
    return table;
}

String Existing(StringImpl* string) {
    return String(Retained(string));
}

template<typename Create> String AddToTable(const Characters& characters, Create create) {
    auto& table = CurrentTable();
    if (StringImpl* existing = table.Find(characters)) return Existing(existing);
    String created = create();
    StringImpl& string = *created.impl;
    SetHashIfMissing(string);
    string.hashAndFlags |= IsAtomFlag;
    table.Insert(string);
    return created;
}

String CopyCharacters(const Characters& characters) {
    if (characters.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(characters.data), characters.length);
    return _ZN3WTF10StringImpl20create8BitIfPossibleEPKDsj(static_cast<const char16_t*>(characters.data), characters.length);
}

String WithoutCopying(const Characters& characters, unsigned hash) {
    auto* string = new (NKWebKit::Allocate(sizeof(StringImpl))) StringImpl{2, characters.length, characters.data, characters.is8Bit ? Is8BitFlag : 0U};
    string->hashAndFlags |= hash << 8;
    return String(string);
}

String Empty() {
    return String(NKWebKit::WtfAbi::EmptyStringImpl());
}

String AddCharacters(const Characters& characters) {
    if (!characters.data) return String();
    if (!characters.length) return Empty();
    return AddToTable(characters, [&] { return CopyCharacters(characters); });
}

String CreateSubstringSharingImpl(StringImpl& base, unsigned start, unsigned length) {
    if (start || length != base.length) {
        const String owner(Retained(&base));
        return NKWebKit::WtfAbi::SubstringSharingImpl(owner, start, length);
    }
    const Characters characters = CharactersOf(base);
    const std::size_t width = characters.is8Bit ? 1 : 2;
    if (SubstringAllocationSize >= ImplHeaderSize + width * length) {
        if (characters.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(characters.data), length);
        return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(characters.data), length);
    }
    StringImpl* owner = (base.hashAndFlags & BufferOwnershipMask) == BufferSubstring ? *reinterpret_cast<StringImpl**>(reinterpret_cast<unsigned char*>(&base) + SubstringOwnerOffset) : &base;
    auto* memory = static_cast<unsigned char*>(NKWebKit::Allocate(SubstringAllocationSize));
    auto* string = new (memory) StringImpl{2, length, characters.data, (characters.is8Bit ? Is8BitFlag : 0U) | BufferSubstring};
    *reinterpret_cast<StringImpl**>(memory + SubstringOwnerOffset) = Retained(owner);
    return String(string);
}

String AddSubstring(StringImpl& base, unsigned start, unsigned length) {
    const Characters whole = CharactersOf(base);
    const std::size_t width = whole.is8Bit ? 1 : 2;
    const Characters characters{static_cast<const unsigned char*>(whole.data) + width * start, length, whole.is8Bit};
    return AddToTable(characters, [&] { return CreateSubstringSharingImpl(base, start, length); });
}

String AddStatic(const StringImpl& base) {
    const Characters characters = CharactersOf(base);
    return AddToTable(characters, [&] { return WithoutCopying(characters, base.hashAndFlags >> 8); });
}

String AddExisting(StringImpl& string) {
    if (!string.length) return Empty();
    if (IsStatic(string)) return AddStatic(string);
    if (string.hashAndFlags & IsSymbolFlag) return AddSubstring(string, 0, string.length);
    if (string.hashAndFlags & IsAtomFlag) throw std::logic_error("WTF::AtomStringImpl::addSlowCase was given a string that is already an atom");
    auto& table = CurrentTable();
    if (StringImpl* existing = table.Find(CharactersOf(string))) return Existing(existing);
    SetHashIfMissing(string);
    string.hashAndFlags |= IsAtomFlag;
    table.Insert(string);
    return Existing(&string);
}

String AddPointer(StringImpl* string) {
    if (!string) return String();
    if (string->hashAndFlags & IsAtomFlag) return Existing(string);
    return AddExisting(*string);
}

String LookUp(const Characters& characters) {
    if (StringImpl* existing = CurrentTable().Find(characters)) return Existing(existing);
    return String();
}

AtomString Atom(String string) {
    return AtomString{std::move(string)};
}

AtomString FromLatin1(const char* characters, std::size_t length) {
    return Atom(AddCharacters({characters, static_cast<unsigned>(length), true}));
}

template<typename Integer> AtomString IntegerAtom(Integer number) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer), number);
    if (result.ec != std::errc{}) throw std::runtime_error("WTF::AtomString::number integer formatting failed");
    return FromLatin1(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

template<typename Number> AtomString ShortestAtom(Number number) {
    const std::string text = NKWebKit::WtfAbi::NumberToString(number);
    return FromLatin1(text.data(), text.size());
}

AtomString ConvertCase(const AtomString* self, bool lower) {
    if (!self) throw std::invalid_argument("WTF::AtomString case conversion requires an object");
    StringImpl* string = self->string.impl;
    if (!string) return AtomString{};
    const auto changes = [lower](char16_t c) { return lower ? c >= 'A' && c <= 'Z' : c >= 'a' && c <= 'z'; };
    const auto convert = [lower](char16_t c) { return static_cast<char16_t>(lower ? c + 32 : c - 32); };
    const Characters characters = CharactersOf(*string);
    if (characters.is8Bit && characters.length <= LocalBufferSize) {
        unsigned failingIndex = 0;
        while (failingIndex < characters.length && !changes(characters[failingIndex])) ++failingIndex;
        if (failingIndex == characters.length) return Atom(Existing(string));
        unsigned char localBuffer[LocalBufferSize];
        for (unsigned i = 0; i < characters.length; ++i) {
            const char16_t c = characters[i];
            localBuffer[i] = static_cast<unsigned char>(i >= failingIndex && changes(c) ? convert(c) : c);
        }
        return Atom(AddCharacters({localBuffer, characters.length, true}));
    }
    String converted = lower ? _ZN3WTF10StringImpl23convertToASCIILowercaseEv(string) : _ZN3WTF10StringImpl23convertToASCIIUppercaseEv(string);
    if (converted.impl == string) return Atom(std::move(converted));
    return Atom(AddPointer(converted.impl));
}

}  // namespace

namespace NKWebKit::WtfAbi {

void RemoveAtom(StringImpl* string) {
    if (!CurrentTable().Remove(*string)) std::terminate();
}

}  // namespace NKWebKit::WtfAbi

extern "C" {

String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKhj(const unsigned char* characters, unsigned length) {
    return AddCharacters({characters, length, true});
}

String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKDsj(const char16_t* characters, unsigned length) {
    return AddCharacters({characters, length, false});
}

String APS5_VABI _ZN3WTF14AtomStringImpl3addEPNS_10StringImplEjj(StringImpl* baseString, unsigned start, unsigned length) {
    if (!baseString) return String();
    if (!length || start >= baseString->length) return Empty();
    const unsigned maxLength = baseString->length - start;
    if (length >= maxLength) {
        if (!start) return AddPointer(baseString);
        length = maxLength;
    }
    return AddSubstring(*baseString, start, length);
}

String APS5_VABI _ZN3WTF14AtomStringImpl3addEPKNS_10StringImpl16StaticStringImplE(const StringImpl* string) {
    if (!string) throw std::invalid_argument("WTF::AtomStringImpl::add requires a static string");
    if (!IsStatic(*string)) throw std::invalid_argument("WTF::AtomStringImpl::add was given a string that is not static");
    if (!string->length) throw std::invalid_argument("WTF::AtomStringImpl::add was given an empty static string");
    return AddStatic(*string);
}

String APS5_VABI _ZN3WTF14AtomStringImpl10addLiteralEPKcj(const char* characters, unsigned length) {
    if (!characters || !length) throw std::invalid_argument("WTF::AtomStringImpl::addLiteral requires a non-empty literal");
    const Characters literal{characters, length, true};
    return AddToTable(literal, [&] { return WithoutCopying(literal, 0); });
}

String APS5_VABI _ZN3WTF14AtomStringImpl11addSlowCaseERNS_10StringImplE(StringImpl* string) {
    if (!string) throw std::invalid_argument("WTF::AtomStringImpl::addSlowCase requires a string");
    return AddExisting(*string);
}

String APS5_VABI _ZN3WTF14AtomStringImpl11addSlowCaseERNS_15AtomStringTableERNS_10StringImplE(void* table, StringImpl* string) {
    if (!table || !string) throw std::invalid_argument("WTF::AtomStringImpl::addSlowCase requires a table and a string");
    return AddExisting(*string);
}

String APS5_VABI _ZN3WTF14AtomStringImpl14lookUpSlowCaseERNS_10StringImplE(StringImpl* string) {
    if (!string) throw std::invalid_argument("WTF::AtomStringImpl::lookUpSlowCase requires a string");
    if (string->hashAndFlags & IsAtomFlag) throw std::logic_error("WTF::AtomStringImpl::lookUpSlowCase was given a string that is already an atom");
    if (!string->length) return Empty();
    return LookUp(CharactersOf(*string));
}

String APS5_VABI _ZN3WTF14AtomStringImpl6lookUpEPKhj(const unsigned char* characters, unsigned length) {
    if (!characters && length) throw std::invalid_argument("WTF::AtomStringImpl::lookUp requires characters");
    return LookUp({characters, length, true});
}

String APS5_VABI _ZN3WTF14AtomStringImpl6lookUpEPKDsj(const char16_t* characters, unsigned length) {
    if (!characters && length) throw std::invalid_argument("WTF::AtomStringImpl::lookUp requires characters");
    return LookUp({characters, length, false});
}

AtomString APS5_VABI _ZN3WTF10AtomString16fromUTF8InternalEPKcS2_(const char* start, const char* end) {
    if (!start) throw std::invalid_argument("WTF::AtomString::fromUTF8Internal requires characters");
    if (!end) end = start + std::strlen(start);
    if (end <= start) throw std::invalid_argument("WTF::AtomString::fromUTF8Internal requires a non-empty range");
    unsigned length = 0;
    unsigned utf16Length = 0;
    if (!_ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(start, end, length, utf16Length)) return AtomString{};
    String decoded = _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(start), static_cast<std::size_t>(end - start));
    if (!decoded.impl) return AtomString{};
    StringImpl& string = *decoded.impl;
    return Atom(AddToTable(CharactersOf(string), [&] { return std::move(decoded); }));
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEi(int number) {
    return IntegerAtom(number);
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEj(unsigned number) {
    return IntegerAtom(number);
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEm(std::uint64_t number) {
    return IntegerAtom(number);
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEy(unsigned long long number) {
    return IntegerAtom(number);
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEf(float number) {
    return ShortestAtom(number);
}

AtomString APS5_VABI _ZN3WTF10AtomString6numberEd(double number) {
    return ShortestAtom(number);
}

AtomString APS5_VABI _ZNK3WTF10AtomString23convertToASCIILowercaseEv(const AtomString* self) {
    return ConvertCase(self, true);
}

AtomString APS5_VABI _ZNK3WTF10AtomString23convertToASCIIUppercaseEv(const AtomString* self) {
    return ConvertCase(self, false);
}

void APS5_VABI _ZN3WTF10StringImpl7destroyEPS0_(StringImpl* stringImpl) {
    if (!stringImpl) throw std::invalid_argument("WTF::StringImpl::destroy requires a string");
    if (stringImpl->refCount != 2) throw std::logic_error("WTF::StringImpl::destroy was given a string whose last reference is not being dropped");
    NKWebKit::WtfAbi::Destroy(stringImpl);
}

}
