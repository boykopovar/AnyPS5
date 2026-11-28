#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>

static_assert(offsetof(SceKernelProsperoSystemSwVersion, size) == 0x00);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, versionString) == 0x08);
static_assert(offsetof(SceKernelProsperoSystemSwVersion, version) == 0x24);
static_assert(sizeof(SceKernelProsperoSystemSwVersion) == 0x28);

namespace {

constexpr std::uint32_t DefaultFirmwareVersion = 0x01000000;
constexpr std::uint32_t SdkPatchMask = 0xfff;

std::optional<std::uint32_t> ReadGuestFirmwareVersion() {
    const char* configured = std::getenv("ANYPS5_FIRMWARE_VERSION");
    if (configured == nullptr || *configured == '\0') return DefaultFirmwareVersion;

    const std::string_view text(configured);
    if (text.size() != 10 || text[2] != '.' || text[6] != '.') return std::nullopt;

    std::uint32_t packed = 0;
    for (const auto field : {text.substr(0, 2), text.substr(3, 3), text.substr(7, 3)}) {
        for (const char digit : field) {
            if (digit < '0' || digit > '9') return std::nullopt;
            packed = (packed << 4) | static_cast<std::uint32_t>(digit - '0');
        }
    }
    return packed;
}

const std::optional<std::uint32_t> ConfiguredFirmwareVersion = ReadGuestFirmwareVersion();

std::uint32_t GuestFirmwareVersion() {
    if (!ConfiguredFirmwareVersion) throw std::invalid_argument("ANYPS5_FIRMWARE_VERSION must use the MM.mmm.ppp decimal format");
    return *ConfiguredFirmwareVersion;
}

struct VersionPayload {
    char versionString[sizeof(SceKernelProsperoSystemSwVersion::versionString)]{};
    std::uint32_t version{};
};
static_assert(sizeof(VersionPayload) == sizeof(SceKernelProsperoSystemSwVersion) - offsetof(SceKernelProsperoSystemSwVersion, versionString));
static_assert(offsetof(VersionPayload, versionString) == 0);
static_assert(offsetof(VersionPayload, version) == offsetof(SceKernelProsperoSystemSwVersion, version) - offsetof(SceKernelProsperoSystemSwVersion, versionString));

}

extern "C" {

int APS5_VABI sceKernelGetProsperoSystemSwVersion_nid_postfix(SceKernelProsperoSystemSwVersion* version) {
    if (version == nullptr) return 0;
    constexpr auto offset = offsetof(SceKernelProsperoSystemSwVersion, versionString);
    const auto address = reinterpret_cast<std::uintptr_t>(version);
    if (address > std::numeric_limits<std::uintptr_t>::max() - offset - sizeof(VersionPayload)) {
        return SCE_KERNEL_ERROR_EFAULT;
    }
    const int saved = errno;
    VersionPayload payload{};
    payload.version = GuestFirmwareVersion();
    const auto packed = static_cast<unsigned int>(payload.version);
    std::snprintf(payload.versionString, sizeof(payload.versionString), "%02x.%03x.%03x",
        packed >> 24, (packed >> 12) & SdkPatchMask, packed & SdkPatchMask);
    const bool copied = GuestCopyOut_nid_no_patch(reinterpret_cast<void*>(address + offset), &payload, sizeof(payload));
    errno = saved;
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI sceKernelGetAllowedSdkVersionOnSystem_nid_postfix(std::uint32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    const std::uint32_t value = GuestFirmwareVersion() | SdkPatchMask;
    const bool copied = GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    if (!copied) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

}
