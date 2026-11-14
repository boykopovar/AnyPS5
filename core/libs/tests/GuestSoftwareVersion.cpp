#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include <cerrno>
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
    Require(sceKernelGetProsperoSystemSwVersion_nid_postfix(reinterpret_cast<SceKernelProsperoSystemSwVersion*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
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

namespace {

alignas(16) std::array<std::byte, 0x40> parameters{};

}

static void CheckCompiledSdkVersions() {
    const std::uint64_t size = parameters.size();
    const std::uint32_t magic = 0x4942524f;
    const std::uint32_t compiledSdkVersion = 0x07008001;
    const std::uint32_t prosperoSdkVersion = 0x0a008001;
    std::memcpy(parameters.data(), &size, sizeof(size));
    std::memcpy(parameters.data() + 8, &magic, sizeof(magic));
    std::memcpy(parameters.data() + 0x10, &compiledSdkVersion, sizeof(compiledSdkVersion));
    std::memcpy(parameters.data() + 0x14, &prosperoSdkVersion, sizeof(prosperoSdkVersion));
    Require(ProcessParametersFixture::Install(parameters.data(), parameters.size()));

    Require(sceKernelGetCompiledSdkVersion(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetCompiledSdkVersion(reinterpret_cast<std::int32_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    std::array<std::int32_t, 2> output{-1, 0x5a5a5a5a};
    Require(sceKernelGetCompiledSdkVersion(output.data()) == 0);
    Require(output[0] == static_cast<std::int32_t>(compiledSdkVersion));
    Require(output[1] == 0x5a5a5a5a);

    errno = 0x44;
    Require(sceKernelGetProsperoCompiledSdkVersion(nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetProsperoCompiledSdkVersion(reinterpret_cast<std::int32_t*>(0x10)) == SCE_KERNEL_ERROR_EFAULT);
    output = {-1, 0x5a5a5a5a};
    Require(sceKernelGetProsperoCompiledSdkVersion(output.data()) == 0);
    Require(output[0] == static_cast<std::int32_t>(prosperoSdkVersion));
    Require(output[1] == 0x5a5a5a5a);
    Require(errno == 0x44);
}

static void CheckTitleWorkaround() {
    KernelTitleWorkaround workaround{1, 0, {(1ull << 3) | (1ull << 0x39) | (1ull << 63), (1ull << 0) | (1ull << 0x3F)}};
    std::int32_t enabled = -1;
    Require(sceKernelTitleWorkaroundIsEnabled(nullptr, 3, &enabled) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 3, nullptr) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(reinterpret_cast<const KernelTitleWorkaround*>(0x10), 3, &enabled) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 3, reinterpret_cast<std::int32_t*>(0x10)) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(nullptr, 0x80, &enabled) == static_cast<int>(0x8002000E));
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x80, &enabled) == static_cast<int>(0x80020016) && enabled == -1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, -1, &enabled) == static_cast<int>(0x80020016) && enabled == -1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 3, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 4, &enabled) == 0 && enabled == 0);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x39, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x3A, &enabled) == 0 && enabled == 0);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 63, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 64, &enabled) == 0 && enabled == 1);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 65, &enabled) == 0 && enabled == 0);
    enabled = -1;
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x7E, &enabled) == 0 && enabled == 0);
    Require(sceKernelTitleWorkaroundIsEnabled(&workaround, 0x7F, &enabled) == 0 && enabled == 1);

}

int main() {
    CheckCompiledSdkVersions();
    CheckProsperoSystemSwVersion();
    CheckAllowedSdkVersionOnSystem();
    CheckTitleWorkaround();
    return 0;
}
