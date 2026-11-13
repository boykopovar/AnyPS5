#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

template <typename Table>
static void RequireEmpty(const Table* table, std::uint64_t size) {
    Require(table != nullptr);
    Require(table->size == size);
    const auto* entries = reinterpret_cast<const unsigned char*>(table) + sizeof(table->size);
    for (std::size_t index = 0; index < sizeof(Table) - sizeof(table->size); index++) Require(entries[index] == 0);
}

int main() {
    const auto* mallocTable = sceKernelGetSanitizerMallocReplaceExternal();
    RequireEmpty(mallocTable, 0x70);
    Require(sceKernelGetSanitizerMallocReplaceExternal() == mallocTable);
    const auto* newTable = sceKernelGetSanitizerNewReplaceExternal();
    RequireEmpty(newTable, 0x68);
    Require(sceKernelGetSanitizerNewReplaceExternal() == newTable);
}
