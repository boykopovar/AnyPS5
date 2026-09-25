#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <filesystem>

#include "prx/libc/include/General.hpp"

namespace {

// Guest prefixes (without leading slashes) mapped to host directories, e.g. save-data mount points:
// the PS5 hands the title a short mount point ("/_sm/0") whose files live under _sd/<dir name>.
struct PathAliases {
    std::mutex mutex;
    std::vector<std::pair<std::string, std::string>> entries;
};

PathAliases& Aliases() {
    static PathAliases aliases;
    return aliases;
}

std::string TrimSlashes(const char* path) {
    std::string s(path);
    std::size_t start = 0;
    while (start < s.size() && (s[start] == '/' || s[start] == '\\')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == '/' || s[end - 1] == '\\')) {
        --end;
    }
    return s.substr(start, end - start);
}

}

extern "C" void AddPathAlias_nid_no_patch(const char* guestPrefix, const char* hostPath) {
    if (guestPrefix == nullptr || hostPath == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    const auto prefix = TrimSlashes(guestPrefix);
    for (auto& entry : aliases.entries) {
        if (entry.first == prefix) {
            entry.second = hostPath;
            return;
        }
    }
    aliases.entries.emplace_back(prefix, hostPath);
}

extern "C" void RemovePathAlias_nid_no_patch(const char* guestPrefix) {
    if (guestPrefix == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& aliases = Aliases();
    std::lock_guard lock(aliases.mutex);
    const auto prefix = TrimSlashes(guestPrefix);
    std::erase_if(aliases.entries, [&](const auto& entry) { return entry.first == prefix; });
}

extern "C" std::filesystem::path ResolvePath_nid_no_patch(const char* path) {
    if (path == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const auto relative = TrimSlashes(path);
    {
        auto& aliases = Aliases();
        std::lock_guard lock(aliases.mutex);
        for (const auto& [prefix, host] : aliases.entries) {
            if (relative.size() < prefix.size() || relative.compare(0, prefix.size(), prefix) != 0) continue;
            if (relative.size() == prefix.size()) return std::filesystem::path(host).make_preferred();
            if (relative[prefix.size()] != '/' && relative[prefix.size()] != '\\') continue;
            std::filesystem::path result = std::filesystem::path(host) / std::filesystem::path(relative.substr(prefix.size() + 1));
            return result.make_preferred();
        }
    }
    std::filesystem::path result = std::filesystem::current_path() / std::filesystem::path(relative);
    return result.make_preferred();
}

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName) + " not implemented");
}
