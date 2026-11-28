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

    const auto initialTimestamp = get_vm_map_timestamp_nid_postfix();
    void* reserved = nullptr;
    Require(sceKernelReserveVirtualRange(&reserved, reserveBytes, 0, page) == 0);

    const auto reservedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(reservedTimestamp != initialTimestamp);
    Require(get_vm_map_timestamp_nid_postfix() == reservedTimestamp);

    void* mapped = reserved;
    Require(sceKernelMapFlexibleMemory(&mapped, page, 3, 0x10) == 0 && mapped == reserved);
    const auto mappedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(mappedTimestamp != reservedTimestamp);
    Require(get_vm_map_timestamp_nid_postfix() == mappedTimestamp);

    const auto registryGeneration = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    {
        GuestAllocations::Mutation query;
        Require(query.Covers(mapped, page));
    }
    Require(GuestAllocations::GuestAllocationsGeneration_nid_postfix() != registryGeneration);
    Require(get_vm_map_timestamp_nid_postfix() == mappedTimestamp);

    const auto failedMutationTimestamp = get_vm_map_timestamp_nid_postfix();
    bool addFailed = false;
    try {
        GuestAllocations::Mutation mutation;
        mutation.Add(mapped, page, true, true, false);
    } catch (...) {
        addFailed = true;
    }
    Require(addFailed);
    Require(get_vm_map_timestamp_nid_postfix() == failedMutationTimestamp);

    const auto beforeProtectTimestamp = get_vm_map_timestamp_nid_postfix();
    {
        GuestAllocations::Mutation mutation;
        mutation.Protect(mapped, page, true, false, false, [] {});
    }
    const auto protectedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(protectedTimestamp != beforeProtectTimestamp);
    Require(get_vm_map_timestamp_nid_postfix() == protectedTimestamp);

    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(reserved), reserveBytes) == 0);
    const auto unmappedTimestamp = get_vm_map_timestamp_nid_postfix();
    Require(unmappedTimestamp != protectedTimestamp);

    bool unmapFailed = false;
    try {
        GuestAllocations::Mutation mutation;
        mutation.Unmap(reserved, page, [](const void*, std::size_t, const void*, bool) {});
    } catch (...) {
        unmapFailed = true;
    }
    Require(unmapFailed);
    Require(get_vm_map_timestamp_nid_postfix() == unmappedTimestamp);

    bool protectFailed = false;
    try {
        GuestAllocations::Mutation mutation;
        mutation.Protect(reserved, page, true, true, false, [] {});
    } catch (...) {
        protectFailed = true;
    }
    Require(protectFailed);
    Require(get_vm_map_timestamp_nid_postfix() == unmappedTimestamp);

    GuestAllocations::GuestAllocationsInvalidate_nid_postfix(reinterpret_cast<std::uintptr_t>(reserved), page);
    Require(get_vm_map_timestamp_nid_postfix() != unmappedTimestamp);

    void* block = std::malloc(page);
    Require(block != nullptr);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, page, true, true, false);
    }
    const auto addedTimestamp = get_vm_map_timestamp_nid_postfix();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    Require(get_vm_map_timestamp_nid_postfix() != addedTimestamp);
    std::free(block);
    return 0;
}
