/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
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
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
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
 * Derived from Source/WebCore/page/SettingsBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba,
 * Source/WebCore/platform/graphics/FontGenericFamilies.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 * and Source/WTF/wtf/HashTable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include "prx/libSceNKWebKit/include/SettingsBaseExports.hpp"

#include "prx/libSceNKWebKit/src/SettingsBaseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfLanguageAbi.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using namespace NKWebKit::SettingsBaseAbi;
using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WebCoreAbi::ContentType;

namespace WtfAbi = NKWebKit::WtfAbi;

constexpr int kUscriptCommon = 0;
constexpr int kUscriptHan = 17;
constexpr int kUscriptSimplifiedHan = 73;
constexpr int kUscriptTraditionalHan = 74;
constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);
constexpr std::size_t kMinVectorCapacity = 16;

struct RawTableMeta {
    unsigned deletedCount;
    unsigned keyCount;
    unsigned sizeMask;
    unsigned tableSize;
};

struct RawScriptEntry {
    int key;
    StringImpl* value;
};

struct RawVector {
    ContentType* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<StringImpl*>);
static_assert(std::is_trivially_copyable_v<void*>);
static_assert(std::is_trivially_copyable_v<double>);
static_assert(std::is_trivially_copyable_v<RawTableMeta>);
static_assert(sizeof(RawTableMeta) == kTableMetadataSize);
static_assert(std::is_trivially_copyable_v<RawScriptEntry>);
static_assert(sizeof(RawScriptEntry) == sizeof(ScriptFontFamilyEntry));
static_assert(std::is_trivially_copyable_v<RawVector>);
static_assert(sizeof(RawVector) == sizeof(WtfAbi::Vector<ContentType>));

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    RequireWritableGuestBytes(pointer, sizeof(T), function);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

struct GuestString {
    std::vector<unsigned char> narrow;
    std::vector<char16_t> wide;
    unsigned length = 0;
    bool is8Bit = true;
    bool isNull = true;

    StringView View() const {
        if (isNull || !length) return StringView{nullptr, 0, is8Bit};
        return StringView{is8Bit ? static_cast<const void*>(narrow.data()) : static_cast<const void*>(wide.data()), length, is8Bit};
    }
};

GuestString ReadGuestString(const StringImpl* impl, const char* function) {
    GuestString result{};
    if (!impl) return result;
    const StringImpl header = ReadGuestValue(impl, function);
    result.isNull = false;
    result.length = header.length;
    result.is8Bit = (header.hashAndFlags & 4u) != 0;
    if (!header.length) return result;
    if (!header.data) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (result.is8Bit ? 1u : sizeof(char16_t));
    RequireReadableGuestBytes(header.data, byteLength, function);
    if (result.is8Bit) {
        result.narrow.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    } else {
        result.wide.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(result.wide.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return result;
}

bool GuestStringsEqual(const GuestString& left, const GuestString& right) {
    if (left.isNull || right.isNull) return left.isNull && right.isNull;
    if (left.length != right.length) return false;
    const StringView leftView = left.View();
    const StringView rightView = right.View();
    for (unsigned index = 0; index < left.length; ++index) {
        if (NKWebKit::WtfAbi::CodeUnitAt(leftView, index) != NKWebKit::WtfAbi::CodeUnitAt(rightView, index)) return false;
    }
    return true;
}

void* CheckedSelf(void* self) {
    if (!self) throw std::invalid_argument("null WebCore::SettingsBase");
    return self;
}

using FontGenericFamiliesGetter = void* (APS5_VABI*)(void*);

void* FontGenericFamiliesOf(void* self, bool constant, const char* function) {
    CheckedSelf(self);
    void* const* entries = ReadGuestValue(reinterpret_cast<void* const* const*>(self), function);
    if (!entries) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    void* getterAddress = ReadGuestValue(entries + (constant ? 1 : 0), function);
    if (!getterAddress) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    auto getter = reinterpret_cast<FontGenericFamiliesGetter>(getterAddress);
    void* families = getter(self);
    if (!families) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return families;
}

unsigned HashScript(int script) {
    std::uint32_t key = static_cast<std::uint32_t>(script);
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^= (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

ScriptFontFamilyEntry* Lookup(ScriptFontFamilyEntry* table, int script, const char* function) {
    if (!table) return nullptr;
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(table);
    if (tableAddress < kTableMetadataSize) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const RawTableMeta meta = ReadGuestValue(reinterpret_cast<const RawTableMeta*>(tableAddress - kTableMetadataSize), function);
    if (!meta.tableSize || (meta.tableSize & (meta.tableSize - 1)) != 0 || meta.sizeMask != meta.tableSize - 1)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(table, static_cast<std::size_t>(meta.tableSize) * sizeof(ScriptFontFamilyEntry), function);
    unsigned i = HashScript(script) & meta.sizeMask;
    unsigned probeCount = 0;
    for (unsigned step = 0; step < meta.tableSize; ++step) {
        const RawScriptEntry entry = ReadGuestValue(reinterpret_cast<const RawScriptEntry*>(table) + i, function);
        if (entry.key == script) return &table[i];
        if (entry.key == kEmptyKey) return nullptr;
        ++probeCount;
        i = (i + probeCount) & meta.sizeMask;
    }
    NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

const String* EmptyFamilyAtom() {
    static const String value(NKWebKit::WtfAbi::EmptyStringImpl());
    return &value;
}

const String* GenericFamilyForScript(ScriptFontFamilyEntry* table, int script, const char* function) {
    if (ScriptFontFamilyEntry* found = Lookup(table, script, function)) return &found->value;
    if (script == kUscriptHan) {
        const int fallback = NKWebKit::UserPrefersSimplifiedChinese_nid_no_patch() ? kUscriptSimplifiedHan : kUscriptTraditionalHan;
        if (ScriptFontFamilyEntry* found = Lookup(table, fallback, function)) return &found->value;
    }
    if (script != kUscriptCommon) return GenericFamilyForScript(table, kUscriptCommon, function);
    return EmptyFamilyAtom();
}

bool GenericFamilyWouldChange(ScriptFontFamilyEntry* table, const AtomString* family, int script, const char* function) {
    StringImpl* const familyImpl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(family), function);
    const GuestString familyText = ReadGuestString(familyImpl, function);
    ScriptFontFamilyEntry* entry = Lookup(table, script, function);
    if (familyText.isNull || !familyText.length) return entry != nullptr;
    if (!entry) return true;
    StringImpl* const entryImpl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(reinterpret_cast<const unsigned char*>(entry) + offsetof(ScriptFontFamilyEntry, value)), function);
    const GuestString entryText = ReadGuestString(entryImpl, function);
    return !GuestStringsEqual(entryText, familyText);
}

ScriptFontFamilyEntry* MapTableOf(void* families, std::size_t offset, const char* function) {
    return ReadGuestValue(reinterpret_cast<ScriptFontFamilyEntry* const*>(static_cast<const unsigned char*>(families) + offset), function);
}

const String* FamilyOf(void* self, std::size_t offset, int script, const char* function) {
    void* families = FontGenericFamiliesOf(self, true, function);
    return GenericFamilyForScript(MapTableOf(families, offset, function), script, function);
}

void SetFamilyOf(void* self, std::size_t offset, const AtomString* family, int script, const char* name) {
    if (!family) throw std::invalid_argument("null WTF::AtomString");
    void* families = FontGenericFamiliesOf(self, false, name);
    if (GenericFamilyWouldChange(MapTableOf(families, offset, name), family, script, name)) NotImplemented_nid_no_patch(name);
}

WtfAbi::Vector<ContentType>& MediaVectorOf(void* self) {
    return *reinterpret_cast<WtfAbi::Vector<ContentType>*>(static_cast<unsigned char*>(self) + kMediaContentTypesRequiringHardwareSupportOffset);
}

void ValidateTargetMediaVector(void* self, const char* function) {
    auto* vectorPtr = reinterpret_cast<RawVector*>(static_cast<unsigned char*>(self) + kMediaContentTypesRequiringHardwareSupportOffset);
    const RawVector vector = ReadGuestValue(vectorPtr, function);
    RequireWritableGuestValue(vectorPtr, function);
    if (vector.size > vector.capacity) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if ((vector.size || vector.capacity) && !vector.buffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (vector.capacity) {
        RequireWritableGuestBytes(vector.buffer, static_cast<std::size_t>(vector.capacity) * sizeof(ContentType), function);
    }
    for (unsigned i = 0; i < vector.size; ++i) {
        StringImpl* const impl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(vector.buffer) + i, function);
        if (impl) {
            static_cast<void>(ReadGuestValue(impl, function));
            RequireWritableGuestValue(impl, function);
        }
    }
}

void ReserveMediaVector(WtfAbi::Vector<ContentType>& vector, std::size_t newCapacity) {
    if (newCapacity <= vector.capacity) return;
    if (newCapacity > 0xFFFFFFFFULL) throw std::length_error("WTF vector capacity overflow");
    auto* newBuffer = static_cast<ContentType*>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(ContentType)));
    for (unsigned i = 0; i < vector.size; ++i) {
        new (&newBuffer[i]) ContentType(std::move(vector.buffer[i]));
        vector.buffer[i].~ContentType();
    }
    if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = newBuffer;
    vector.capacity = static_cast<unsigned>(newCapacity);
}

void ShrinkMediaVector(WtfAbi::Vector<ContentType>& vector, unsigned newSize) {
    for (unsigned i = newSize; i < vector.size; ++i) vector.buffer[i].~ContentType();
    vector.size = newSize;
}

void ClearMediaVector(WtfAbi::Vector<ContentType>& vector) {
    ShrinkMediaVector(vector, 0);
    if (vector.buffer) _ZN3WTF8fastFreeEPv(vector.buffer);
    vector.buffer = nullptr;
    vector.capacity = 0;
}

void AppendContentType(WtfAbi::Vector<ContentType>& vector, String&& type) {
    if (vector.size == vector.capacity) {
        const std::size_t expanded = std::max<std::size_t>(std::size_t{vector.size} + 1, std::max<std::size_t>(kMinVectorCapacity, std::size_t{vector.capacity} + vector.capacity / 4 + 1));
        ReserveMediaVector(vector, expanded);
    }
    new (&vector.buffer[vector.size]) ContentType(std::move(type));
    ++vector.size;
}

std::size_t FindCodeUnit(const StringView& view, char16_t value, unsigned start) {
    const bool is8Bit = view.is8Bit;
    const auto* data8 = static_cast<const unsigned char*>(view.data);
    const auto* data16 = static_cast<const char16_t*>(view.data);
    for (unsigned i = start; i < view.length; ++i) {
        const char16_t unit = is8Bit ? static_cast<char16_t>(data8[i]) : data16[i];
        if (unit == value) return i;
    }
    return kNotFound;
}

String MakeStringFromView(const StringView& view, unsigned start, unsigned length) {
    if (view.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(view.data) + start, length);
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(view.data) + start, length);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore12SettingsBase18setFixedFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kFixedFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase18setSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kSerifFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase20setCursiveFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kCursiveFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase20setFantasyFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kFantasyFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase21setStandardFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kStandardFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase22setSansSerifFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kSansSerifFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase23setPictographFontFamilyERKN3WTF10AtomStringE11UScriptCode(void* self, const AtomString* family, int script) {
    SetFamilyOf(CheckedSelf(self), kPictographFontFamilyMapOffset, family, script, __func__);
}

void APS5_VABI _ZN7WebCore12SettingsBase26setMinimumDOMTimerIntervalEN3WTF7SecondsE(void* self, WTF::Seconds interval) {
    CheckedSelf(self);
    void* const page = ReadGuestValue(reinterpret_cast<void* const*>(static_cast<const unsigned char*>(self) + kPageOffset), __func__);
    if (page) NotImplemented_nid_no_patch(__func__);
    auto* intervalPtr = reinterpret_cast<double*>(static_cast<unsigned char*>(self) + kMinimumDOMTimerIntervalOffset);
    const double previous = ReadGuestValue(intervalPtr, __func__);
    RequireWritableGuestValue(intervalPtr, __func__);
    if (!TryPublishGuestValue(intervalPtr, interval.m_value, previous))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(void* self, const String* contentTypes) {
    CheckedSelf(self);
    if (!contentTypes) throw std::invalid_argument("null WTF::String");
    StringImpl* const impl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(contentTypes), __func__);
    const GuestString source = ReadGuestString(impl, __func__);
    ValidateTargetMediaVector(self, __func__);
    const StringView view = source.View();
    std::vector<String> parsed;
    unsigned start = 0;
    while (true) {
        const std::size_t separator = FindCodeUnit(view, ':', start);
        const unsigned end = separator == kNotFound ? view.length : static_cast<unsigned>(separator);
        if (end > start) parsed.push_back(MakeStringFromView(view, start, end - start));
        if (separator == kNotFound) break;
        start = static_cast<unsigned>(separator) + 1;
    }
    auto& vector = MediaVectorOf(self);
    ShrinkMediaVector(vector, 0);
    for (String& item : parsed) AppendContentType(vector, std::move(item));
}

void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(void* self, const WtfAbi::Vector<ContentType>* contentTypes) {
    CheckedSelf(self);
    if (!contentTypes) throw std::invalid_argument("null WTF::Vector");
    const RawVector source = ReadGuestValue(reinterpret_cast<const RawVector*>(contentTypes), __func__);
    if (source.size > source.capacity || (source.size && !source.buffer))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const void* const targetAddress = static_cast<const unsigned char*>(self) + kMediaContentTypesRequiringHardwareSupportOffset;
    if (static_cast<const void*>(contentTypes) == targetAddress) return;
    ValidateTargetMediaVector(self, __func__);
    if (source.size) {
        RequireReadableGuestBytes(source.buffer, static_cast<std::size_t>(source.size) * sizeof(ContentType), __func__);
    }
    std::vector<StringImpl*> sourceImpls(source.size);
    for (unsigned i = 0; i < source.size; ++i) {
        StringImpl* const impl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(source.buffer) + i, __func__);
        if (impl) {
            const StringImpl header = ReadGuestValue(impl, __func__);
            RequireWritableGuestValue(impl, __func__);
            if (header.length) {
                if (!header.data) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
                const std::size_t byteLength = static_cast<std::size_t>(header.length) * ((header.hashAndFlags & 4u) ? 1u : sizeof(char16_t));
                RequireReadableGuestBytes(header.data, byteLength, __func__);
            }
        }
        sourceImpls[i] = impl;
    }
    std::vector<String> copies;
    copies.reserve(source.size);
    for (StringImpl* impl : sourceImpls) {
        if (impl) impl->refCount += 2;
        copies.emplace_back(impl);
    }
    auto& target = MediaVectorOf(self);
    if (target.size > source.size) {
        ShrinkMediaVector(target, source.size);
    } else if (source.size > target.capacity) {
        ClearMediaVector(target);
        ReserveMediaVector(target, source.size);
    }
    for (unsigned i = 0; i < target.size; ++i) target.buffer[i].type = std::move(copies[i]);
    for (unsigned i = target.size; i < source.size; ++i) new (&target.buffer[i]) ContentType(std::move(copies[i]));
    target.size = source.size;
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kFixedFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kSerifFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kCursiveFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kFantasyFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kStandardFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kSansSerifFontFamilyMapOffset, script, __func__);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kPictographFontFamilyMapOffset, script, __func__);
}

}
