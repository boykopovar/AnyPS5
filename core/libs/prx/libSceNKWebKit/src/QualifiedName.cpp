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

#include "prx/libSceNKWebKit/include/QualifiedNameExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

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

StringImpl* Retain(StringImpl* impl) {
    if (impl) impl->refCount += 2;
    return impl;
}

StringImpl* NamespaceImpl(const AtomString& namespaceURI) {
    StringImpl* impl = namespaceURI.string.impl;
    return impl && impl->length ? impl : nullptr;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(QualifiedName* self, const AtomString* prefix, const AtomString* localName, const AtomString* namespaceURI) {
    if (!self || !prefix || !localName || !namespaceURI)
        throw std::invalid_argument("QualifiedName requires a target and three atom strings");

    const Components key { prefix->string.impl, localName->string.impl, NamespaceImpl(*namespaceURI) };
    Cache& cache = QualifiedNameCache();
    std::lock_guard lock(cache.mutex);

    auto [entry, inserted] = cache.entries.try_emplace(key, nullptr);
    if (!inserted && entry->second->m_refCount) {
        ++entry->second->m_refCount;
        self->m_impl = entry->second;
        return;
    }

    auto* impl = static_cast<QualifiedNameImpl*>(_ZN3WTF10fastMallocEm(sizeof(QualifiedNameImpl)));
    *impl = QualifiedNameImpl { 1, 0, Retain(key.prefix), Retain(key.localName), Retain(key.namespaceURI), nullptr };
    entry->second = impl;
    self->m_impl = impl;
}

void APS5_VABI _ZN7WebCore13QualifiedNameC2ERKN3WTF10AtomStringES4_S4_(QualifiedName* self, const AtomString* prefix, const AtomString* localName, const AtomString* namespaceURI) {
    _ZN7WebCore13QualifiedNameC1ERKN3WTF10AtomStringES4_S4_(self, prefix, localName, namespaceURI);
}

void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(QualifiedNameImpl* self) {
    if (!self) throw std::invalid_argument("QualifiedNameImpl destructor requires an object");
    
    {
        Cache& cache = QualifiedNameCache();
        std::lock_guard lock(cache.mutex);
        const auto entry = cache.entries.find(Components { self->m_prefix, self->m_localName, self->m_namespace });
        if (entry != cache.entries.end() && entry->second == self) cache.entries.erase(entry);
    }

    NKWebKit::WtfAbi::Release(self->m_localNameUpper);
    NKWebKit::WtfAbi::Release(self->m_namespace);
    NKWebKit::WtfAbi::Release(self->m_localName);
    NKWebKit::WtfAbi::Release(self->m_prefix);
}

void APS5_VABI _ZN7WebCore13QualifiedName17QualifiedNameImplD2Ev(QualifiedNameImpl* self) {
    _ZN7WebCore13QualifiedName17QualifiedNameImplD1Ev(self);
}

}