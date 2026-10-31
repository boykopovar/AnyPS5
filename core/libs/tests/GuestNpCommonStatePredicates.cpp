#include "prx/libSceNpCommon/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "NP common state predicates: %s\n", message);
    std::exit(1);
}

template <typename TValue, std::size_t N>
void WriteField(std::array<std::byte, N>& object, std::size_t offset, TValue value) {
    std::memcpy(object.data() + offset, &value, sizeof(value));
}

void WriteGuest(void* destination, const void* source, std::size_t bytes) {
    const GuestArena::HostWrite access(destination, bytes);
    Require(access.Open(), "guest fixture memory was not writable");
    std::memcpy(destination, source, bytes);
}

void CheckInaccessibleObjects() {
    const auto* invalid = reinterpret_cast<const void*>(std::uintptr_t{1});
    Require(!_ZNK3sce2np9EventFlag6IsInitEv(invalid), "inaccessible event flag fallback");
    Require(!_ZNK3sce2np7RingBuf6IsFullEv(invalid), "inaccessible ring buffer fallback");
    Require(!_ZNK3sce2np10EventQueue6IsInitEv(invalid), "inaccessible event queue fallback");
    Require(!_ZNK3sce2np5Mutex6IsInitEv(invalid), "inaccessible mutex fallback");
    Require(!_ZN3sce2np3ipc10IpmiClient11IsConnectedEv(const_cast<void*>(invalid)),
            "inaccessible IPMI client fallback");
    Require(!_ZN3sce2np7Callout10IsTimedoutEv(const_cast<void*>(invalid)),
            "inaccessible callout fallback");
    Require(!_ZN3sce2np13RingBufMemory6IsInitEv(const_cast<void*>(invalid)),
            "inaccessible ring buffer memory fallback");
    _ZN3sce2np13NpTitleSecret5ClearEv(const_cast<void*>(invalid));
    _ZN3sce2np8JsonNull5ClearEv(const_cast<void*>(invalid));

    const auto overflow = reinterpret_cast<void*>(std::numeric_limits<std::uintptr_t>::max() - 0x20);
    Require(!_ZNK3sce2np10EventQueue6IsInitEv(overflow), "overflowed event queue address fallback");
    _ZN3sce2np13NpTitleSecret5ClearEv(overflow);
}

void CheckPartiallyAccessibleTitleSecret() {
#if defined(_WIN32)
    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    const std::size_t pageSize = systemInfo.dwPageSize;
    Require(pageSize >= 0x80, "Windows page size");
    auto* pages = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT,
                                                       PAGE_READWRITE));
    Require(pages != nullptr, "Windows page allocation");
    std::memset(pages, 0x5a, pageSize * 2);
    DWORD previousProtection = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &previousProtection) != 0,
            "Windows page protection");
    _ZN3sce2np13NpTitleSecret5ClearEv(pages + pageSize - 0x48);
    for (std::size_t i = 0; i < pageSize; ++i)
        Require(pages[i] == std::byte{0x5a}, "Windows failed clear preserves accessible page");
    DWORD restoredProtection = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_READWRITE, &restoredProtection) != 0,
            "Windows page protection restore");
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0, "Windows page release");
#elif defined(__linux__)
    const long configuredPageSize = sysconf(_SC_PAGESIZE);
    Require(configuredPageSize >= 0x80, "Linux page size");
    const std::size_t pageSize = static_cast<std::size_t>(configuredPageSize);
    void* mapping = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                         -1, 0);
    Require(mapping != MAP_FAILED, "Linux page mapping");
    auto* pages = static_cast<std::byte*>(mapping);
    std::memset(pages, 0x5a, pageSize * 2);
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "Linux page protection");
    _ZN3sce2np13NpTitleSecret5ClearEv(pages + pageSize - 0x48);
    for (std::size_t i = 0; i < pageSize - 0x40; ++i)
        Require(pages[i] == std::byte{0x5a}, "Linux clear stays within target prefix");
    Require(mprotect(pages + pageSize, pageSize, PROT_READ | PROT_WRITE) == 0,
            "Linux page protection restore");
    for (std::size_t i = 0; i < pageSize; ++i)
        Require(pages[pageSize + i] == std::byte{0x5a}, "Linux clear does not modify protected page");
    Require(munmap(mapping, pageSize * 2) == 0, "Linux page unmapping");
#endif
}

}

int main() {
    std::array<std::byte, 0x20> eventFlag{};
    Require(!_ZNK3sce2np9EventFlag6IsInitEv(eventFlag.data()), "uninitialized event flag");
    WriteField(eventFlag, 0x10, std::int32_t{1});
    Require(_ZNK3sce2np9EventFlag6IsInitEv(eventFlag.data()), "initialized event flag");

    std::array<std::byte, 0x30> ringBuffer{};
    WriteField(ringBuffer, 0x20, std::uint64_t{12});
    WriteField(ringBuffer, 0x28, std::uint64_t{12});
    Require(_ZNK3sce2np7RingBuf6IsFullEv(ringBuffer.data()), "full ring buffer");
    WriteField(ringBuffer, 0x28, std::uint64_t{13});
    Require(!_ZNK3sce2np7RingBuf6IsFullEv(ringBuffer.data()), "ring buffer with available space");

    std::array<std::byte, 0xa0> eventQueue{};
    Require(!_ZNK3sce2np10EventQueue6IsInitEv(eventQueue.data()), "uninitialized event queue");
    WriteField(eventQueue, 0x98, std::uint16_t{1});
    Require(_ZNK3sce2np10EventQueue6IsInitEv(eventQueue.data()), "initialized event queue");

    std::array<std::byte, 0x20> mutex{};
    Require(!_ZNK3sce2np5Mutex6IsInitEv(mutex.data()), "uninitialized mutex");
    WriteField(mutex, 0x10, std::uint8_t{1});
    Require(_ZNK3sce2np5Mutex6IsInitEv(mutex.data()), "initialized mutex");

    std::array<std::byte, 0x20> ipmiClient{};
    Require(_ZN3sce2np3ipc10IpmiClient11IsConnectedEv(ipmiClient.data()), "connected IPMI client");
    WriteField(ipmiClient, 0x18, std::int32_t{1});
    Require(!_ZN3sce2np3ipc10IpmiClient11IsConnectedEv(ipmiClient.data()), "disconnected IPMI client");

    std::array<std::byte, 0x18> callout{};
    WriteField(callout, 0x10, std::int32_t{-2});
    Require(_ZN3sce2np7Callout10IsTimedoutEv(callout.data()), "callout in timed-out state -2");
    WriteField(callout, 0x10, std::int32_t{0});
    Require(!_ZN3sce2np7Callout10IsTimedoutEv(callout.data()), "callout in state zero");
    WriteField(callout, 0x10, std::int32_t{1});
    Require(_ZN3sce2np7Callout10IsTimedoutEv(callout.data()), "callout in positive state");
    WriteField(callout, 0x10, std::int32_t{-1});
    Require(!_ZN3sce2np7Callout10IsTimedoutEv(callout.data()), "callout in state -1");

    std::array<std::byte, 0x10> ringBufferMemory{};
    Require(!_ZN3sce2np13RingBufMemory6IsInitEv(ringBufferMemory.data()), "uninitialized ring buffer memory");
    WriteField(ringBufferMemory, 0x8, std::uintptr_t{0x1000});
    Require(_ZN3sce2np13RingBufMemory6IsInitEv(ringBufferMemory.data()), "initialized ring buffer memory");

    auto* titleSecret = static_cast<std::byte*>(GuestHeap::GuestHeapAllocate_nid_postfix(0x88));
    Require(titleSecret != nullptr, "title secret fixture allocation");
    std::array<std::byte, 0x88> titleSecretContents;
    titleSecretContents.fill(std::byte{0xa5});
    WriteGuest(titleSecret, titleSecretContents.data(), titleSecretContents.size());
    _ZN3sce2np13NpTitleSecret5ClearEv(titleSecret);
    std::array<std::byte, 0x88> clearedTitleSecret{};
    Require(GuestCopyIn_nid_no_patch(clearedTitleSecret.data(), titleSecret, clearedTitleSecret.size()),
            "title secret fixture readback");
    for (std::size_t i = 0; i < 8; ++i)
        Require(clearedTitleSecret[i] == std::byte{0xa5}, "title secret preserves its leading eight bytes");
    for (std::size_t i = 8; i < clearedTitleSecret.size(); ++i)
        Require(clearedTitleSecret[i] == std::byte{0}, "title secret clears bytes after the vtable");
    GuestHeap::GuestHeapFree_nid_postfix(titleSecret);

    std::array<std::byte, 0x20> jsonNull;
    jsonNull.fill(std::byte{0x5a});
    _ZN3sce2np8JsonNull5ClearEv(jsonNull.data());
    for (const auto value : jsonNull) Require(value == std::byte{0x5a}, "JSON null clear leaves state untouched");

    Require(!_ZNK3sce2np9EventFlag6IsInitEv(nullptr), "null event flag fallback");
    Require(!_ZNK3sce2np7RingBuf6IsFullEv(nullptr), "null ring buffer fallback");
    Require(!_ZNK3sce2np10EventQueue6IsInitEv(nullptr), "null event queue fallback");
    Require(!_ZNK3sce2np5Mutex6IsInitEv(nullptr), "null mutex fallback");
    Require(!_ZN3sce2np3ipc10IpmiClient11IsConnectedEv(nullptr), "null IPMI client fallback");
    Require(!_ZN3sce2np7Callout10IsTimedoutEv(nullptr), "null callout fallback");
    Require(!_ZN3sce2np13RingBufMemory6IsInitEv(nullptr), "null ring buffer memory fallback");
    _ZN3sce2np13NpTitleSecret5ClearEv(nullptr);
    _ZN3sce2np8JsonNull5ClearEv(nullptr);

    CheckInaccessibleObjects();
    CheckPartiallyAccessibleTitleSecret();

    std::puts("NP common state predicate tests passed");
    return 0;
}
