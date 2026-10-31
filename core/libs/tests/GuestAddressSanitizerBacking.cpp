#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libSceAddressSanitizer/src/BackingMemory.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowRequire.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <utility>

static bool GuestRange(std::uintptr_t address, std::size_t size) {
    return address < (1ull << 47) && size <= (1ull << 47) - address &&
        (address + size <= AsanRuntime::ShadowBase || address >= AsanRuntime::ShadowBase + AsanRuntime::ShadowSize);
}

static bool Released(void* pointer) {
    MEMORY_BASIC_INFORMATION memory{};
    return VirtualQuery(pointer, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE;
}

static int InvalidRanges(std::uintptr_t host) {
    const std::array<std::pair<std::uintptr_t, std::size_t>, 5> ranges{{
        {host, 8},
        {AsanRuntime::ShadowBase - 8, 16},
        {(1ull << 47) - 8, 16},
        {1ull << 47, 8},
        {8, std::numeric_limits<std::size_t>::max()}
    }};
    for (const auto& [address, size] : ranges) {
        try {
            RequireShadowRange_nid_no_patch(address, size, __func__);
            std::fprintf(stderr, "accepted invalid backing range: %p size=%zu\n", reinterpret_cast<void*>(address), size);
            return 1;
        } catch (const std::invalid_argument&) {
        }
    }
    return 0;
}

int main() {
    constexpr auto firstGuest = AsanRuntime::ShadowBase + AsanRuntime::ShadowSize;
    constexpr auto hostAddress = AsanRuntime::ShadowBase + (firstGuest >> 3);
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto hostBytes = static_cast<std::size_t>(info.dwAllocationGranularity);
    auto* host = static_cast<unsigned char*>(VirtualAlloc(reinterpret_cast<void*>(hostAddress), hostBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (host != reinterpret_cast<void*>(hostAddress)) return 1;
    std::memset(host, 0x5a, hostBytes);
    if (!AsanRuntime::InitializeShadow_nid_no_patch()) return 2;
    if (AsanRuntime::ShadowRangeAvailable_nid_no_patch(hostAddress, hostAddress + hostBytes)) return 3;
    const auto projected = AsanRuntime::ShadowBase + (hostAddress >> 3);
    if (!AsanRuntime::ShadowRangeAvailable_nid_no_patch(projected, projected + 1)) return 4;
    if (InvalidRanges(hostAddress)) return 5;

    for (const auto alignment : {std::size_t{8}, std::size_t{65536}, std::size_t{1} << 20}) {
        void* block = AsanRuntime::AllocateBacking_nid_no_patch(8193, alignment);
        const auto address = reinterpret_cast<std::uintptr_t>(block);
        if (!block || !GuestRange(address, 8193) || address % alignment != 0) {
            std::fprintf(stderr, "backing allocation failed: pointer=%p alignment=%zu error=%lu\n", block, alignment, GetLastError());
            return 6;
        }
        RequireShadowRange_nid_no_patch(address, 8193, __func__);
        std::memset(block, 0x39, 8193);
        AsanRuntime::FreeBacking_nid_no_patch(block, alignment);
        if (!Released(block)) return 7;
    }

    auto* p = static_cast<unsigned char*>(__asan_malloc(13));
    if (!p || !GuestRange(reinterpret_cast<std::uintptr_t>(p), 13)) return 8;
    if (__asan_address_is_poisoned(p) || __asan_address_is_poisoned(p + 12) || !__asan_address_is_poisoned(p + 13) || !__asan_address_is_poisoned(p - 1)) return 9;
    __asan_memset(p, 0x6b, 13);
    p = static_cast<unsigned char*>(__asan_realloc(p, 257));
    if (!p || !GuestRange(reinterpret_cast<std::uintptr_t>(p), 257)) return 10;
    for (std::size_t i = 0; i < 13; ++i) if (p[i] != 0x6b) return 11;
    if (__asan_region_is_poisoned(reinterpret_cast<std::uintptr_t>(p), 257) || !__asan_address_is_poisoned(p + 257)) return 12;
    std::uintptr_t begin = 0;
    std::uintptr_t size = 0;
    if (std::strcmp(__asan_locate_address(reinterpret_cast<std::uintptr_t>(p), nullptr, 0, &begin, &size), "heap") || begin != reinterpret_cast<std::uintptr_t>(p) || size != 257) return 13;
    MEMORY_BASIC_INFORMATION heapMemory{};
    if (VirtualQuery(p, &heapMemory, sizeof(heapMemory)) != sizeof(heapMemory)) return 14;
    __asan_free(p);
    if (!__asan_address_is_poisoned(p)) return 15;
    __sanitizer_purge_allocator();
    if (!Released(heapMemory.AllocationBase)) return 16;

    void* fakeBase = nullptr;
    int fakeResult = 0;
    std::thread thread([&] {
        const auto frame = __asan_stack_malloc_always_0(64);
        if (!frame || !GuestRange(frame, 64) || __asan_region_is_poisoned(frame, 64)) {
            fakeResult = 17;
            return;
        }
        __asan_memset(reinterpret_cast<void*>(frame), 0x42, 32);
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<void*>(frame), &memory, sizeof(memory)) != sizeof(memory)) {
            fakeResult = 18;
            return;
        }
        fakeBase = memory.AllocationBase;
        __asan_stack_free_0(frame, 64);
        if (!__asan_address_is_poisoned(reinterpret_cast<void*>(frame))) fakeResult = 19;
    });
    thread.join();
    if (fakeResult) return fakeResult;
    if (!fakeBase || !Released(fakeBase)) return 20;
    for (std::size_t i = 0; i < hostBytes; ++i) if (host[i] != 0x5a) return 21;
    std::printf("host allocation in shadow=%p; heap=%p; fake stack backing=%p; shadow hole preserved\n", static_cast<void*>(host), static_cast<void*>(p), fakeBase);
    if (!VirtualFree(host, 0, MEM_RELEASE)) return 22;
    return 0;
}
