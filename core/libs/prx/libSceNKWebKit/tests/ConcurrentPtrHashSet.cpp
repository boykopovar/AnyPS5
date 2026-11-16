#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ConcurrentPtrHashSet check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using Set = WTF::ConcurrentPtrHashSet;
using Table = WTF::ConcurrentPtrHashSetTable;
using Lifetime = void (APS5_VABI *)(Set*);
using AddSlow = bool (APS5_VABI *)(Set*, Table*, std::uint32_t, std::uint32_t, std::uint32_t, void*);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSetC1Ev), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSetC2Ev), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSetD1Ev), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSetD2Ev), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSet5clearEv), Lifetime>);
static_assert(std::is_same_v<decltype(&_ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv), AddSlow>);

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

void* Entry(Table* table, std::uint32_t index) {
    auto* array = reinterpret_cast<void**>(reinterpret_cast<std::byte*>(table) + offsetof(Table, array));
    return std::atomic_ref<void*>(array[index]).load(std::memory_order_relaxed);
}

Table* CurrentTable(Set* set) {
    return std::atomic_ref<Table*>(set->table).load(std::memory_order_relaxed);
}

bool InlineAdd(Set* set, void* ptr) {
    auto* table = CurrentTable(set);
    const auto mask = table->mask;
    const auto startIndex = PtrHash(ptr) & mask;
    auto index = startIndex;
    for (;;) {
        auto* entry = Entry(table, index);
        if (!entry) return _ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv(set, table, mask, startIndex, index, ptr);
        if (entry == ptr) return false;
        index = (index + 1) & mask;
        Require(index != startIndex);
    }
}

bool InlineContains(Set* set, void* ptr) {
    auto* table = CurrentTable(set);
    Require(table != &set->stubTable);
    const auto mask = table->mask;
    const auto startIndex = PtrHash(ptr) & mask;
    auto index = startIndex;
    for (;;) {
        auto* entry = Entry(table, index);
        if (!entry) return false;
        if (entry == ptr) return true;
        index = (index + 1) & mask;
        Require(index != startIndex);
    }
}

void* Key(std::uintptr_t value) {
    return reinterpret_cast<void*>((value + 1) * 16);
}

struct GuardedSet {
    std::uint64_t before = 0x0123456789ABCDEF;
    Set set;
    std::uint64_t after = 0xFEDCBA9876543210;

    void CheckGuards() const {
        Require(before == 0x0123456789ABCDEF && after == 0xFEDCBA9876543210);
    }
};

void RequireFreshState(Set& set) {
    Require(set.tablesBuffer == set.tablesInline && set.tablesCapacity == 4 && set.tablesSize == 1);
    auto* table = set.tablesBuffer[0];
    Require(CurrentTable(&set) == table);
    Require(table->size == 32 && table->mask == 31 && table->load == 0);
    Require(_ZN3WTF14fastMallocSizeEPKv(table) == offsetof(Table, array) + 32 * sizeof(void*));
    for (std::uint32_t i = 0; i < 32; ++i) Require(Entry(table, i) == nullptr);
    Require(set.stubTable.size == 0 && set.stubTable.mask == 0 && set.stubTable.load == 10);
    Require(set.stubTable.array[0] == nullptr && set.lock == 0);
}

void TestConstructionAndClear() {
    for (auto construct : {&_ZN3WTF20ConcurrentPtrHashSetC1Ev, &_ZN3WTF20ConcurrentPtrHashSetC2Ev}) {
        GuardedSet value;
        std::memset(&value.set, 0xCD, sizeof(value.set));
        construct(&value.set);
        value.CheckGuards();
        RequireFreshState(value.set);
        _ZN3WTF20ConcurrentPtrHashSetD1Ev(&value.set);
        value.CheckGuards();
    }

    GuardedSet value;
    _ZN3WTF20ConcurrentPtrHashSetC1Ev(&value.set);
    for (std::uintptr_t i = 0; i < 1000; ++i) Require(InlineAdd(&value.set, Key(i)));
    Require(value.set.tablesBuffer != value.set.tablesInline);
    Require(value.set.tablesCapacity == 16 && value.set.tablesSize == 7);
    for (std::uint32_t i = 0; i < 7; ++i) Require(value.set.tablesBuffer[i]->size == (32u << i));
    _ZN3WTF20ConcurrentPtrHashSet5clearEv(&value.set);
    value.CheckGuards();
    RequireFreshState(value.set);
    Require(!InlineContains(&value.set, Key(0)));
    Require(InlineAdd(&value.set, Key(0)));
    Require(InlineContains(&value.set, Key(0)));
    _ZN3WTF20ConcurrentPtrHashSetD2Ev(&value.set);
    value.CheckGuards();
    std::printf("ConcurrentPtrHashSet: construction, clear and destruction passed\n");
}

void TestAddAndResize() {
    GuardedSet value;
    _ZN3WTF20ConcurrentPtrHashSetC1Ev(&value.set);
    constexpr std::uintptr_t count = 5000;
    for (std::uintptr_t i = 0; i < count; ++i) {
        Require(InlineAdd(&value.set, Key(i)));
        Require(!InlineAdd(&value.set, Key(i)));
    }
    auto* table = CurrentTable(&value.set);
    Require(table->size == 16384 && table->mask == 16383);
    Require(table->load == count);
    Require(value.set.stubTable.load == 10 && value.set.lock == 0);
    for (std::uintptr_t i = 0; i < count; ++i) Require(InlineContains(&value.set, Key(i)));
    for (std::uintptr_t i = count; i < count * 2; ++i) Require(!InlineContains(&value.set, Key(i)));

    std::size_t stored = 0;
    for (std::uint32_t i = 0; i < table->size; ++i) stored += Entry(table, i) != nullptr;
    Require(stored == count);

    Require(value.set.tablesSize == 10 && value.set.tablesCapacity == 16);
    _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(&value.set);
    Require(value.set.tablesSize == 1 && value.set.tablesBuffer[0] == table);
    Require(value.set.tablesBuffer != value.set.tablesInline && value.set.tablesCapacity == 16);
    for (std::uintptr_t i = 0; i < count; ++i) Require(InlineContains(&value.set, Key(i)));
    _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(&value.set);
    Require(value.set.tablesSize == 1 && value.set.tablesBuffer[0] == table);
    Require(InlineAdd(&value.set, Key(count)));
    _ZN3WTF20ConcurrentPtrHashSetD1Ev(&value.set);
    value.CheckGuards();
    std::printf("ConcurrentPtrHashSet: %zu adds, resizes and deleteOldTables passed\n", static_cast<std::size_t>(count));
}

void TestConcurrentAdds() {
    Set set;
    _ZN3WTF20ConcurrentPtrHashSetC1Ev(&set);
    constexpr unsigned threads = 4;
    constexpr std::uintptr_t keys = 4000;
    std::atomic<std::size_t> added{0};
    std::vector<std::thread> workers;
    for (unsigned t = 0; t < threads; ++t) {
        workers.emplace_back([&, t] {
            std::size_t local = 0;
            for (std::uintptr_t i = 0; i < keys; ++i) local += InlineAdd(&set, Key((i * 7919 + t * 1013) % keys));
            added += local;
        });
    }
    for (auto& worker : workers) worker.join();
    Require(added <= keys);
    for (std::uintptr_t i = 0; i < keys; ++i) Require(InlineContains(&set, Key(i)));
    Require(CurrentTable(&set) != &set.stubTable && set.lock == 0);
    _ZN3WTF20ConcurrentPtrHashSetD1Ev(&set);
    std::printf("ConcurrentPtrHashSet: %u threads stored all %zu keys, at most one add reported each\n", threads, static_cast<std::size_t>(keys));
}

void TestInvalidArguments() {
    RequireThrows([] { _ZN3WTF20ConcurrentPtrHashSetC1Ev(nullptr); });
    RequireThrows([] { _ZN3WTF20ConcurrentPtrHashSetD1Ev(nullptr); });
    RequireThrows([] { _ZN3WTF20ConcurrentPtrHashSet5clearEv(nullptr); });
    RequireThrows([] { _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(nullptr); });
    Set set;
    _ZN3WTF20ConcurrentPtrHashSetC1Ev(&set);
    auto* table = CurrentTable(&set);
    auto add = &_ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv;
    RequireThrows([&] { add(nullptr, table, 31, 0, 0, Key(1)); });
    RequireThrows([&] { add(&set, nullptr, 31, 0, 0, Key(1)); });
    RequireThrows([&] { add(&set, table, 31, 0, 0, nullptr); });
    RequireThrows([&] { add(&set, table, 15, 0, 0, Key(1)); });
    RequireThrows([&] { add(&set, table, 31, 32, 0, Key(1)); });
    RequireThrows([&] { add(&set, table, 31, 0, 32, Key(1)); });
    Require(table->load == 0 && set.lock == 0);
    Require(add(&set, &set.stubTable, 0, 0, 0, Key(1)));
    Require(InlineContains(&set, Key(1)) && set.tablesSize == 1 && set.stubTable.load == 11);
    _ZN3WTF20ConcurrentPtrHashSetD1Ev(&set);
    std::printf("ConcurrentPtrHashSet: invalid arguments throw\n");
}

}

int main() {
    TestConstructionAndClear();
    TestAddAndResize();
    TestConcurrentAdds();
    TestInvalidArguments();
}
