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

#include "prx/libSceNKWebKit/include/SettingsBaseExports.hpp"

#include "prx/libSceNKWebKit/src/SettingsBaseAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfLanguageAbi.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

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

struct GuestMap {
    ScriptFontFamilyEntry* table;
};

static_assert(sizeof(GuestMap) == 8);
static_assert(alignof(GuestMap) == 8);

void* CheckedSelf(void* self) {
    if (!self) throw std::invalid_argument("null WebCore::SettingsBase");
    return self;
}

using FontGenericFamiliesGetter = void* (APS5_VABI*)(void*);

void* FontGenericFamiliesOf(void* self, bool constant) {
    CheckedSelf(self);
    void* const* entries = *reinterpret_cast<void* const**>(self);
    auto getter = reinterpret_cast<FontGenericFamiliesGetter>(entries[constant ? 1 : 0]);
    return getter(self);
}

unsigned TableSizeMask(const ScriptFontFamilyEntry* table) {
    return reinterpret_cast<const unsigned*>(table)[-kTableSizeMaskSlot];
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

bool IsEmptyBucket(const ScriptFontFamilyEntry& entry) {
    return entry.key == kEmptyKey;
}

ScriptFontFamilyEntry* Lookup(ScriptFontFamilyEntry* table, int script) {
    if (!table) return nullptr;
    const unsigned sizeMask = TableSizeMask(table);
    unsigned i = HashScript(script) & sizeMask;
    unsigned probeCount = 0;
    while (true) {
        ScriptFontFamilyEntry& entry = table[i];
        if (entry.key == script) return &entry;
        if (IsEmptyBucket(entry)) return nullptr;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
}

const String* EmptyFamilyAtom() {
    static const String value(NKWebKit::WtfAbi::EmptyStringImpl());
    return &value;
}

const String* GenericFamilyForScript(ScriptFontFamilyEntry* table, int script) {
    if (ScriptFontFamilyEntry* found = Lookup(table, script)) return &found->value;
    if (script == kUscriptHan) {
        const int fallback = NKWebKit::UserPrefersSimplifiedChinese_nid_no_patch() ? kUscriptSimplifiedHan : kUscriptTraditionalHan;
        if (ScriptFontFamilyEntry* found = Lookup(table, fallback)) return &found->value;
    }
    if (script != kUscriptCommon) return GenericFamilyForScript(table, kUscriptCommon);
    return EmptyFamilyAtom();
}

bool GenericFamilyWouldChange(const GuestMap& map, const String& family, int script) {
    ScriptFontFamilyEntry* entry = Lookup(map.table, script);
    if (!family.impl || !family.impl->length) return entry != nullptr;
    if (!entry) return true;
    return !_ZN3WTF5equalEPKNS_10StringImplES2_(entry->value.impl, family.impl);
}

GuestMap& MapOf(void* families, std::size_t offset) {
    return *reinterpret_cast<GuestMap*>(static_cast<unsigned char*>(families) + offset);
}

const String* FamilyOf(void* self, std::size_t offset, int script) {
    void* families = FontGenericFamiliesOf(const_cast<void*>(self), true);
    return GenericFamilyForScript(MapOf(families, offset).table, script);
}

void SetFamilyOf(void* self, std::size_t offset, const AtomString* family, int script, const char* name) {
    if (!family) throw std::invalid_argument("null WTF::AtomString");
    void* families = FontGenericFamiliesOf(self, false);
    if (GenericFamilyWouldChange(MapOf(families, offset), family->string, script)) NotImplemented_nid_no_patch(name);
}

WtfAbi::Vector<ContentType>& MediaVectorOf(void* self) {
    return *reinterpret_cast<WtfAbi::Vector<ContentType>*>(static_cast<unsigned char*>(self) + kMediaContentTypesRequiringHardwareSupportOffset);
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
    void* page;
    std::memcpy(&page, static_cast<unsigned char*>(self) + kPageOffset, sizeof(page));
    if (page) NotImplemented_nid_no_patch(__func__);
    *reinterpret_cast<double*>(static_cast<unsigned char*>(self) + kMinimumDOMTimerIntervalOffset) = interval.m_value;
}

void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6StringE(void* self, const String* contentTypes) {
    CheckedSelf(self);
    if (!contentTypes) throw std::invalid_argument("null WTF::String");
    auto& vector = MediaVectorOf(self);
    ShrinkMediaVector(vector, 0);
    const StringView view = NKWebKit::WtfAbi::ViewOf(contentTypes->impl);
    unsigned start = 0;
    while (true) {
        const std::size_t separator = FindCodeUnit(view, ':', start);
        const unsigned end = separator == kNotFound ? view.length : static_cast<unsigned>(separator);
        if (end > start) AppendContentType(vector, MakeStringFromView(view, start, end - start));
        if (separator == kNotFound) break;
        start = static_cast<unsigned>(separator) + 1;
    }
}

void APS5_VABI _ZN7WebCore12SettingsBase44setMediaContentTypesRequiringHardwareSupportERKN3WTF6VectorINS_11ContentTypeELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(void* self, const WtfAbi::Vector<ContentType>* contentTypes) {
    CheckedSelf(self);
    if (!contentTypes) throw std::invalid_argument("null WTF::Vector");
    auto& target = MediaVectorOf(self);
    if (static_cast<const void*>(contentTypes) == static_cast<const void*>(&target)) return;
    if (target.size > contentTypes->size) {
        ShrinkMediaVector(target, contentTypes->size);
    } else if (contentTypes->size > target.capacity) {
        ClearMediaVector(target);
        ReserveMediaVector(target, contentTypes->size);
    }
    for (unsigned i = 0; i < target.size; ++i) target.buffer[i].type = contentTypes->buffer[i].type;
    for (unsigned i = target.size; i < contentTypes->size; ++i) new (&target.buffer[i]) ContentType(contentTypes->buffer[i].type);
    target.size = contentTypes->size;
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase15fixedFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kFixedFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase15serifFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kSerifFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase17cursiveFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kCursiveFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase17fantasyFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kFantasyFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase18standardFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kStandardFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase19sansSerifFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kSansSerifFontFamilyMapOffset, script);
}

const String& APS5_VABI _ZNK7WebCore12SettingsBase20pictographFontFamilyE11UScriptCode(const void* self, int script) {
    return *FamilyOf(const_cast<void*>(self), kPictographFontFamilyMapOffset, script);
}

}
