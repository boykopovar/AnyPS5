#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "crt setup check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

namespace {

struct Table {
    std::uint64_t size;
    std::uintptr_t unknown;
    std::uintptr_t primary[8];
    std::uintptr_t extended[4];
    std::uintptr_t trailing[10];
};

static_assert(offsetof(Table, primary) == 0x10);
static_assert(offsetof(Table, extended) == 0x50);
static_assert(offsetof(Table, trailing) == 0x70);
static_assert(sizeof(Table) == 0xc0);

void Fill(std::uintptr_t* slots, std::size_t count, std::uintptr_t base) {
    for (std::size_t index = 0; index < count; ++index) slots[index] = base + index * 8;
}

void Empty(std::uintptr_t* slots, std::size_t count) { std::memset(slots, 0, count * sizeof(std::uintptr_t)); }

void TableChecks() {
    Table table{};
    std::memset(&table, 0, sizeof(table));

    Require(_new_setup_nid_postfix(nullptr) == 0);

    table.size = 0x4f;
    Require(_new_setup_nid_postfix(&table) == 1);

    table.size = 0x50;
    Require(_new_setup_nid_postfix(&table) == 0);

    Fill(table.primary, 8, 0x1000);
    Require(_new_setup_nid_postfix(&table) == 0);

    table.primary[3] = 0;
    Require(_new_setup_nid_postfix(&table) == 1);

    table.primary[3] = 0x1018;
    Require(_new_setup_nid_postfix(&table) == 0);

    table.size = 0x70;
    Fill(table.extended, 4, 0x2000);
    table.extended[1] = 0;
    table.extended[2] = 0;
    Require(_new_setup_nid_postfix(&table) == 0);

    table.size = 0xc0;
    Fill(table.trailing, 10, 0x3000);
    Require(_new_setup_nid_postfix(&table) == 0);

    table.trailing[9] = 0;
    Require(_new_setup_nid_postfix(&table) == 1);

    Fill(table.trailing, 10, 0x4000);
    table.trailing[0] = 0;
    Require(_new_setup_nid_postfix(&table) == 1);

    Empty(table.trailing, 10);
    Require(_new_setup_nid_postfix(&table) == 0);

    Empty(table.primary, 8);
    Require(_new_setup_nid_postfix(&table) == 0);
}

struct Region {
    void* memory;
    std::size_t pageSize;
    Table* table;
};

Region MapAbutting() {
    Region region{};
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    region.pageSize = info.dwPageSize;
    auto* base = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, region.pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(base != nullptr);
    DWORD previous = 0;
    Require(VirtualProtect(base + region.pageSize, region.pageSize, PAGE_NOACCESS, &previous) != 0);
    region.memory = base;
#else
    region.pageSize = 4096;
    auto* base = static_cast<unsigned char*>(
        mmap(nullptr, region.pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(base != MAP_FAILED);
    Require(mprotect(base + region.pageSize, region.pageSize, PROT_NONE) == 0);
    region.memory = base;
#endif
    region.table = reinterpret_cast<Table*>(static_cast<unsigned char*>(region.memory) + region.pageSize - 0x50);
    return region;
}

void AbuttingChecks() {
    const Region region = MapAbutting();
    std::memset(region.table, 0, 0x50);
    region.table->size = 0x50;
    Fill(region.table->primary, 8, 0x5000);
    Require(_new_setup_nid_postfix(region.table) == 0);
    Require(region.table->primary[7] == 0x5038);
}

void ConstantChecks() {
    Require(_sceLibcGetMallocParam_nid_postfix() == nullptr);
    for (int index = 0; index < 64; ++index) Require(_sceLibcGetMallocParam_nid_postfix() == nullptr);
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "table") == 0) TableChecks();
    else if (std::strcmp(argv[1], "abutting") == 0) AbuttingChecks();
    else if (std::strcmp(argv[1], "constants") == 0) ConstantChecks();
    else Require(false);
    std::puts("crt setup table checks and constant answers passed");
}
