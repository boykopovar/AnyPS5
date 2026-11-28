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
#include <system_error>
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

inline bool HasConfiguredEntitlements() {
    const auto* configured = ConfiguredEntitlements();
    return configured != nullptr && configured[0] != 0;
}

inline std::filesystem::path EntitlementsPath() {
    if (HasConfiguredEntitlements()) return ConfiguredEntitlements();
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
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) {
        throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
    }
    std::ifstream file(path);
    if (!file) {
        error.clear();
        if (HasConfiguredEntitlements() || std::filesystem::exists(path, error) || error) throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
        return list;
    }
    bool firstLine = true;
    std::string line;
    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
        }
        line.erase(std::min(line.find_first_of("#;"), line.size()));
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        const auto trimmed = line.substr(first, line.find_last_not_of(" \t\r") + 1 - first);
        const auto delimiter = trimmed.find_first_of("= \t");
        const auto label = delimiter == std::string::npos ? trimmed : trimmed.substr(0, delimiter);
        if (label.empty()) throw std::runtime_error("NpEntitlementAccess: empty entitlement label in " + path.string());
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
    if (file.bad()) {
        throw std::runtime_error("NpEntitlementAccess: cannot read " + path.string());
    }
    return list;
}

}
