// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/SecurityOriginData.cpp
/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/text/WTFString.cpp
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/text/StringToIntegerConversion.h
/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/text/StringImpl.cpp
/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacySecurityOriginTupleExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceIcu/Properties.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::LegacyOptionalSecurityOriginTuple;
using NKWebKit::WebCoreAbi::LegacyOriginPort;
using NKWebKit::WebCoreAbi::LegacySecurityOriginTuple;
using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;

static_assert(sizeof(bool) == 1);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr std::int32_t kWhiteSpaceNeutral = 9;
constexpr char16_t kSeparator = u'_';
constexpr unsigned kNotFound = ~0U;

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
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
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, size);
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, size);
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

struct HostStringSnapshot {
    bool isNull = true;
    bool is8Bit = true;
    std::vector<unsigned char> narrow;
    std::vector<char16_t> wide;

    StringView View() const {
        if (isNull) return {nullptr, 0, true};
        if (is8Bit) return {narrow.empty() ? nullptr : narrow.data(), static_cast<unsigned>(narrow.size()), true};
        return {wide.empty() ? nullptr : wide.data(), static_cast<unsigned>(wide.size()), false};
    }
};

HostStringSnapshot CopyGuestString(const String* source, const char* function) {
    StringImpl* const impl = ReadGuestValue(reinterpret_cast<StringImpl* const*>(source), function);
    HostStringSnapshot snapshot{};
    if (!impl) return snapshot;
    const StringImpl header = ReadGuestValue(impl, function);
    snapshot.isNull = false;
    snapshot.is8Bit = (header.hashAndFlags & 4u) != 0;
    if (!header.length) return snapshot;
    if (!header.data) throw std::invalid_argument("null WTF character buffer");
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (snapshot.is8Bit ? 1u : sizeof(char16_t));
    RequireReadableGuestBytes(header.data, byteLength, function);
    if (snapshot.is8Bit) {
        snapshot.narrow.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(snapshot.narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    } else {
        snapshot.wide.resize(header.length);
        if (!GuestCopyIn_nid_no_patch(snapshot.wide.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return snapshot;
}

struct StringImplGuard {
    StringImpl* ptr = nullptr;

    ~StringImplGuard() {
        if (!ptr) return;
        try {
            NKWebKit::WtfAbi::Release(ptr);
        } catch (...) {
        }
    }
};

bool IsSpaceOrNewline(char16_t c) {
    if (c <= 0xFF) return c == u' ' || (c <= 0xD && c >= 0x9);
    return u_charDirection_nid_postfix(c) == kWhiteSpaceNeutral;
}

bool IsDigit(char16_t c) {
    return c >= u'0' && c <= u'9';
}

int ToInt(const StringView& view, unsigned start, bool& ok) {
    ok = false;
    unsigned end = start;
    while (end != view.length && IsSpaceOrNewline(CodeUnitAt(view, end))) ++end;
    if (end != view.length && (CodeUnitAt(view, end) == u'+' || CodeUnitAt(view, end) == u'-')) ++end;
    while (end != view.length && IsDigit(CodeUnitAt(view, end))) ++end;

    unsigned position = start;
    while (position != end && IsSpaceOrNewline(CodeUnitAt(view, position))) ++position;
    bool negative = false;
    if (position != end && CodeUnitAt(view, position) == u'-') {
        ++position;
        negative = true;
    } else if (position != end && CodeUnitAt(view, position) == u'+') {
        ++position;
    }
    if (position == end || !IsDigit(CodeUnitAt(view, position))) return 0;
    constexpr std::uint32_t max = std::numeric_limits<int>::max();
    std::uint32_t value = 0;
    for (; position != end; ++position) {
        const std::uint32_t digit = CodeUnitAt(view, position) - u'0';
        if (value > max / 10 || (value == max / 10 && digit > max % 10 + (negative ? 1U : 0U))) return 0;
        value = value * 10 + digit;
    }
    ok = true;
    return static_cast<int>(negative ? 0U - value : value);
}

StringImpl* Create(const StringView& view, unsigned start, unsigned length) {
    void* characters = nullptr;
    StringImpl* impl = NKWebKit::WtfAbi::CreateUninitialized(length, view.is8Bit, characters);
    for (unsigned index = 0; index < length; ++index) {
        const char16_t unit = CodeUnitAt(view, start + index);
        if (view.is8Bit) static_cast<unsigned char*>(characters)[index] = static_cast<unsigned char>(unit);
        else static_cast<char16_t*>(characters)[index] = unit;
    }
    return impl;
}

void PublishDisengaged(LegacyOptionalSecurityOriginTuple* result, const char* function) {
    const unsigned char engaged = 0;
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(result) + offsetof(LegacyOptionalSecurityOriginTuple, engaged), &engaged, sizeof(engaged)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

}  // namespace

extern "C" {

LegacyOptionalSecurityOriginTuple* APS5_VABI _ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(
    LegacyOptionalSecurityOriginTuple* result, const String* databaseIdentifier) {
    if (!result) throw std::invalid_argument("WebCore::SecurityOriginData::fromDatabaseIdentifier: null result");
    if (!databaseIdentifier) throw std::invalid_argument("WebCore::SecurityOriginData::fromDatabaseIdentifier: null WTF::String");
    RequireReadableGuestBytes(result, sizeof(LegacyOptionalSecurityOriginTuple), __func__);
    RequireWritableGuestBytes(result, sizeof(LegacyOptionalSecurityOriginTuple), __func__);
    unsigned char previousRaw[sizeof(LegacyOptionalSecurityOriginTuple)]{};
    if (!GuestCopyIn_nid_no_patch(previousRaw, result, sizeof(previousRaw)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const HostStringSnapshot identifier = CopyGuestString(databaseIdentifier, __func__);
    if (identifier.isNull) {
        PublishDisengaged(result, __func__);
        return result;
    }
    const StringView view = identifier.View();
    unsigned first = kNotFound;
    unsigned last = kNotFound;
    for (unsigned index = 0; index < view.length; ++index) {
        if (CodeUnitAt(view, index) != kSeparator) continue;
        if (first == kNotFound) first = index;
        last = index;
    }
    if (first == kNotFound || first == last) {
        PublishDisengaged(result, __func__);
        return result;
    }
    bool portOkay = false;
    const int port = ToInt(view, last + 1, portOkay);
    const bool portAbsent = last == view.length - 1;
    if (!portOkay && !portAbsent) {
        PublishDisengaged(result, __func__);
        return result;
    }
    if (port < 0 || port > std::numeric_limits<std::uint16_t>::max()) {
        PublishDisengaged(result, __func__);
        return result;
    }
    StringImplGuard protocol{Create(view, 0, first)};
    StringImplGuard host{Create(view, first + 1, last - first - 1)};
    unsigned char updatedRaw[sizeof(LegacyOptionalSecurityOriginTuple)]{};
    std::memcpy(updatedRaw, previousRaw, sizeof(updatedRaw));
    const unsigned char engagedTrue = 1;
    const unsigned char portEngaged = port != 0 ? 1u : 0u;
    const auto portValue = static_cast<std::uint16_t>(port);
    constexpr std::size_t kValueOffset = offsetof(LegacyOptionalSecurityOriginTuple, value);
    std::memcpy(updatedRaw + offsetof(LegacyOptionalSecurityOriginTuple, engaged), &engagedTrue, sizeof(engagedTrue));
    std::memcpy(updatedRaw + kValueOffset + offsetof(LegacySecurityOriginTuple, protocol), &protocol.ptr, sizeof(protocol.ptr));
    std::memcpy(updatedRaw + kValueOffset + offsetof(LegacySecurityOriginTuple, host), &host.ptr, sizeof(host.ptr));
    std::memcpy(updatedRaw + kValueOffset + offsetof(LegacySecurityOriginTuple, port) + offsetof(LegacyOriginPort, engaged), &portEngaged, sizeof(portEngaged));
    std::memcpy(updatedRaw + kValueOffset + offsetof(LegacySecurityOriginTuple, port) + offsetof(LegacyOriginPort, value), &portValue, sizeof(portValue));
    if (!GuestCopyOut_nid_no_patch(result, updatedRaw, sizeof(updatedRaw))) {
        static_cast<void>(GuestCopyOut_nid_no_patch(result, previousRaw, sizeof(previousRaw)));
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    protocol.ptr = nullptr;
    host.ptr = nullptr;
    return result;
}

}
