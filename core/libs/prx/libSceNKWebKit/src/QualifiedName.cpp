/*
 * Copyright (C) 2005, 2006, 2009 Apple Inc. All rights reserved.
 *
 * Derived from Source/WebCore/dom/QualifiedName.cpp@c55364d53f971f63a709976f168d932d558b8909
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

/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
 *
 * Derived from Source/WebCore/dom/QualifiedNameCache.cpp@c55364d53f971f63a709976f168d932d558b8909
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
 */

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/QualifiedNameExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace {

using NKWebKit::WtfAbi::AtomString;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::QualifiedName;
using WebCore::QualifiedNameImpl;

static_assert(sizeof(AtomString) == sizeof(StringImpl*));

template <typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    const int savedErrno = errno;
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
bool TryWriteGuestValue(T* pointer, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (TryWriteGuestValue(pointer, value)) return true;
    static_cast<void>(TryWriteGuestValue(pointer, previous));
    return false;
}

void RequireWritableStringImpl(StringImpl* impl, const char* function) {
    if (!impl) return;
    static_cast<void>(ReadGuestValue(impl, function));
    RequireWritableGuestValue(impl, function);
}

StringImpl* ReadAtomStringImpl(const AtomString* atom, const char* function) {
    return ReadGuestValue(reinterpret_cast<StringImpl* const*>(atom), function);
}

struct Components {
    StringImpl* prefix;
    StringImpl* localName;
    StringImpl* namespaceURI;

    bool operator==(const Components&) const = default;
};

struct ComponentsHash {
    std::size_t operator()(const Components& components) const noexcept {
        const std::hash<const void*> hash;
        std::size_t result = hash(components.prefix);
        result = result * 31 + hash(components.localName);
        return result * 31 + hash(components.namespaceURI);
    }
};

struct Cache {
    std::mutex mutex;
    std::unordered_map<Components, QualifiedNameImpl*, ComponentsHash> entries;
};

Cache& QualifiedNameCache() {
    static Cache* const cache = new Cache;
    return *cache;
}

bool TryAdjustRefCount(StringImpl* impl, int delta) {
    if (!impl) return true;
    unsigned count = 0;
    const int savedErrno = errno;
    bool ok = GuestCopyIn_nid_no_patch(&count, &impl->refCount, sizeof(count));
    if (ok) {
        count = static_cast<unsigned>(static_cast<int>(count) + delta);
        ok = GuestCopyOut_nid_no_patch(&impl->refCount, &count, sizeof(count));
    }
    errno = savedErrno;
    return ok;
}

void RetainAll(std::array<StringImpl*, 3> impls, const char* function) {
    for (std::size_t index = 0; index < impls.size(); ++index) {
        if (TryAdjustRefCount(impls[index], 2)) continue;
        for (std::size_t undone = 0; undone < index; ++undone) static_cast<void>(TryAdjustRefCount(impls[undone], -2));
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

StringImpl* NamespaceImpl(const AtomString* namespaceURI, const char* function) {
    StringImpl* impl = ReadAtomStringImpl(namespaceURI, function);
    if (!impl) return nullptr;
    const auto header = ReadGuestValue(impl, function);
    return header.length ? impl : nullptr;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(QualifiedName* self, const AtomString* prefix, const AtomString* localName, const AtomString* namespaceURI) {
    if (!self || !prefix || !localName || !namespaceURI)
        throw std::invalid_argument("QualifiedName requires a target and three atom strings");

    const QualifiedName previous = ReadGuestValue(self, __func__);
    RequireWritableGuestValue(self, __func__);
    const Components key {
        ReadAtomStringImpl(prefix, __func__),
        ReadAtomStringImpl(localName, __func__),
        NamespaceImpl(namespaceURI, __func__)
    };

    Cache& cache = QualifiedNameCache();
    std::lock_guard lock(cache.mutex);

    const auto existing = cache.entries.find(key);
    if (existing != cache.entries.end() && existing->second->m_refCount) {
        ++existing->second->m_refCount;
        const QualifiedName published { existing->second };
        if (!TryPublishGuestValue(self, published, previous)) {
            --existing->second->m_refCount;
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        return;
    }

    RequireWritableStringImpl(key.prefix, __func__);
    RequireWritableStringImpl(key.localName, __func__);
    RequireWritableStringImpl(key.namespaceURI, __func__);

    RetainAll({key.prefix, key.localName, key.namespaceURI}, __func__);
    auto* impl = static_cast<QualifiedNameImpl*>(_ZN3WTF10fastMallocEm(sizeof(QualifiedNameImpl)));
    *impl = QualifiedNameImpl { 1, 0, key.prefix, key.localName, key.namespaceURI, nullptr };
    const QualifiedName published { impl };
    if (!TryPublishGuestValue(self, published, previous)) {
        static_cast<void>(TryAdjustRefCount(key.namespaceURI, -2));
        static_cast<void>(TryAdjustRefCount(key.localName, -2));
        static_cast<void>(TryAdjustRefCount(key.prefix, -2));
        _ZN3WTF8fastFreeEPv(impl);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    cache.entries[key] = impl;
}

void APS5_VABI _ZN7WebCore13QualifiedNameC2ERKN3WTF10AtomStringES4_S4_(QualifiedName* self, const AtomString* prefix, const AtomString* localName, const AtomString* namespaceURI) {
    _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(self, prefix, localName, namespaceURI);
}

void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(QualifiedNameImpl* self) {
    if (!self) throw std::invalid_argument("QualifiedNameImpl destructor requires an object");

    const QualifiedNameImpl local = ReadGuestValue(self, __func__);
    RequireWritableStringImpl(local.m_localNameUpper, __func__);
    RequireWritableStringImpl(local.m_namespace, __func__);
    RequireWritableStringImpl(local.m_localName, __func__);
    RequireWritableStringImpl(local.m_prefix, __func__);

    {
        Cache& cache = QualifiedNameCache();
        std::lock_guard lock(cache.mutex);
        const auto entry = cache.entries.find(Components { local.m_prefix, local.m_localName, local.m_namespace });
        if (entry != cache.entries.end() && entry->second == self) cache.entries.erase(entry);
    }

    NKWebKit::WtfAbi::Release(local.m_localNameUpper);
    NKWebKit::WtfAbi::Release(local.m_namespace);
    NKWebKit::WtfAbi::Release(local.m_localName);
    NKWebKit::WtfAbi::Release(local.m_prefix);
}

void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD2Ev(QualifiedNameImpl* self) {
    _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(self);
}

}
