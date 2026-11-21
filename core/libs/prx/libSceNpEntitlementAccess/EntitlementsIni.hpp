#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace AnyPS5::Entitlements {

#ifdef _WIN32
inline const wchar_t* ConfiguredEntitlements() { return _wgetenv(L"ANYPS5_ENTITLEMENTS"); }
#else
inline const char* ConfiguredEntitlements() { return std::getenv("ANYPS5_ENTITLEMENTS"); }
#endif

inline std::filesystem::path EntitlementsPath() {
    if (const auto* configured = ConfiguredEntitlements(); configured != nullptr && configured[0] != 0) return configured;
#ifdef _WIN32
    wchar_t module[MAX_PATH];
    const auto length = GetModuleFileNameW(nullptr, module, MAX_PATH);
    if (length == 0 || length == MAX_PATH) throw std::runtime_error("NpEntitlementAccess: cannot locate the executable");
    return std::filesystem::path(module).parent_path() / "anyps5-entitlements.ini";
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> executable(size);
    std::error_code error;
    std::filesystem::path path;
    if (_NSGetExecutablePath(executable.data(), &size) == 0) path = std::filesystem::canonical(executable.data(), error);
    if (path.empty()) throw std::runtime_error("NpEntitlementAccess: cannot locate the executable");
    return path.parent_path() / "anyps5-entitlements.ini";
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path() / "anyps5-entitlements.ini";
#endif
}

struct ParsedEntry {
    std::string label;
    std::string target;
};

inline std::vector<ParsedEntry> ParseEntries() {
    std::vector<ParsedEntry> list;
    const auto path = EntitlementsPath();
    std::ifstream file(path);
    if (!file) {
        if (ConfiguredEntitlements() != nullptr || std::filesystem::exists(path)) throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
        return list;
    }
    std::string line;
    while (std::getline(file, line)) {
        line.erase(std::min(line.find_first_of("#;"), line.size()));
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        const auto trimmed = line.substr(first, line.find_last_not_of(" \t\r") + 1 - first);
        const auto delimiter = trimmed.find_first_of("= \t");
        const auto label = delimiter == std::string::npos ? trimmed : trimmed.substr(0, delimiter);
        if (label.size() >= 17) throw std::runtime_error("NpEntitlementAccess: entitlement label '" + label + "' in " + path.string() + " is longer than 16 characters");
        std::string target;
        if (delimiter != std::string::npos) {
            const auto targetStart = trimmed.find_first_not_of("= \t", delimiter);
            if (targetStart != std::string::npos) {
                target = trimmed.substr(targetStart);
            }
        }
        if (target.size() >= 16) throw std::runtime_error("NpEntitlementAccess: mount target '" + target + "' in " + path.string() + " is longer than 15 characters");
        list.push_back({label, target});
    }
    return list;
}

}

