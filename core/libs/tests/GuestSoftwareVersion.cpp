#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

static_assert(offsetof(SceKernelProsperoSystemSwVersion, versionString) == 0x08);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, version) == 0x24);
static_assert(sizeof(SceKernelProsperoSystemSwVersion) == 0x28);

static void Require(bool value) { if (!value) std::abort(); }

static void CheckProsperoSystemSwVersion() {
    *__error_nid_postfix() = 13;
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(nullptr) == 0);
    const std::uint64_t sizes[] = {0, 0x18, 0x27, 0x28, 0x29, 0x30, 0x100, ~0ull};
    for (const std::uint64_t size : sizes) {
        struct {
            SceKernelProsperoSystemSwVersion version;
            unsigned char suffix[0x40];
        } storage;
        std::memset(&storage, 0xaa, sizeof(storage));
        storage.version.size = size;
        Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(&storage.version) == 0);
        Require(storage.version.size == size);
        Require(std::strcmp(storage.version.versionString, "01.000.000") == 0);
        for (std::size_t i = sizeof("01.000.000"); i < sizeof(storage.version.versionString); ++i) Require(storage.version.versionString[i] == 0);
        Require(storage.version.version == 0x01000000);
        for (const unsigned char byte : storage.suffix) Require(byte == 0xaa);
    }
    Require(*__error_nid_postfix() == 13);
}

static void CheckAllowedSdkVersionOnSystem() {
    *__error_nid_postfix() = 13;
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(reinterpret_cast<std::uint32_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    std::uint32_t allowed = 0xaaaaaaaa;
    Require(sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(&allowed) == 0);
    SceKernelProsperoSystemSwVersion system{};
    system.size = sizeof(system);
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(&system) == 0);
    Require(allowed == (system.version | 0xfff) && allowed >> 16 == system.version >> 16);
    Require(*__error_nid_postfix() == 13);
}

int main() {
    CheckProsperoSystemSwVersion();
    CheckAllowedSdkVersionOnSystem();
    return 0;
}
