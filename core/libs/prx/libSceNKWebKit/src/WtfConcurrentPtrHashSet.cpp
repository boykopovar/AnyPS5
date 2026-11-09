// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/ConcurrentPtrHashSet.cpp
/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfConcurrentPtrHashSet.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>

namespace {

using Set = WTF::ConcurrentPtrHashSet;
using Table = WTF::ConcurrentPtrHashSetTable;

constexpr std::uint32_t kInitialSize = 32;
constexpr std::uint32_t kStubDefaultLoad = 10;
constexpr std::uint32_t kInlineTables = 4;
constexpr std::size_t kMinTablesCapacity = 16;
constexpr std::uint8_t kLockHeldBit = 1;

class SetLocker {
public:
    explicit SetLocker(Set* set)
        : m_byte(set->lock) {
        std::uint8_t expected = 0;
        while (!m_byte.compare_exchange_weak(expected, kLockHeldBit, std::memory_order_acquire)) {
            if (expected & ~kLockHeldBit)
                throw std::runtime_error("ConcurrentPtrHashSet: lock has parked threads");
            expected = 0;
            std::this_thread::yield();
        }
    }

    ~SetLocker() {
        m_byte.store(0, std::memory_order_release);
    }

    SetLocker(const SetLocker&) = delete;
    SetLocker& operator=(const SetLocker&) = delete;

private:
    std::atomic_ref<std::uint8_t> m_byte;
};

Set* CheckedSet(Set* set) {
    if (!set) throw std::invalid_argument("ConcurrentPtrHashSet: null set");
    return set;
}

unsigned PtrHash(void* ptr) {
    auto key = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(ptr));
    key += ~(key << 32);
    key ^= (key >> 22);
    key += ~(key << 13);
    key ^= (key >> 8);
    key += (key << 3);
    key ^= (key >> 15);
    key += ~(key << 27);
    key ^= (key >> 31);
    return static_cast<unsigned>(key);
}

std::atomic_ref<void*> Slot(Table* table, std::uint32_t index) {
    auto* array = reinterpret_cast<void**>(reinterpret_cast<std::byte*>(table) + offsetof(Table, array));
    return std::atomic_ref<void*>(array[index]);
}

std::atomic_ref<std::uint32_t> Load(Table* table) {
    return std::atomic_ref<std::uint32_t>(table->load);
}

std::atomic_ref<Table*> Current(Set* set) {
    return std::atomic_ref<Table*>(set->table);
}

Table* CreateTable(std::uint32_t size) {
    auto* table = static_cast<Table*>(_ZN3WTF10fastMallocEm(offsetof(Table, array) + sizeof(void*) * std::size_t{size}));
    table->size = size;
    table->mask = size - 1;
    Load(table).store(0, std::memory_order_relaxed);
    for (std::uint32_t i = 0; i < size; ++i) Slot(table, i).store(nullptr, std::memory_order_relaxed);
    return table;
}

void InitializeStub(Table& stub) {
    stub.size = 0;
    stub.mask = 0;
    Load(&stub).store(kStubDefaultLoad, std::memory_order_relaxed);
    Slot(&stub, 0).store(nullptr, std::memory_order_relaxed);
}

void InitializeTables(Set* set) {
    set->tablesBuffer = set->tablesInline;
    set->tablesCapacity = kInlineTables;
    set->tablesSize = 0;
}

void AppendTable(Set* set, Table* table) {
    if (set->tablesSize == set->tablesCapacity) {
        const std::size_t capacity = set->tablesCapacity;
        const auto newCapacity = std::max<std::size_t>(capacity + 1, std::max(kMinTablesCapacity, capacity + capacity / 4 + 1));
        if (newCapacity > std::numeric_limits<std::uint32_t>::max() / sizeof(Table*))
            throw std::length_error("ConcurrentPtrHashSet: table list capacity overflow");
        auto* buffer = static_cast<Table**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(Table*)));
        std::memcpy(buffer, set->tablesBuffer, set->tablesSize * sizeof(Table*));
        if (set->tablesBuffer != set->tablesInline) _ZN3WTF8fastFreeEPv(set->tablesBuffer);
        set->tablesBuffer = buffer;
        set->tablesCapacity = static_cast<std::uint32_t>(newCapacity);
    }
    set->tablesBuffer[set->tablesSize++] = table;
}

void DestroyTables(Set* set) {
    if (set->tablesSize && !set->tablesBuffer) throw std::invalid_argument("ConcurrentPtrHashSet: null table list");
    for (std::uint32_t i = 0; i < set->tablesSize; ++i) _ZN3WTF8fastFreeEPv(set->tablesBuffer[i]);
    set->tablesSize = 0;
    if (set->tablesBuffer != set->tablesInline) _ZN3WTF8fastFreeEPv(set->tablesBuffer);
    InitializeTables(set);
}

void Initialize(Set* set) {
    auto* table = CreateTable(kInitialSize);
    Current(set).store(table, std::memory_order_relaxed);
    AppendTable(set, table);
    InitializeStub(set->stubTable);
}

void Construct(Set* set) {
    InitializeTables(CheckedSet(set));
    set->lock = 0;
    Initialize(set);
}

void Destroy(Set* set) {
    DestroyTables(CheckedSet(set));
}

void ResizeIfNecessary(Set* set) {
    SetLocker locker(set);
    auto* table = Current(set).load(std::memory_order_relaxed);
    if (table == &set->stubTable) throw std::runtime_error("ConcurrentPtrHashSet: stub table installed outside a resize");
    if (Load(table).load(std::memory_order_relaxed) < table->size / 2) return;
    if (table->size > std::numeric_limits<std::uint32_t>::max() / 2)
        throw std::length_error("ConcurrentPtrHashSet: table size overflow");

    Current(set).store(&set->stubTable);

    auto* newTable = CreateTable(table->size * 2);
    const auto mask = newTable->mask;
    std::uint32_t load = 0;
    for (std::uint32_t i = 0; i < table->size; ++i) {
        auto* ptr = Slot(table, i).load(std::memory_order_relaxed);
        if (!ptr) continue;
        const auto startIndex = PtrHash(ptr) & mask;
        auto index = startIndex;
        for (;;) {
            auto entry = Slot(newTable, index);
            auto* value = entry.load(std::memory_order_relaxed);
            if (!value) {
                entry.store(ptr, std::memory_order_relaxed);
                break;
            }
            if (value == ptr) throw std::runtime_error("ConcurrentPtrHashSet: duplicate entry during resize");
            index = (index + 1) & mask;
            if (index == startIndex) throw std::runtime_error("ConcurrentPtrHashSet: resized table is full");
        }
        ++load;
    }
    Load(newTable).store(load, std::memory_order_relaxed);

    Current(set).store(newTable);
    Load(&set->stubTable).store(kStubDefaultLoad);
    AppendTable(set, newTable);
}

bool AddSlow(Set* set, Table* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr);

bool Add(Set* set, void* ptr) {
    auto* table = Current(set).load(std::memory_order_relaxed);
    const auto mask = table->mask;
    const auto startIndex = PtrHash(ptr) & mask;
    auto index = startIndex;
    for (;;) {
        auto* entry = Slot(table, index).load(std::memory_order_relaxed);
        if (!entry) return AddSlow(set, table, mask, startIndex, index, ptr);
        if (entry == ptr) return false;
        index = (index + 1) & mask;
        if (index == startIndex) throw std::runtime_error("ConcurrentPtrHashSet: table is full");
    }
}

bool AddSlow(Set* set, Table* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr) {
    if (Load(table).fetch_add(1) >= table->size / 2) {
        ResizeIfNecessary(set);
        return Add(set, ptr);
    }
    for (;;) {
        void* oldEntry = nullptr;
        Slot(table, index).compare_exchange_strong(oldEntry, ptr);
        if (!oldEntry) {
            if (Current(set).load() != table) return Add(set, ptr);
            return true;
        }
        if (oldEntry == ptr) return false;
        index = (index + 1) & mask;
        if (index == startIndex) throw std::runtime_error("ConcurrentPtrHashSet: table is full");
    }
}

}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC1Ev(WTF::ConcurrentPtrHashSet* self) {
    Construct(self);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC2Ev(WTF::ConcurrentPtrHashSet* self) {
    Construct(self);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD1Ev(WTF::ConcurrentPtrHashSet* self) {
    Destroy(self);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD2Ev(WTF::ConcurrentPtrHashSet* self) {
    Destroy(self);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(WTF::ConcurrentPtrHashSet* self) {
    SetLocker locker(CheckedSet(self));
    auto* current = Current(self).load(std::memory_order_relaxed);
    if (current == &self->stubTable) throw std::runtime_error("ConcurrentPtrHashSet::deleteOldTables: stub table installed");
    std::uint32_t kept = 0;
    for (std::uint32_t i = 0; i < self->tablesSize; ++i) {
        auto* table = self->tablesBuffer[i];
        if (table == current)
            self->tablesBuffer[kept++] = table;
        else
            _ZN3WTF8fastFreeEPv(table);
    }
    self->tablesSize = kept;
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet5clearEv(WTF::ConcurrentPtrHashSet* self) {
    SetLocker locker(CheckedSet(self));
    DestroyTables(self);
    Initialize(self);
}

extern "C" bool APS5_VABI _ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv(WTF::ConcurrentPtrHashSet* self,
    WTF::ConcurrentPtrHashSetTable* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr) {
    CheckedSet(self);
    if (!table) throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: null table");
    if (!ptr) throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: null pointer");
    if (mask != table->mask || startIndex > mask || index > mask)
        throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: index outside the table");
    return AddSlow(self, table, mask, startIndex, index, ptr);
}
