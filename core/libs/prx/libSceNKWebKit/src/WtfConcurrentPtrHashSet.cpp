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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <type_traits>
#include <vector>

namespace {

using Set = WTF::ConcurrentPtrHashSet;
using Table = WTF::ConcurrentPtrHashSetTable;

static_assert(std::is_trivially_copyable_v<Table>);
static_assert(std::is_trivially_copyable_v<Set>);

constexpr std::uint32_t kInitialSize = 32;
constexpr std::uint32_t kStubDefaultLoad = 10;
constexpr std::uint32_t kInlineTables = 4;
constexpr std::size_t kMinTablesCapacity = 16;
constexpr std::uint8_t kLockHeldBit = 1;

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

void RequireAccessibleSet(Set* set, const char* function) {
    if (!set) throw std::invalid_argument("ConcurrentPtrHashSet: null set");
    RequireReadableGuestBytes(set, sizeof(Set), function);
    RequireWritableGuestBytes(set, sizeof(Set), function);
}

Table ValidateTable(Table* table, const char* function) {
    if (!table) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(table, sizeof(Table), function);
    RequireWritableGuestBytes(table, sizeof(Table), function);
    const Table header = ReadGuestValue(table, function);
    if (header.mask == std::numeric_limits<std::uint32_t>::max())
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t slotCount = std::max<std::size_t>(
        std::max<std::size_t>(1u, static_cast<std::size_t>(header.size)),
        static_cast<std::size_t>(header.mask) + 1u);
    if (slotCount > (std::numeric_limits<std::size_t>::max() - offsetof(Table, array)) / sizeof(void*))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t totalBytes = offsetof(Table, array) + slotCount * sizeof(void*);
    RequireReadableGuestBytes(table, totalBytes, function);
    RequireWritableGuestBytes(table, totalBytes, function);
    return header;
}

void FastFreeNoThrow(void* pointer) noexcept {
    if (!pointer) return;
    try {
        _ZN3WTF8fastFreeEPv(pointer);
    } catch (...) {
    }
}

struct FastFreeGuard {
    void* ptr = nullptr;

    ~FastFreeGuard() {
        FastFreeNoThrow(ptr);
    }
};

class SetLocker {
public:
    SetLocker(Set* set, const char* function)
        : m_guard(set, sizeof(Set)), m_set(set) {
        if (!m_guard.Open() || !GuestWritable_nid_no_patch(set, sizeof(Set)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        std::atomic_ref<std::uint8_t> byte(m_set->lock);
        std::uint8_t expected = 0;
        while (!byte.compare_exchange_weak(expected, kLockHeldBit, std::memory_order_acquire)) {
            if (expected & ~kLockHeldBit)
                throw std::runtime_error("ConcurrentPtrHashSet: lock has parked threads");
            expected = 0;
            std::this_thread::yield();
        }
    }

    ~SetLocker() {
        std::atomic_ref<std::uint8_t>(m_set->lock).store(0, std::memory_order_release);
    }

    SetLocker(const SetLocker&) = delete;
    SetLocker& operator=(const SetLocker&) = delete;

private:
    GuestArena::HostWrite m_guard;
    Set* m_set;
};

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

std::vector<Table*> ReadTableList(const Set& snapshot, const char* function) {
    if (snapshot.tablesSize && !snapshot.tablesBuffer)
        throw std::invalid_argument("ConcurrentPtrHashSet: null table list");
    if (snapshot.tablesSize > snapshot.tablesCapacity)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::vector<Table*> entries(snapshot.tablesSize);
    if (snapshot.tablesSize) {
        if (snapshot.tablesSize > std::numeric_limits<std::size_t>::max() / sizeof(Table*))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const std::size_t byteLength = static_cast<std::size_t>(snapshot.tablesSize) * sizeof(Table*);
        RequireReadableGuestBytes(snapshot.tablesBuffer, byteLength, function);
        if (!GuestCopyIn_nid_no_patch(entries.data(), snapshot.tablesBuffer, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    return entries;
}

void ValidateAppendableTableList(const Set& snapshot, const char* function) {
    if (snapshot.tablesSize > snapshot.tablesCapacity)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (snapshot.tablesSize && !snapshot.tablesBuffer)
        throw std::invalid_argument("ConcurrentPtrHashSet: null table list");
    if (snapshot.tablesSize == snapshot.tablesCapacity) {
        if (snapshot.tablesSize) {
            if (snapshot.tablesSize > std::numeric_limits<std::size_t>::max() / sizeof(Table*))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
            RequireReadableGuestBytes(snapshot.tablesBuffer, static_cast<std::size_t>(snapshot.tablesSize) * sizeof(Table*), function);
        }
        return;
    }
    if (!snapshot.tablesBuffer) throw std::invalid_argument("ConcurrentPtrHashSet: null table list");
    if (snapshot.tablesSize >= std::numeric_limits<std::size_t>::max() / sizeof(Table*))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireWritableGuestBytes(snapshot.tablesBuffer + snapshot.tablesSize, sizeof(Table*), function);
}

void AppendTable(Set* set, Table* table, const char* function) {
    if (set->tablesSize > set->tablesCapacity)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (set->tablesSize == set->tablesCapacity) {
        const std::size_t capacity = set->tablesCapacity;
        const auto newCapacity = std::max<std::size_t>(capacity + 1, std::max(kMinTablesCapacity, capacity + capacity / 4 + 1));
        if (newCapacity > std::numeric_limits<std::uint32_t>::max() / sizeof(Table*))
            throw std::length_error("ConcurrentPtrHashSet: table list capacity overflow");
        auto* buffer = static_cast<Table**>(_ZN3WTF10fastMallocEm(newCapacity * sizeof(Table*)));
        FastFreeGuard bufferGuard{buffer};
        if (set->tablesSize) {
            if (!GuestCopyIn_nid_no_patch(buffer, set->tablesBuffer, set->tablesSize * sizeof(Table*)))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        Table** const previousBuffer = set->tablesBuffer;
        set->tablesBuffer = buffer;
        set->tablesCapacity = static_cast<std::uint32_t>(newCapacity);
        bufferGuard.ptr = nullptr;
        if (previousBuffer && previousBuffer != set->tablesInline) FastFreeNoThrow(previousBuffer);
    }
    if (!GuestCopyOut_nid_no_patch(set->tablesBuffer + set->tablesSize, &table, sizeof(table)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    ++set->tablesSize;
}

void Construct(Set* set, const char* function) {
    if (!set) throw std::invalid_argument("ConcurrentPtrHashSet: null set");
    RequireWritableGuestBytes(set, sizeof(Set), function);
    FastFreeGuard tableGuard{CreateTable(kInitialSize)};
    auto* const initialTable = static_cast<Table*>(tableGuard.ptr);
    Set initialized{};
    initialized.tablesBuffer = set->tablesInline;
    initialized.tablesCapacity = kInlineTables;
    initialized.tablesSize = 1;
    initialized.tablesInline[0] = initialTable;
    initialized.table = initialTable;
    InitializeStub(initialized.stubTable);
    initialized.lock = 0;
    if (!GuestCopyOut_nid_no_patch(set, &initialized, sizeof(initialized)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    tableGuard.ptr = nullptr;
}

void Destroy(Set* set, const char* function) {
    RequireAccessibleSet(set, function);
    const Set snapshot = ReadGuestValue(set, function);
    const std::vector<Table*> oldTables = ReadTableList(snapshot, function);
    Set cleared = snapshot;
    cleared.tablesBuffer = set->tablesInline;
    cleared.tablesCapacity = kInlineTables;
    cleared.tablesSize = 0;
    if (!GuestCopyOut_nid_no_patch(set, &cleared, sizeof(cleared)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    for (Table* oldTable : oldTables) FastFreeNoThrow(oldTable);
    if (snapshot.tablesBuffer && snapshot.tablesBuffer != set->tablesInline) FastFreeNoThrow(snapshot.tablesBuffer);
}

void DeleteOldTables(Set* set, const char* function) {
    RequireAccessibleSet(set, function);
    SetLocker locker(set, function);
    const Set snapshot = ReadGuestValue(set, function);
    auto* current = Current(set).load(std::memory_order_relaxed);
    if (current == &set->stubTable) throw std::runtime_error("ConcurrentPtrHashSet::deleteOldTables: stub table installed");
    const std::vector<Table*> entries = ReadTableList(snapshot, function);
    std::vector<Table*> kept;
    std::vector<Table*> retired;
    kept.reserve(entries.size());
    retired.reserve(entries.size());
    for (Table* table : entries) {
        if (table == current) kept.push_back(table);
        else retired.push_back(table);
    }
    if (!kept.empty()) {
        const std::size_t keptBytes = kept.size() * sizeof(Table*);
        RequireWritableGuestBytes(snapshot.tablesBuffer, keptBytes, function);
        if (!GuestCopyOut_nid_no_patch(snapshot.tablesBuffer, kept.data(), keptBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
    set->tablesSize = static_cast<std::uint32_t>(kept.size());
    for (Table* table : retired) FastFreeNoThrow(table);
}

void Clear(Set* set, const char* function) {
    RequireAccessibleSet(set, function);
    SetLocker locker(set, function);
    const Set snapshot = ReadGuestValue(set, function);
    const std::vector<Table*> oldTables = ReadTableList(snapshot, function);
    FastFreeGuard tableGuard{CreateTable(kInitialSize)};
    auto* const initialTable = static_cast<Table*>(tableGuard.ptr);
    set->tablesBuffer = set->tablesInline;
    set->tablesCapacity = kInlineTables;
    set->tablesSize = 1;
    set->tablesInline[0] = initialTable;
    Current(set).store(initialTable, std::memory_order_relaxed);
    InitializeStub(set->stubTable);
    tableGuard.ptr = nullptr;
    for (Table* oldTable : oldTables) FastFreeNoThrow(oldTable);
    if (snapshot.tablesBuffer && snapshot.tablesBuffer != set->tablesInline) FastFreeNoThrow(snapshot.tablesBuffer);
}

struct StubRestoreGuard {
    Set* set = nullptr;
    Table* previous = nullptr;

    ~StubRestoreGuard() {
        if (set && previous) {
            Current(set).store(previous);
            Load(&set->stubTable).store(kStubDefaultLoad);
        }
    }
};

void ResizeIfNecessary(Set* set, const char* function) {
    SetLocker locker(set, function);
    const Set snapshot = ReadGuestValue(set, function);
    auto* table = Current(set).load(std::memory_order_relaxed);
    if (table == &set->stubTable) throw std::runtime_error("ConcurrentPtrHashSet: stub table installed outside a resize");
    const Table tableHeader = ValidateTable(table, function);
    if (Load(table).load(std::memory_order_relaxed) < tableHeader.size / 2) return;
    if (tableHeader.size > std::numeric_limits<std::uint32_t>::max() / 2)
        throw std::length_error("ConcurrentPtrHashSet: table size overflow");
    ValidateAppendableTableList(snapshot, function);

    Current(set).store(&set->stubTable);
    StubRestoreGuard stubRestore{set, table};

    FastFreeGuard newTableGuard{CreateTable(tableHeader.size * 2)};
    auto* const newTable = static_cast<Table*>(newTableGuard.ptr);
    const auto mask = newTable->mask;
    std::uint32_t load = 0;
    for (std::uint32_t i = 0; i < tableHeader.size; ++i) {
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

    AppendTable(set, newTable, function);
    newTableGuard.ptr = nullptr;
    stubRestore.previous = nullptr;
    Current(set).store(newTable);
    Load(&set->stubTable).store(kStubDefaultLoad);
}

bool AddSlow(Set* set, Table* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr, const char* function);

bool Add(Set* set, void* ptr, const char* function) {
    auto* table = Current(set).load(std::memory_order_relaxed);
    const Table tableHeader = ValidateTable(table, function);
    const auto mask = tableHeader.mask;
    const auto startIndex = PtrHash(ptr) & mask;
    auto index = startIndex;
    for (;;) {
        auto* entry = Slot(table, index).load(std::memory_order_relaxed);
        if (!entry) return AddSlow(set, table, mask, startIndex, index, ptr, function);
        if (entry == ptr) return false;
        index = (index + 1) & mask;
        if (index == startIndex) throw std::runtime_error("ConcurrentPtrHashSet: table is full");
    }
}

bool AddSlow(Set* set, Table* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr, const char* function) {
    const Table tableHeader = ValidateTable(table, function);
    const std::size_t tableBytes = offsetof(Table, array) + std::max<std::size_t>(1u, static_cast<std::size_t>(mask) + 1u) * sizeof(void*);
    bool needsResize = false;
    bool retry = false;
    bool added = false;
    {
        const GuestArena::HostWrite tableGuard(table, tableBytes);
        if (!tableGuard.Open() || !GuestWritable_nid_no_patch(table, tableBytes))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (Load(table).fetch_add(1) >= tableHeader.size / 2) {
            needsResize = true;
        } else {
            for (;;) {
                void* oldEntry = nullptr;
                Slot(table, index).compare_exchange_strong(oldEntry, ptr);
                if (!oldEntry) {
                    if (Current(set).load() != table) retry = true;
                    else added = true;
                    break;
                }
                if (oldEntry == ptr) break;
                index = (index + 1) & mask;
                if (index == startIndex) throw std::runtime_error("ConcurrentPtrHashSet: table is full");
            }
        }
    }
    if (needsResize) {
        ResizeIfNecessary(set, function);
        return Add(set, ptr, function);
    }
    if (retry) return Add(set, ptr, function);
    return added;
}

bool AddSlowEntry(Set* self, Table* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr, const char* function) {
    RequireAccessibleSet(self, function);
    if (!table) throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: null table");
    if (!ptr) throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: null pointer");
    const Table header = ValidateTable(table, function);
    if (mask != header.mask || startIndex > mask || index > mask)
        throw std::invalid_argument("ConcurrentPtrHashSet::addSlow: index outside the table");
    return AddSlow(self, table, mask, startIndex, index, ptr, function);
}

}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC1Ev(WTF::ConcurrentPtrHashSet* self) {
    Construct(self, __func__);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC2Ev(WTF::ConcurrentPtrHashSet* self) {
    Construct(self, __func__);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD1Ev(WTF::ConcurrentPtrHashSet* self) {
    Destroy(self, __func__);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD2Ev(WTF::ConcurrentPtrHashSet* self) {
    Destroy(self, __func__);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(WTF::ConcurrentPtrHashSet* self) {
    DeleteOldTables(self, __func__);
}

extern "C" void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet5clearEv(WTF::ConcurrentPtrHashSet* self) {
    Clear(self, __func__);
}

extern "C" bool APS5_VABI _ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv(WTF::ConcurrentPtrHashSet* self,
    WTF::ConcurrentPtrHashSetTable* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr) {
    return AddSlowEntry(self, table, mask, startIndex, index, ptr, __func__);
}
