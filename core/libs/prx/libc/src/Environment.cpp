#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace {
struct EnvironmentEntry {
    std::unique_ptr<char[]> owned;
    char* borrowed = nullptr;
    std::size_t valueSize = 0;
    bool active = true;
    char* Data() const { return borrowed ? borrowed : owned.get(); }
    bool Matches(std::string_view name) const {
        const char* data = Data();
        const char* separator = std::strchr(data, '=');
        return separator && std::string_view(data, separator - data) == name;
    }
};
constexpr std::size_t missingEntry = static_cast<std::size_t>(-1);
std::mutex environmentMutex;
std::vector<EnvironmentEntry>& Environment() {
    static std::vector<EnvironmentEntry> entries;
    return entries;
}
bool ValidEnvironmentName(const char* name) {
    return name && *name && !std::strchr(name, '=');
}
int EnvironmentError(int error) { errno = error; return -1; }
std::size_t LatestEntry(std::string_view name, bool onlyActive) {
    const auto& entries = Environment();
    for (std::size_t index = entries.size(); index-- > 0;)
        if ((entries[index].active || !onlyActive) && entries[index].Matches(name)) return index;
    return missingEntry;
}
char* ActiveValue(const char* name) {
    const std::size_t found = LatestEntry(name, true);
    return found == missingEntry ? nullptr : Environment()[found].Data() + std::strlen(name) + 1;
}
}

extern "C" {
bool CopyGuestEnvironment_nid_no_patch(const char* name, std::string& value) {
    if (!ValidEnvironmentName(name)) return false;
    std::lock_guard lock(environmentMutex);
    const char* found = ActiveValue(name);
    if (!found) return false;
    value = found;
    return true;
}

char* APS5_VABI getenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) { errno = 22; return nullptr; }
    std::lock_guard lock(environmentMutex);
    return ActiveValue(name);
}

int APS5_VABI setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    if (!ValidEnvironmentName(name) || !value) return EnvironmentError(22);
    try {
        std::lock_guard lock(environmentMutex);
        auto& entries = Environment();
        const std::size_t nameLength = std::strlen(name);
        const std::size_t valueLength = std::strlen(value);
        const std::size_t found = LatestEntry(name, false);
        if (found != missingEntry) {
            auto& entry = entries[found];
            if (entry.active && !overwrite) return 0;
            if (!entry.borrowed && entry.valueSize >= valueLength) {
                std::memmove(entry.owned.get() + nameLength + 1, value, valueLength + 1);
                entry.active = true;
                return 0;
            }
        }
        EnvironmentEntry created{std::make_unique<char[]>(nameLength + valueLength + 2), nullptr, valueLength};
        std::memcpy(created.owned.get(), name, nameLength);
        created.owned[nameLength] = '=';
        std::memcpy(created.owned.get() + nameLength + 1, value, valueLength + 1);
        entries.push_back(std::move(created));
        if (found != missingEntry) {
            if (entries[found].borrowed) entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(found));
            else entries[found].active = false;
        }
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}

int APS5_VABI unsetenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) return EnvironmentError(22);
    std::lock_guard lock(environmentMutex);
    auto& entries = Environment();
    const std::string_view key(name);
    std::erase_if(entries, [key](const EnvironmentEntry& entry) { return entry.borrowed && entry.Matches(key); });
    for (auto& entry : entries)
        if (entry.Matches(key)) entry.active = false;
    return 0;
}

int APS5_VABI putenv_nid_postfix(char* text) {
    const char* separator = text ? std::strchr(text, '=') : nullptr;
    if (!separator || separator == text) return EnvironmentError(22);
    try {
        std::lock_guard lock(environmentMutex);
        auto& entries = Environment();
        const std::size_t found = LatestEntry(std::string_view(text, separator - text), true);
        if (found != missingEntry && entries[found].borrowed) {
            entries[found].borrowed = text;
            return 0;
        }
        entries.push_back({nullptr, text});
        if (found != missingEntry) entries[found].active = false;
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}
}
