#ifndef RELINKER_PACKAGEINPUT_HPP
#define RELINKER_PACKAGEINPUT_HPP

#include <relinker/guest/GuestImage.hpp>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Relinker {

inline constexpr char PackageSidecarName[] = "anyps5-package.ini";

struct PackageInput {
    std::vector<std::uint8_t> Executable;
    std::vector<GuestModuleSource> Modules;
    std::optional<std::vector<std::uint8_t>> Icon;
    std::string ContentId;
    std::uint64_t PackageSize = 0;
};

PackageInput ReadPackageInput(const std::filesystem::path& package, const std::filesystem::path& oodle, bool readModules, const std::set<std::string>& excludedModules);

std::string FormatPackageSidecar(const std::filesystem::path& package, const PackageInput& input, const std::filesystem::path& oodle);

}

#endif
