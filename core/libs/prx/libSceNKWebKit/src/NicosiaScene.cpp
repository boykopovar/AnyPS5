// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/nicosia/NicosiaScene.cpp
/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/nicosia/NicosiaSceneIntegration.cpp
/*
 * Copyright (C) 2019 Metrological Group B.V.
 * Copyright (C) 2019 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/HashTable.h
/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/HashFunctions.h
/*
 * Copyright (C) 2005-2019 Apple Inc. All rights reserved.
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/ThreadSafeRefCounted.h
/*
 * Copyright (C) 2007, 2008, 2010, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/NicosiaSceneExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

using NKWebKit::NicosiaAbi::PlatformLayer;
using NKWebKit::NicosiaAbi::Scene;
using NKWebKit::NicosiaAbi::SceneIntegration;
using NKWebKit::NicosiaAbi::SceneIntegrationClient;
using NKWebKit::NicosiaAbi::SceneIntegrationRef;
using NKWebKit::NicosiaAbi::SceneRef;
using NKWebKit::NicosiaAbi::SceneState;

constexpr std::uint8_t kLockHeldBit = 1;
constexpr unsigned kMinimumTableSize = 8;
constexpr unsigned kMaxSmallTableCapacity = 1024;
constexpr int kTableSize = -1;
constexpr int kTableSizeMask = -2;
constexpr int kKeyCount = -3;
constexpr std::size_t kMetadataSize = 4 * sizeof(unsigned);

class Locker {
public:
    explicit Locker(std::uint8_t& byte)
        : m_byte(byte) {
        std::uint8_t expected = 0;
        while (!m_byte.compare_exchange_weak(expected, kLockHeldBit, std::memory_order_acquire)) {
            if (expected & ~kLockHeldBit) throw std::runtime_error("Nicosia::SceneIntegration: the lock has parked threads");
            expected = 0;
            std::this_thread::yield();
        }
    }

    ~Locker() {
        m_byte.fetch_and(static_cast<std::uint8_t>(~kLockHeldBit), std::memory_order_release);
    }

    Locker(const Locker&) = delete;
    Locker& operator=(const Locker&) = delete;

private:
    std::atomic_ref<std::uint8_t> m_byte;
};

template<typename T>
T& Checked(T* object, const char* message) {
    if (!object) throw std::invalid_argument(message);
    return *object;
}

unsigned* Metadata(PlatformLayer** table) {
    return reinterpret_cast<unsigned*>(table);
}

bool IsLive(const PlatformLayer* layer) {
    return layer && reinterpret_cast<std::uintptr_t>(layer) != ~std::uintptr_t{0};
}

void Ref(PlatformLayer* layer) {
    std::atomic_ref<unsigned>(layer->refCount).fetch_add(1);
}

void Deref(PlatformLayer* layer) {
    std::atomic_ref<unsigned> refCount(layer->refCount);
    if (refCount.fetch_sub(1) != 1) return;
    refCount.store(1);
    layer->vtable->destroyAndFree(layer);
}

unsigned PointerHash(const PlatformLayer* layer) {
    std::uint64_t key = reinterpret_cast<std::uintptr_t>(layer);
    key += ~(key << 32);
    key ^= key >> 22;
    key += ~(key << 13);
    key ^= key >> 8;
    key += key << 3;
    key ^= key >> 15;
    key += ~(key << 27);
    key ^= key >> 31;
    return static_cast<unsigned>(key);
}

unsigned DoubleHash(unsigned key) {
    key = ~key + (key >> 23);
    key ^= key << 12;
    key ^= key >> 7;
    key ^= key << 2;
    key ^= key >> 20;
    return key;
}

bool ShouldExpand(std::uint64_t keyCount, std::uint64_t tableSize) {
    if (tableSize <= kMaxSmallTableCapacity) return keyCount * 4 >= tableSize * 3;
    return keyCount * 2 >= tableSize;
}

unsigned BestTableSize(unsigned keyCount) {
    unsigned size = keyCount - 1;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    ++size;
    if (ShouldExpand(keyCount, size)) size *= 2;
    const double maxLoad = size <= kMaxSmallTableCapacity ? 3.0 / 4.0 : 1.0 / 2.0;
    const double minLoad = 1.0 / 6.0;
    const double eagerLoad = ((maxLoad + minLoad) / 2 + maxLoad) / 2;
    if (keyCount >= size * eagerLoad) size *= 2;
    return size < kMinimumTableSize ? kMinimumTableSize : size;
}

PlatformLayer** CopyTable(PlatformLayer** source) {
    if (!source) return nullptr;
    const unsigned keyCount = Metadata(source)[kKeyCount];
    if (!keyCount) return nullptr;
    const unsigned size = BestTableSize(keyCount);
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(kMetadataSize + std::size_t(size) * sizeof(PlatformLayer*)));
    auto** table = reinterpret_cast<PlatformLayer**>(storage + kMetadataSize);
    Metadata(table)[kTableSize] = size;
    Metadata(table)[kTableSizeMask] = size - 1;
    Metadata(table)[kKeyCount] = keyCount;
    const unsigned mask = size - 1;
    const unsigned sourceSize = Metadata(source)[kTableSize];
    for (unsigned index = 0; index < sourceSize; ++index) {
        PlatformLayer* layer = source[index];
        if (!IsLive(layer)) continue;
        const unsigned hash = PointerHash(layer);
        unsigned slot = hash & mask;
        unsigned step = 0;
        while (table[slot]) {
            if (!step) step = 1 | DoubleHash(hash);
            slot = (slot + step) & mask;
        }
        Ref(layer);
        table[slot] = layer;
    }
    return table;
}

void FreeTable(PlatformLayer** table) {
    if (!table) return;
    const unsigned size = Metadata(table)[kTableSize];
    for (unsigned index = 0; index < size; ++index)
        if (IsLive(table[index])) Deref(table[index]);
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - kMetadataSize);
}

void ConstructState(SceneState& state) {
    state.id = 0;
    state.platformLayerUpdated = false;
    state.layers = nullptr;
    state.rootLayer = nullptr;
}

void CopyState(SceneState& state, const SceneState& other) {
    state.id = other.id;
    state.platformLayerUpdated = other.platformLayerUpdated;
    state.layers = CopyTable(other.layers);
    state.rootLayer = other.rootLayer;
    if (state.rootLayer) Ref(state.rootLayer);
}

void DestroyState(SceneState& state) {
    if (PlatformLayer* rootLayer = std::exchange(state.rootLayer, nullptr)) Deref(rootLayer);
    FreeTable(state.layers);
}

void ConstructScene(Scene& scene) {
    scene.refCount = 1;
    scene.scene.lock = 0;
    ConstructState(scene.scene.state);
}

void DerefScene(Scene* scene) {
    std::atomic_ref<unsigned> refCount(scene->refCount);
    if (refCount.fetch_sub(1) != 1) return;
    refCount.store(1);
    DestroyState(scene->scene.state);
    _ZN3WTF8fastFreeEPv(scene);
}

void ConstructIntegration(SceneIntegration* self, Scene* scene, SceneIntegrationClient* client) {
    SceneIntegration& integration = Checked(self, "Nicosia::SceneIntegration::SceneIntegration: null this");
    Scene& target = Checked(scene, "Nicosia::SceneIntegration::SceneIntegration: null scene");
    Checked(client, "Nicosia::SceneIntegration::SceneIntegration: null client");
    integration.refCount = 1;
    integration.client.lock = 0;
    std::atomic_ref<unsigned>(target.refCount).fetch_add(1);
    integration.client.scene = scene;
    integration.client.object = client;
}

void DestroyIntegration(SceneIntegration* self) {
    SceneIntegration& integration = Checked(self, "Nicosia::SceneIntegration::~SceneIntegration: null this");
    if (Scene* scene = std::exchange(integration.client.scene, nullptr)) DerefScene(scene);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7Nicosia5Scene5StateC1Ev(SceneState* self) {
    ConstructState(Checked(self, "Nicosia::Scene::State::State: null this"));
}

void APS5_VABI _ZN7Nicosia5Scene5StateC2Ev(SceneState* self) {
    ConstructState(Checked(self, "Nicosia::Scene::State::State: null this"));
}

void APS5_VABI _ZN7Nicosia5Scene5StateC1ERKS1_(SceneState* self, const SceneState* other) {
    CopyState(Checked(self, "Nicosia::Scene::State::State: null this"), Checked(other, "Nicosia::Scene::State::State: null source"));
}

void APS5_VABI _ZN7Nicosia5Scene5StateC2ERKS1_(SceneState* self, const SceneState* other) {
    CopyState(Checked(self, "Nicosia::Scene::State::State: null this"), Checked(other, "Nicosia::Scene::State::State: null source"));
}

void APS5_VABI _ZN7Nicosia5Scene5StateD1Ev(SceneState* self) {
    DestroyState(Checked(self, "Nicosia::Scene::State::~State: null this"));
}

void APS5_VABI _ZN7Nicosia5Scene5StateD2Ev(SceneState* self) {
    DestroyState(Checked(self, "Nicosia::Scene::State::~State: null this"));
}

SceneState* APS5_VABI _ZN7Nicosia5Scene5StateaSERKS1_(SceneState* self, const SceneState* other) {
    SceneState& state = Checked(self, "Nicosia::Scene::State::operator=: null this");
    const SceneState& source = Checked(other, "Nicosia::Scene::State::operator=: null source");
    state.id = source.id;
    state.platformLayerUpdated = source.platformLayerUpdated;
    PlatformLayer** layers = CopyTable(source.layers);
    FreeTable(std::exchange(state.layers, layers));
    PlatformLayer* rootLayer = source.rootLayer;
    if (rootLayer) Ref(rootLayer);
    if (PlatformLayer* previous = std::exchange(state.rootLayer, rootLayer)) Deref(previous);
    return self;
}

void APS5_VABI _ZN7Nicosia5SceneC1Ev(Scene* self) {
    ConstructScene(Checked(self, "Nicosia::Scene::Scene: null this"));
}

void APS5_VABI _ZN7Nicosia5SceneC2Ev(Scene* self) {
    ConstructScene(Checked(self, "Nicosia::Scene::Scene: null this"));
}

void APS5_VABI _ZN7Nicosia5SceneD1Ev(Scene* self) {
    DestroyState(Checked(self, "Nicosia::Scene::~Scene: null this").scene.state);
}

void APS5_VABI _ZN7Nicosia5SceneD2Ev(Scene* self) {
    DestroyState(Checked(self, "Nicosia::Scene::~Scene: null this").scene.state);
}

SceneRef* APS5_VABI _ZN7Nicosia5Scene6createEv(SceneRef* result) {
    SceneRef& reference = Checked(result, "Nicosia::Scene::create: null result");
    auto* scene = static_cast<Scene*>(_ZN3WTF10fastMallocEm(sizeof(Scene)));
    ConstructScene(*scene);
    reference.ptr = scene;
    return result;
}

void APS5_VABI _ZN7Nicosia16SceneIntegrationC1ERNS_5SceneERNS0_6ClientE(SceneIntegration* self, Scene* scene, SceneIntegrationClient* client) {
    ConstructIntegration(self, scene, client);
}

void APS5_VABI _ZN7Nicosia16SceneIntegrationC2ERNS_5SceneERNS0_6ClientE(SceneIntegration* self, Scene* scene, SceneIntegrationClient* client) {
    ConstructIntegration(self, scene, client);
}

void APS5_VABI _ZN7Nicosia16SceneIntegrationD1Ev(SceneIntegration* self) {
    DestroyIntegration(self);
}

void APS5_VABI _ZN7Nicosia16SceneIntegrationD2Ev(SceneIntegration* self) {
    DestroyIntegration(self);
}

SceneIntegrationRef* APS5_VABI _ZN7Nicosia16SceneIntegration6createERNS_5SceneERNS0_6ClientE(SceneIntegrationRef* result, Scene* scene, SceneIntegrationClient* client) {
    SceneIntegrationRef& reference = Checked(result, "Nicosia::SceneIntegration::create: null result");
    Checked(scene, "Nicosia::SceneIntegration::create: null scene");
    Checked(client, "Nicosia::SceneIntegration::create: null client");
    auto* integration = static_cast<SceneIntegration*>(_ZN3WTF10fastMallocEm(sizeof(SceneIntegration)));
    ConstructIntegration(integration, scene, client);
    reference.ptr = integration;
    return result;
}

void APS5_VABI _ZN7Nicosia16SceneIntegration9setClientERNS0_6ClientE(SceneIntegration* self, SceneIntegrationClient* client) {
    SceneIntegration& integration = Checked(self, "Nicosia::SceneIntegration::setClient: null this");
    Checked(client, "Nicosia::SceneIntegration::setClient: null client");
    Locker locker(integration.client.lock);
    integration.client.object = client;
}

void APS5_VABI _ZN7Nicosia16SceneIntegration10invalidateEv(SceneIntegration* self) {
    SceneIntegration& integration = Checked(self, "Nicosia::SceneIntegration::invalidate: null this");
    Locker locker(integration.client.lock);
    if (Scene* scene = std::exchange(integration.client.scene, nullptr)) DerefScene(scene);
    integration.client.object = nullptr;
}

void APS5_VABI _ZN7Nicosia16SceneIntegration13requestUpdateEv(SceneIntegration* self) {
    SceneIntegration& integration = Checked(self, "Nicosia::SceneIntegration::requestUpdate: null this");
    Locker locker(integration.client.lock);
    if (SceneIntegrationClient* client = integration.client.object) client->vtable->requestUpdate(client);
}

}
