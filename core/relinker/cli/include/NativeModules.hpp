#pragma once

#include <relinker/guest/GuestImage.hpp>

namespace Cli {

struct NativeModules {
    std::filesystem::path Root;
    std::vector<std::filesystem::path> RuntimeDirectories;
    std::vector<Relinker::GuestModuleInput> Modules;
};

bool SameDirectory(const std::filesystem::path& first, const std::filesystem::path& second);

NativeModules ReadModules(const std::filesystem::path& inputPath, const std::filesystem::path& sceModulePath, const std::set<std::string>& excludedModules, const std::vector<std::string>& moduleDirectories);

}
