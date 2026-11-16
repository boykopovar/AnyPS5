#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WTF {

struct ConcurrentPtrHashSetTable {
    std::uint32_t size;
    std::uint32_t mask;
    std::uint32_t load;
    void* array[1];
};

struct ConcurrentPtrHashSet {
    ConcurrentPtrHashSetTable** tablesBuffer;
    std::uint32_t tablesCapacity;
    std::uint32_t tablesSize;
    ConcurrentPtrHashSetTable* tablesInline[4];
    ConcurrentPtrHashSetTable* table;
    ConcurrentPtrHashSetTable stubTable;
    std::uint8_t lock;
};

static_assert(std::is_standard_layout_v<ConcurrentPtrHashSetTable>);
static_assert(sizeof(ConcurrentPtrHashSetTable) == 24);
static_assert(offsetof(ConcurrentPtrHashSetTable, size) == 0);
static_assert(offsetof(ConcurrentPtrHashSetTable, mask) == 4);
static_assert(offsetof(ConcurrentPtrHashSetTable, load) == 8);
static_assert(offsetof(ConcurrentPtrHashSetTable, array) == 16);
static_assert(std::is_standard_layout_v<ConcurrentPtrHashSet>);
static_assert(sizeof(ConcurrentPtrHashSet) == 88);
static_assert(alignof(ConcurrentPtrHashSet) == 8);
static_assert(offsetof(ConcurrentPtrHashSet, tablesBuffer) == 0);
static_assert(offsetof(ConcurrentPtrHashSet, tablesCapacity) == 8);
static_assert(offsetof(ConcurrentPtrHashSet, tablesSize) == 12);
static_assert(offsetof(ConcurrentPtrHashSet, tablesInline) == 16);
static_assert(offsetof(ConcurrentPtrHashSet, table) == 48);
static_assert(offsetof(ConcurrentPtrHashSet, stubTable) == 56);
static_assert(offsetof(ConcurrentPtrHashSet, lock) == 80);

}

extern "C" {

void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC1Ev(WTF::ConcurrentPtrHashSet* self);
void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetC2Ev(WTF::ConcurrentPtrHashSet* self);
void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD1Ev(WTF::ConcurrentPtrHashSet* self);
void APS5_VABI _ZN3WTF20ConcurrentPtrHashSetD2Ev(WTF::ConcurrentPtrHashSet* self);
void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet15deleteOldTablesEv(WTF::ConcurrentPtrHashSet* self);
void APS5_VABI _ZN3WTF20ConcurrentPtrHashSet5clearEv(WTF::ConcurrentPtrHashSet* self);
bool APS5_VABI _ZN3WTF20ConcurrentPtrHashSet7addSlowEPNS0_5TableEjjjPv(WTF::ConcurrentPtrHashSet* self,
    WTF::ConcurrentPtrHashSetTable* table, std::uint32_t mask, std::uint32_t startIndex, std::uint32_t index, void* ptr);

}
