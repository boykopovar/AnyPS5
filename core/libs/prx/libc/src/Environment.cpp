#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <cerrno>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
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
    std::size_t capacity = 0;
    bool borrowed = false;
};
struct GuestEnvironment {
    std::mutex mutex;
    char* empty[1]{};
    char** fallback = empty;
    char*** storage = &fallback;
    std::vector<EnvironmentEntry> owned;
    std::unique_ptr<char*[]> published;
};

GuestEnvironment& Environment() {
    static GuestEnvironment state;
    return state;
}

bool Matches(const char* entry, std::string_view name) {
    const char* separator = std::strchr(entry, '=');
    return separator && std::string_view(entry, separator - entry) == name;
}

char* Find(GuestEnvironment& environment, std::string_view name) {
    auto* entries = *environment.storage;
    if (entries == nullptr) return nullptr;
    for (; *entries != nullptr; ++entries)
        if (Matches(*entries, name)) return *entries;
    return nullptr;
}

std::vector<char*> Entries(GuestEnvironment& environment) {
    std::vector<char*> entries;
    if (auto* current = *environment.storage)
        for (; *current != nullptr; ++current) entries.push_back(*current);
    return entries;
}

std::unique_ptr<char*[]> Table(const std::vector<char*>& entries) {
    auto table = std::make_unique<char*[]>(entries.size() + 1);
    std::copy(entries.begin(), entries.end(), table.get());
    return table;
}

void Publish(GuestEnvironment& environment, std::unique_ptr<char*[]> table) {
    environment.published.swap(table);
    *environment.storage = environment.published.get();
}

bool Contains(const EnvironmentEntry& entry, const char* pointer) {
    const auto start = reinterpret_cast<std::uintptr_t>(entry.owned.get());
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    return address >= start && address - start < entry.capacity;
}

EnvironmentEntry* Reusable(GuestEnvironment& environment, std::string_view name, const char* current) {
    for (auto entry = environment.owned.rbegin(); entry != environment.owned.rend(); ++entry) {
        if (current ? entry->owned.get() != current : !Matches(entry->owned.get(), name)) continue;
        if (entry->borrowed) return nullptr;
        if (auto* entries = *environment.storage)
            for (; *entries != nullptr; ++entries)
                if (*entries != entry->owned.get() && Contains(*entry, *entries)) return nullptr;
        return &*entry;
    }
    return nullptr;
}

void Replace(std::vector<char*>& entries, std::string_view name, char* replacement) {
    auto found = std::find_if(entries.begin(), entries.end(), [name](const char* entry) { return Matches(entry, name); });
    if (found == entries.end()) {
        entries.push_back(replacement);
        return;
    }
    *found = replacement;
    entries.erase(std::remove_if(std::next(found), entries.end(), [name](const char* entry) { return Matches(entry, name); }), entries.end());
}

bool ValidEnvironmentName(const char* name) {
    return name && *name && !std::strchr(name, '=');
}
int EnvironmentError(int error) { errno = error; return -1; }
}

extern "C" {
void BindGuestEnvironment_nid_no_patch(char*** storage) {
    auto& environment = Environment();
    std::lock_guard lock(environment.mutex);
    if (storage == environment.storage) return;
    if (storage == nullptr) {
        environment.fallback = *environment.storage;
        environment.storage = &environment.fallback;
    } else {
        *storage = *environment.storage;
        environment.storage = storage;
    }
}

bool CopyGuestEnvironment_nid_no_patch(const char* name, std::string& value) {
    if (!ValidEnvironmentName(name)) return false;
    auto& environment = Environment();
    std::lock_guard lock(environment.mutex);
    if (const char* found = Find(environment, name)) {
        value = found + std::strlen(name) + 1;
        return true;
    }
    return false;
}

char* APS5_VABI getenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) { errno = 22; return nullptr; }
    auto& environment = Environment();
    std::lock_guard lock(environment.mutex);
    char* const found = Find(environment, name);
    return found ? found + std::strlen(name) + 1 : nullptr;
}

int APS5_VABI setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    if (!ValidEnvironmentName(name) || !value) return EnvironmentError(22);
    try {
        auto& environment = Environment();
        std::lock_guard lock(environment.mutex);
        const std::string_view key(name);
        char* const current = Find(environment, key);
        if (current && !overwrite) return 0;
        const std::size_t valueLength = std::strlen(value);
        if (key.size() > std::numeric_limits<std::size_t>::max() - 2 || valueLength > std::numeric_limits<std::size_t>::max() - key.size() - 2)
            return EnvironmentError(12);
        const std::size_t bytes = key.size() + valueLength + 2;
        auto* reused = Reusable(environment, key, current);
        if (reused && reused->capacity < bytes) reused = nullptr;
        EnvironmentEntry created;
        if (!reused) {
            created.owned = std::make_unique<char[]>(bytes);
            created.capacity = bytes;
            std::memcpy(created.owned.get(), name, key.size());
            created.owned[key.size()] = '=';
            std::memcpy(created.owned.get() + key.size() + 1, value, valueLength + 1);
        }
        char* const replacement = reused ? reused->owned.get() : created.owned.get();
        auto entries = Entries(environment);
        Replace(entries, key, replacement);
        auto table = Table(entries);
        if (reused) std::memmove(replacement + key.size() + 1, value, valueLength + 1);
        else environment.owned.push_back(std::move(created));
        Publish(environment, std::move(table));
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}

int APS5_VABI unsetenv_nid_postfix(const char* name) {
    if (!ValidEnvironmentName(name)) return EnvironmentError(22);
    try {
        auto& environment = Environment();
        std::lock_guard lock(environment.mutex);
        const std::string_view key(name);
        if (Find(environment, key) == nullptr) return 0;
        auto entries = Entries(environment);
        std::erase_if(entries, [key](const char* entry) { return Matches(entry, key); });
        Publish(environment, Table(entries));
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}

int APS5_VABI putenv_nid_postfix(char* text) {
    const char* separator = text ? std::strchr(text, '=') : nullptr;
    if (!separator || separator == text) return EnvironmentError(22);
    try {
        auto& environment = Environment();
        std::lock_guard lock(environment.mutex);
        const std::string_view key(text, separator - text);
        auto entries = Entries(environment);
        Replace(entries, key, text);
        auto table = Table(entries);
        for (auto& entry : environment.owned)
            if (Contains(entry, text)) entry.borrowed = true;
        Publish(environment, std::move(table));
        return 0;
    } catch (const std::bad_alloc&) { return EnvironmentError(12); }
}
}
