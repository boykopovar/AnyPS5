#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/SymbolTableAbi.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace NKWebKit::JscAbi;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SymbolTableConstructor check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct FakeVm {
    alignas(16) unsigned char bytes[0xa000] = {};
};

struct FakeStructure {
    alignas(16) unsigned char bytes[0x20] = {};
};

struct FakeTable {
    alignas(16) unsigned char bytes[kSymbolTableSize + 0x10];
};

template <typename T>
static T Get(const unsigned char* base, std::size_t offset) {
    T value;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

template <typename T>
static void Put(unsigned char* base, std::size_t offset, T value) {
    std::memcpy(base + offset, &value, sizeof(value));
}

static void Wire(FakeVm& vm, FakeStructure& structure, const unsigned char*& slot) {
    slot = structure.bytes;
    const unsigned char** slotAddress = &slot;
    Put(vm.bytes, kVmSymbolTableStructureSlotOffset, slotAddress);
}

static void Fill(FakeTable& table, unsigned char value) {
    std::memset(table.bytes, value, sizeof(table.bytes));
}

static void CheckConstructed(const FakeTable& table, const FakeStructure& structure, const unsigned char* before) {
    Require(Get<std::uint32_t>(table.bytes, kCellStructureIdOffset) == Get<std::uint32_t>(structure.bytes, kStructureBlobOffset));
    Require(table.bytes[kCellIndexingTypeOffset] == structure.bytes[kStructureIndexingTypeOffset]);
    Require(table.bytes[kCellTypeByteOffset] == structure.bytes[kStructureTypeOffset]);
    Require(table.bytes[kCellInlineTypeFlagsOffset] == structure.bytes[kStructureInlineTypeFlagsOffset]);
    Require(table.bytes[kCellStateOffset] == kCellStateDefinitelyWhite);
    Require(Get<std::uint64_t>(table.bytes, kSymbolTableMapOffset) == 0);
    Require(Get<std::uint32_t>(table.bytes, kSymbolTableMaxScopeOffsetOffset) == kInvalidScopeOffset);
    Require(table.bytes[kSymbolTableLockOffset] == 0);
    Require(table.bytes[kSymbolTableFlagsOffset] == (before[kSymbolTableFlagsOffset] & kSymbolTableFlagsKeepMask));
    Require(Get<std::uint64_t>(table.bytes, kSymbolTableRareDataOffset) == 0);
    Require(Get<std::uint64_t>(table.bytes, kSymbolTableArgumentsOffset) == 0);
    Require(Get<std::uint64_t>(table.bytes, kSymbolTableSingletonOffset) == kSymbolTableSingletonInitial);
    Require(Get<std::uint64_t>(table.bytes, kSymbolTableLocalToEntryOffset) == 0);
    for (std::size_t offset : {std::size_t{0x16}, std::size_t{0x17}})
        Require(offset == kCellStateOffset ? true : table.bytes[offset] == before[offset]);
    for (std::size_t offset = kSymbolTableSize; offset < sizeof(table.bytes); ++offset)
        Require(table.bytes[offset] == before[offset]);
}

static void TestBothVariants() {
    using Constructor = void (*)(void*, void*);
    const Constructor constructors[] = {
        reinterpret_cast<Constructor>(&_ZN3JSC11SymbolTableC1ERNS_2VME),
        reinterpret_cast<Constructor>(&_ZN3JSC11SymbolTableC2ERNS_2VME),
    };
    const unsigned char fills[] = {0x00, 0xff, 0xa5, 0x5a};
    for (Constructor construct : constructors) {
        for (unsigned char fill : fills) {
            FakeVm vm;
            FakeStructure structure;
            const unsigned char* slot = nullptr;
            Wire(vm, structure, slot);
            for (std::size_t i = 0; i < sizeof(structure.bytes); ++i)
                structure.bytes[i] = static_cast<unsigned char>(0x11 * (i + 1));
            FakeTable table;
            Fill(table, fill);
            unsigned char before[sizeof(table.bytes)];
            std::memcpy(before, table.bytes, sizeof(before));
            construct(table.bytes, vm.bytes);
            CheckConstructed(table, structure, before);
        }
    }
}

static void TestFlagsKeepHighBits() {
    FakeVm vm;
    FakeStructure structure;
    const unsigned char* slot = nullptr;
    Wire(vm, structure, slot);
    FakeTable table;
    Fill(table, 0);
    table.bytes[kSymbolTableFlagsOffset] = 0xff;
    _ZN3JSC11SymbolTableC1ERNS_2VME(table.bytes, vm.bytes);
    Require(table.bytes[kSymbolTableFlagsOffset] == 0xe0);
    table.bytes[kSymbolTableFlagsOffset] = 0x1f;
    _ZN3JSC11SymbolTableC2ERNS_2VME(table.bytes, vm.bytes);
    Require(table.bytes[kSymbolTableFlagsOffset] == 0x00);
}

static bool Throws(void (*function)(void*, void*), void* self, void* vm) {
    try {
        function(self, vm);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestNullInputs() {
    FakeVm vm;
    FakeStructure structure;
    const unsigned char* slot = nullptr;
    Wire(vm, structure, slot);
    FakeTable table;
    Fill(table, 0);
    void (*const functions[])(void*, void*) = {_ZN3JSC11SymbolTableC1ERNS_2VME, _ZN3JSC11SymbolTableC2ERNS_2VME};
    for (auto function : functions) {
        Require(Throws(function, nullptr, vm.bytes));
        Require(Throws(function, table.bytes, nullptr));
        slot = nullptr;
        Require(Throws(function, table.bytes, vm.bytes));
        FakeVm noSlot;
        Require(Throws(function, table.bytes, noSlot.bytes));
        slot = structure.bytes;
    }
}

int main() {
    TestBothVariants();
    TestFlagsKeepHighBits();
    TestNullInputs();
    std::puts("SymbolTableConstructor ok");
    return 0;
}
