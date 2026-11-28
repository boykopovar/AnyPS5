// Adapted from FreeBSD lib/libc/stdlib/hcreate.c, hcreate_r.c, hsearch_r.c and hdestroy_r.c (release/11.4.0): https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/hsearch_r.c
/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr int FindAction = 0;
constexpr std::size_t InitialEntries = 16;

struct HashTable {
    std::size_t offsetBasis = 0;
    std::size_t indexMask = 0;
    std::size_t entriesUsed = 0;
    HsearchEntry* entries = nullptr;
};

HashTable table;

std::string CopyKey(const char* key) {
    std::string text;
    if (CopyGuestString_nid_no_patch(key, 4096, text) != 0) throw std::invalid_argument("hsearch: unreadable key");
    return text;
}

std::size_t Hash(std::size_t offsetBasis, const std::string& text) {
    std::uint64_t hash = offsetBasis;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash);
}

HsearchEntry* LookupFree(std::size_t hash) {
    for (std::size_t index = hash, step = 0;; index += ++step) {
        HsearchEntry* entry = &table.entries[index & table.indexMask];
        if (entry->key == nullptr) return entry;
    }
}

bool Create() {
    auto* entries = static_cast<HsearchEntry*>(calloc_nid_postfix(InitialEntries, sizeof(HsearchEntry)));
    if (entries == nullptr) return false;
    std::size_t offsetBasis = 0;
    HostEntropy_nid_no_patch(&offsetBasis, sizeof(offsetBasis));
    table = HashTable{offsetBasis, InitialEntries - 1, 0, entries};
    return true;
}

bool Grow() {
    const std::size_t oldCount = table.indexMask + 1;
    HsearchEntry* oldEntries = table.entries;
    const std::size_t newCount = oldCount * 2;
    auto* newEntries = static_cast<HsearchEntry*>(calloc_nid_postfix(newCount, sizeof(HsearchEntry)));
    if (newEntries == nullptr) return false;
    table.entries = newEntries;
    table.indexMask = newCount - 1;
    for (std::size_t index = 0; index < oldCount; ++index) {
        const HsearchEntry& entry = oldEntries[index];
        if (entry.key != nullptr) *LookupFree(Hash(table.offsetBasis, CopyKey(entry.key))) = entry;
    }
    free_nid_postfix(oldEntries);
    return true;
}

}

int APS5_VABI hcreate_nid_postfix(std::size_t) {
    return 1;
}

HsearchEntry* APS5_VABI hsearch_nid_postfix(HsearchEntry item, int action) {
    if (table.entries == nullptr && !Create()) return nullptr;
    if (item.key == nullptr) throw std::invalid_argument("hsearch: null key");
    const std::string key = CopyKey(item.key);
    const std::size_t hash = Hash(table.offsetBasis, key);
    HsearchEntry* entry = nullptr;
    for (std::size_t index = hash, step = 0;; index += ++step) {
        entry = &table.entries[index & table.indexMask];
        if (entry->key == nullptr) break;
        if (CopyKey(entry->key) == key) return entry;
    }
    if (action == FindAction) {
        errno = 3;
        return nullptr;
    }
    if (table.entriesUsed * 2 >= table.indexMask) {
        if (!Grow()) return nullptr;
        entry = LookupFree(hash);
    }
    *entry = item;
    ++table.entriesUsed;
    return entry;
}

void APS5_VABI hdestroy_nid_postfix(void) {
    if (table.entries == nullptr) return;
    free_nid_postfix(table.entries);
    table = HashTable{};
}
