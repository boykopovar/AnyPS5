#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace AmmPool {

constexpr std::uint64_t PageBytes = 0x4000;

struct Page {
    std::uint64_t physical;
    std::int32_t type;
    std::int32_t protection;
    bool pooled;
};

struct Records {
    std::map<std::uint64_t, std::uint64_t> pool;
    std::map<std::uintptr_t, Page> pages;
    std::map<std::uint64_t, std::uint32_t> pooledUses;
};

inline void ReturnPages(Records& records, std::uint64_t offset, std::uint64_t bytes) {
    auto next = records.pool.lower_bound(offset);
    if (next != records.pool.end() && next->first == offset + bytes) {
        bytes += next->second;
        next = records.pool.erase(next);
    }
    if (next != records.pool.begin()) {
        const auto previous = std::prev(next);
        if (previous->first + previous->second == offset) {
            previous->second += bytes;
            return;
        }
    }
    records.pool.emplace(offset, bytes);
}

inline bool TryTakeRun(Records& records, std::uint64_t bytes, std::uint64_t& offset) {
    for (auto it = records.pool.begin(); it != records.pool.end(); ++it) {
        if (it->second < bytes) continue;
        offset = it->first;
        const auto remaining = it->second - bytes;
        records.pool.erase(it);
        if (remaining != 0) records.pool.emplace(offset + bytes, remaining);
        return true;
    }
    return false;
}

inline void AddPage(Records& records, std::uint64_t address, const Page& page) {
    if (!page.pooled) {
        records.pages[address] = page;
        return;
    }
    const auto [uses, inserted] = records.pooledUses.try_emplace(page.physical, 0);
    try {
        records.pages[address] = page;
    } catch (...) {
        if (inserted) records.pooledUses.erase(uses);
        throw;
    }
    ++uses->second;
}

template <class MapRun>
void MapScattered(Records& records, std::uint64_t address, std::uint64_t bytes, std::int32_t type, std::int32_t protection, MapRun&& mapRun) {
    std::uint64_t free = 0;
    for (const auto& [offset, size] : records.pool) free += size;
    if (free < bytes) throw std::runtime_error("AMM: no free run of " + std::to_string(bytes) + " bytes in the direct memory given to the mapper");
    std::vector<Page> pages;
    pages.reserve(bytes / PageBytes);
    std::vector<std::map<std::uint64_t, std::uint64_t>::iterator> runs;
    runs.reserve(records.pool.size());
    std::uint64_t remainderOffset = 0;
    std::uint64_t remainderBytes = 0;
    std::uint64_t remaining = bytes;
    for (auto it = records.pool.begin(); remaining != 0; ++it) {
        const auto take = std::min(it->second, remaining);
        for (std::uint64_t page = 0; page < take; page += PageBytes) pages.push_back({it->first + page, type, protection, true});
        runs.push_back(it);
        if (take < it->second) {
            remainderOffset = it->first + take;
            remainderBytes = it->second - take;
        }
        remaining -= take;
    }
    std::map<std::uintptr_t, Page> stagedPages;
    std::map<std::uint64_t, std::uint32_t> stagedUses;
    for (std::size_t index = 0; index < pages.size(); ++index) {
        stagedPages.emplace(address + index * PageBytes, pages[index]);
        stagedUses.emplace(pages[index].physical, 1);
    }
    for (std::size_t first = 0; first < pages.size();) {
        std::size_t last = first + 1;
        while (last < pages.size() && pages[last].physical == pages[last - 1].physical + PageBytes) ++last;
        mapRun(address + first * PageBytes, pages[first].physical, (last - first) * PageBytes);
        first = last;
    }
    if (remainderBytes != 0) records.pool.emplace(remainderOffset, remainderBytes);
    for (const auto run : runs) records.pool.erase(run);
    records.pages.merge(stagedPages);
    records.pooledUses.merge(stagedUses);
}

}
