#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VM map timestamp check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    constexpr std::size_t page = 0x4000;
    constexpr std::size_t reserveBytes = 2 * page;

    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, reserveBytes, 0, page) == 0);

    const auto reservedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(get_vm_map_timestamp_nid_postfix() == reservedTimestamp);

    void* mapped = reserved;
    Require(sceKernelMapFlexibleMemory(&mapped, page, 3, 0x10) == 0 && mapped == reserved);
    const auto mappedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(mappedTimestamp != reservedTimestamp);
    Require(get_vm_map_timestamp_nid_postfix() == mappedTimestamp);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), reserveBytes) == 0);
    Require(get_vm_map_timestamp_nid_postfix() != mappedTimestamp);
    Require(static_cast<std::uint32_t>(get_vm_map_timestamp_nid_postfix()) ==
            static_cast<std::uint32_t>(GuestAllocations::GuestAllocationsGeneration_nid_postfix()));
    return 0;
}
