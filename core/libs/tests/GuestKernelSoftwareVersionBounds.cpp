#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static_assert(sizeof(SceKernelProsperoSystemSwVersion) == 0x28);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, size) == 0x00);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, versionString) == 0x08);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, version) == 0x24);

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestProsperoSystemSwVersionNullPointer() {
    *__error_nid_postfix() = 42;
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(nullptr) == 0);
    Require(*__error_nid_postfix() == 42);
}

void TestProsperoSystemSwVersionUnwritablePointer() {
    *__error_nid_postfix() = 42;
    auto* const badPtr1 = reinterpret_cast<SceKernelProsperoSystemSwVersion*>(static_cast<std::uintptr_t>(0x10));
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(badPtr1) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);

    auto* const badPtr2 = reinterpret_cast<SceKernelProsperoSystemSwVersion*>(static_cast<std::uintptr_t>(0x1000));
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(badPtr2) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);
}

void TestProsperoAddressBounds() {
    constexpr auto maximum = (std::numeric_limits<std::uintptr_t>::max)();
    *__error_nid_postfix() = 42;
    for (const auto address : {maximum - 3, maximum - 0x20}) {
        auto* const version = reinterpret_cast<SceKernelProsperoSystemSwVersion*>(address);
        Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(version) == SCE_KERNEL_ERROR_EFAULT);
        Require(*__error_nid_postfix() == 42);
    }
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(reinterpret_cast<std::uint32_t*>(maximum - 1)) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);
}

void TestProsperoProtectedPages() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* const pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr);
    DWORD previous{};
    Require(VirtualProtect(pages, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    const long size = sysconf(_SC_PAGESIZE);
    Require(size > 0);
    const auto pageSize = static_cast<std::size_t>(size);
    auto* const pages = static_cast<unsigned char*>(mmap(nullptr, 2 * pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    Require(mprotect(pages, pageSize, PROT_NONE) == 0);
#endif
    auto* const boundary = pages + pageSize;
    *__error_nid_postfix() = 42;
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(reinterpret_cast<SceKernelProsperoSystemSwVersion*>(boundary - 8)) == 0);
    Require(*__error_nid_postfix() == 42);
    Require(std::strcmp(reinterpret_cast<const char*>(boundary), "01.000.000") == 0);
    std::uint32_t packed{};
    std::memcpy(&packed, boundary + 0x1c, sizeof(packed));
    Require(packed == 0x01000000);

#ifdef _WIN32
    Require(VirtualProtect(pages, pageSize, PAGE_READWRITE, &previous) != 0);
    Require(VirtualProtect(boundary, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
    Require(mprotect(pages, pageSize, PROT_READ | PROT_WRITE) == 0);
    Require(mprotect(boundary, pageSize, PROT_NONE) == 0);
#endif
    *__error_nid_postfix() = 42;
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(reinterpret_cast<SceKernelProsperoSystemSwVersion*>(boundary - 0x18)) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, 2 * pageSize) == 0);
#endif
}

void TestProsperoSystemSwVersionBlockPreserveSemantics() {
    struct Container {
        std::uint8_t prefix[0x20];
        SceKernelProsperoSystemSwVersion version;
        std::uint8_t suffix[0x40];
    };

    const std::uint64_t sizes[] = {
        0, 1, 0x18, 0x27, 0x28, 0x29, 0x30, 0x40, 0x100,
        0x123456789abcdef0ull, ~0ull
    };

    for (const std::uint64_t inputSize : sizes) {
        Container container{};
        std::memset(container.prefix, 0x5a, sizeof(container.prefix));
        std::memset(container.suffix, 0xa5, sizeof(container.suffix));
        std::memset(&container.version, 0xcc, sizeof(container.version));
        container.version.size = inputSize;

        *__error_nid_postfix() = 99;
        const int result = sceKernelGetProsperoSystemSwVersion_nid_postfix(&container.version);
        Require(result == 0);
        Require(*__error_nid_postfix() == 99);

        Require(container.version.size == inputSize);
        Require(std::strcmp(container.version.versionString, "01.000.000") == 0);

        for (std::size_t i = sizeof("01.000.000"); i < sizeof(container.version.versionString); ++i) {
            Require(container.version.versionString[i] == 0);
        }

        Require(container.version.version == 0x01000000);

        for (const std::uint8_t byte : container.prefix) {
            Require(byte == 0x5a);
        }
        for (const std::uint8_t byte : container.suffix) {
            Require(byte == 0xa5);
        }
    }
}

void TestAllowedSdkVersionOnSystemNullPointer() {
    *__error_nid_postfix() = 42;
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 42);
}

void TestAllowedSdkVersionOnSystemUnwritablePointer() {
    *__error_nid_postfix() = 42;
    auto* const badPtr1 = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x10));
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(badPtr1) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);

    auto* const badPtr2 = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(0x1000));
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(badPtr2) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 42);
}

void TestAllowedSdkVersionOnSystemSuccess() {
    struct Container {
        std::uint8_t prefix[0x10];
        std::uint32_t version;
        std::uint8_t suffix[0x10];
    };

    Container container{};
    std::memset(container.prefix, 0x33, sizeof(container.prefix));
    std::memset(container.suffix, 0x77, sizeof(container.suffix));
    container.version = 0xdeadbeefu;

    *__error_nid_postfix() = 77;
    const int result = sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(&container.version);
    Require(result == 0);
    Require(*__error_nid_postfix() == 77);

    Require(container.version == 0x01000fffu);

    for (const std::uint8_t byte : container.prefix) {
        Require(byte == 0x33);
    }
    for (const std::uint8_t byte : container.suffix) {
        Require(byte == 0x77);
    }

    SceKernelProsperoSystemSwVersion prosperoSw{};
    prosperoSw.size = sizeof(prosperoSw);
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(&prosperoSw) == 0);
    Require(container.version == (prosperoSw.version | 0xfffu));
    Require((container.version >> 16) == (prosperoSw.version >> 16));
}

} // namespace

int main() {
    TestProsperoSystemSwVersionNullPointer();
    TestProsperoSystemSwVersionUnwritablePointer();
    TestProsperoAddressBounds();
    TestProsperoProtectedPages();
    TestProsperoSystemSwVersionBlockPreserveSemantics();
    TestAllowedSdkVersionOnSystemNullPointer();
    TestAllowedSdkVersionOnSystemUnwritablePointer();
    TestAllowedSdkVersionOnSystemSuccess();
    return 0;
}
