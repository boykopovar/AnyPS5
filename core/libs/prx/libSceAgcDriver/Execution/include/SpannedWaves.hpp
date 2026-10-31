#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SPANNEDWAVES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SPANNEDWAVES_HPP

#include <vulkan/vulkan.h>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

namespace AgcDriver {

enum class SpannedWaveMode { FusedThreadTables, All, Never, Tables };

struct MesaVersion {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
};

struct FusedThreadHandles {
    bool shared = false;
    bool versionUnknown = false;
};

struct SpannedWaveTargets {
    bool all = false;
    bool tables = false;
};

inline std::optional<SpannedWaveMode> ParseSpannedWaveMode(const char* value) {
    if (value == nullptr) return SpannedWaveMode::FusedThreadTables;
    const std::string_view text(value);
    if (text == "1") return SpannedWaveMode::All;
    if (text == "0") return SpannedWaveMode::Never;
    if (text == "tables") return SpannedWaveMode::Tables;
    return std::nullopt;
}

inline std::optional<MesaVersion> ParseMesaVersion(std::string_view driverInfo) {
    constexpr std::string_view Prefix = "Mesa ";
    for (auto at = driverInfo.find(Prefix); at != std::string_view::npos; at = driverInfo.find(Prefix, at + 1u)) {
        const auto* begin = driverInfo.data() + at + Prefix.size();
        const auto* end = driverInfo.data() + driverInfo.size();
        MesaVersion version;
        const auto major = std::from_chars(begin, end, version.major);
        if (major.ec != std::errc{} || major.ptr == end || *major.ptr != '.') continue;
        const auto minor = std::from_chars(major.ptr + 1, end, version.minor);
        if (minor.ec != std::errc{}) continue;
        return version;
    }
    return std::nullopt;
}

inline FusedThreadHandles FusedThreadHandleSharing(VkDriverId driver, std::string_view driverInfo, std::uint32_t minSubgroupSize, bool computeFullSubgroups) {
    if (!computeFullSubgroups) return {};
    if (driver == VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS) return {true, false};
    if (driver != VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA || minSubgroupSize >= 16u) return {};
    const auto version = ParseMesaVersion(driverInfo);
    if (!version) return {true, true};
    return {version->major < 25u || (version->major == 25u && version->minor < 3u), false};
}

inline SpannedWaveTargets SpannedWaveTargetsFor(SpannedWaveMode mode, bool computeFullSubgroups, bool fusedThreadHandles) {
    if (!computeFullSubgroups) return {};
    switch (mode) {
    case SpannedWaveMode::All: return {true, false};
    case SpannedWaveMode::Never: return {};
    case SpannedWaveMode::Tables: return {false, true};
    case SpannedWaveMode::FusedThreadTables: return {false, fusedThreadHandles};
    }
    return {};
}

}

#endif
