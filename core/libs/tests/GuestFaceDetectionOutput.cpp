#include "prx/libSceFace/include/Api.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int InvalidParameter = static_cast<int>(0x808b0002u);

char* GuardedPageEnd() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* memory = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(memory != nullptr, "the guarded region was not allocated");
    DWORD previous = 0;
    Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "the guard page was not protected");
#else
    const long nativePageSize = sysconf(_SC_PAGESIZE);
    Require(nativePageSize > 0, "the page size is unknown");
    const std::size_t pageSize = static_cast<std::size_t>(nativePageSize);
    void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(allocation != MAP_FAILED, "the guarded region was not allocated");
    auto* memory = static_cast<char*>(allocation);
    Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "the guard page was not protected");
#endif
    return memory + pageSize;
}

}

int main() {
    Require(sceFaceDetectionGetDefaultParam(nullptr) == InvalidParameter, "null output was accepted");
    errno = 71;
    Require(sceFaceDetectionGetDefaultParam(reinterpret_cast<void*>(0x10)) == InvalidParameter, "an invalid guest output address was written");
    Require(errno == 71, "invalid guest output changed errno");

    const std::array<std::uint32_t, 14> expected = {56u, 0x3f800000u, 0x3f574bc7u, 13200u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0x3f000000u, 1u, 0u};
    std::array<std::uint32_t, 16> actual{};
    actual.fill(0xaaaaaaaau);
    errno = 71;
    Require(sceFaceDetectionGetDefaultParam(actual.data()) == 0, "valid output was refused");
    Require(errno == 71, "valid output changed errno");
    Require(std::memcmp(actual.data(), expected.data(), sizeof(expected)) == 0, "the measured defaults changed");
    Require(actual[14] == 0xaaaaaaaau && actual[15] == 0xaaaaaaaau, "the defaults wrote past 0x38 bytes");

    char* pageEnd = GuardedPageEnd();
    std::memset(pageEnd - 0x38, 0xaa, 0x38);
    errno = 71;
    Require(sceFaceDetectionGetDefaultParam(pageEnd - 0x38) == 0, "an output ending at a guard page was refused");
    Require(errno == 71, "guard-page output changed errno");
    Require(std::memcmp(pageEnd - 0x38, expected.data(), sizeof(expected)) == 0, "the guard-page defaults changed");
    Require(sceFaceDetectionGetDefaultParam(pageEnd - 0x37) == InvalidParameter, "an output crossing a guard page was accepted");

    std::puts("Face detection guest output tests passed");
    return 0;
}
