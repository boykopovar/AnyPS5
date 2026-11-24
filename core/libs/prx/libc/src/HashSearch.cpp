#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int FindAction = 0;
constexpr int EnterAction = 1;

struct HashTable {
    HsearchEntry* entries = nullptr;
    std::size_t capacity = 0;
    std::size_t used = 0;
};

HashTable table;

std::string CopyKey(const char* key) {
    std::string text;
    if (CopyGuestString_nid_no_patch(key, 4096, text) != 0) throw std::invalid_argument("hsearch: unreadable key");
    return text;
}

std::size_t Hash(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash);
}

}

int APS5_VABI hcreate_nid_postfix(std::size_t nel) {
    if (table.entries != nullptr) {
        errno = 17;
        return 0;
    }
    constexpr std::size_t MinimumSlots = 16;
    if (nel > std::numeric_limits<std::size_t>::max() / 4) {
        errno = 12;
        return 0;
    }
    std::size_t slots = MinimumSlots;
    while (slots < nel * 2) slots <<= 1;
    auto* entries = static_cast<HsearchEntry*>(calloc_nid_postfix(slots, sizeof(HsearchEntry)));
    if (entries == nullptr) {
        errno = 12;
        return 0;
    }
    table = HashTable{entries, slots, 0};
    return 1;
}

HsearchEntry* APS5_VABI hsearch_nid_postfix(HsearchEntry item, int action) {
    if (table.entries == nullptr) {
        errno = 3;
        return nullptr;
    }
    if (action != FindAction && action != EnterAction) {
        errno = 22;
        return nullptr;
    }
    if (item.key == nullptr) throw std::invalid_argument("hsearch: null key");
    const std::string key = CopyKey(item.key);
    std::size_t index = Hash(key) % table.capacity;
    for (std::size_t probes = 0; probes < table.capacity; ++probes) {
        HsearchEntry& slot = table.entries[index];
        if (slot.key == nullptr) {
            if (action == FindAction) return nullptr;
            slot = item;
            ++table.used;
            return &slot;
        }
        if (CopyKey(slot.key) == key) return &slot;
        index = (index + 1) % table.capacity;
    }
    if (action == EnterAction) errno = 12;
    return nullptr;
}

void APS5_VABI hdestroy_nid_postfix(void) {
    if (table.entries == nullptr) return;
    free_nid_postfix(table.entries);
    table = HashTable{};
}
