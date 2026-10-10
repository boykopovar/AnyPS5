#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "prx/libc/include/general/VabiMacros.hpp"

#ifndef _WIN32
extern char** environ;
#endif

namespace {
struct EnvironmentEntry {
    std::string owned;
    char* borrowed = nullptr;
    char* Data() { return borrowed ? borrowed : owned.data(); }
    bool Matches(std::string_view name) {
        const char* data = Data();
        const char* separator = std::strchr(data, '=');
        return separator && std::string_view(data, separator - data) == name;
    }
};
std::mutex environmentMutex;
std::vector<EnvironmentEntry>& Environment() {
    // Seed once from the host. Guest mutations remain local to the guest runtime.
    static std::vector<EnvironmentEntry> entries = [] {
        std::vector<EnvironmentEntry> result;
#ifdef _WIN32
        char** source = _environ;
#else
        char** source = environ;
#endif
        if (source) for (; *source; ++source) {
            if (**source && **source != '=' && std::strchr(*source, '='))
                result.push_back({*source, nullptr});
        }
        return result;
    }();
    return entries;
}
bool ValidEnvironmentName(const char* name) {
    return name && *name && !std::strchr(name, '=');
}
int EnvironmentError(int error) { errno = error; return -1; }
}

extern "C" {
char* APS5_VABI getenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) return nullptr;
    try {
        std::lock_guard lock(environmentMutex);
        for (auto& entry : Environment())
            if (entry.Matches(name)) return entry.Data() + std::strlen(name) + 1;
        return nullptr;
    } catch (const std::bad_alloc&) { errno = 12; return nullptr; }
}

int APS5_VABI setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    if (!ValidEnvironmentName(name) || !value) return EnvironmentError(22);
    try {
        std::lock_guard lock(environmentMutex);
        auto& entries = Environment();
        auto found = std::find_if(entries.begin(), entries.end(),
            [name](auto& entry) { return entry.Matches(name); });
        if (found != entries.end() && !overwrite) return 0;
        EnvironmentEntry replacement{std::string(name) + '=' + value, nullptr};
        if (found != entries.end()) *found = std::move(replacement);
        else entries.push_back(std::move(replacement));
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}

int APS5_VABI unsetenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) return EnvironmentError(22);
    try {
        std::lock_guard lock(environmentMutex);
        auto& entries = Environment();
        const std::string_view key(name);
        std::erase_if(entries, [&key](auto& entry) { return entry.Matches(key); });
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}

int APS5_VABI putenv_nid_postfix(char* text) {
    const char* separator = text ? std::strchr(text, '=') : nullptr;
    if (!separator || separator == text) return EnvironmentError(22);
    try {
        std::lock_guard lock(environmentMutex);
        auto& entries = Environment();
        const std::string_view name(text, separator - text);
        auto found = std::find_if(entries.begin(), entries.end(),
            [name](auto& entry) { return entry.Matches(name); });
        EnvironmentEntry replacement{{}, text};
        if (found != entries.end()) *found = std::move(replacement);
        else entries.push_back(std::move(replacement));
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}
}
