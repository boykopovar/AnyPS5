#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace AgcDriver {

template <class Item>
class RangeIndex {
public:
    void Insert(Item item, std::uint64_t begin, std::uint64_t end) {
        const auto at = std::upper_bound(entries.begin(), entries.end(), begin, [](std::uint64_t value, const Entry& entry) { return value < entry.begin; });
        entries.insert(at, Entry{begin, end, item});
        stale = true;
    }

    std::size_t Remove(Item item, std::uint64_t begin, bool all = false) {
        auto first = std::lower_bound(entries.begin(), entries.end(), begin, [](const Entry& entry, std::uint64_t value) { return entry.begin < value; });
        std::size_t removed = 0;
        while (first != entries.end() && first->begin == begin) {
            if (first->item == item) {
                first = entries.erase(first);
                ++removed;
                if (!all) break;
            } else {
                ++first;
            }
        }
        if (removed != 0) stale = true;
        return removed;
    }

    template <class Visit>
    bool Overlapping(std::uint64_t address, std::uint64_t bytes, Visit&& visit) {
        if (entries.empty() || bytes == 0) return false;
        if (stale) rebuild();
        const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
        return walk(0, entries.size(), address, end, visit);
    }

    template <class Visit>
    bool StartingAt(std::uint64_t begin, Visit&& visit) const {
        auto it = std::lower_bound(entries.begin(), entries.end(), begin, [](const Entry& entry, std::uint64_t value) { return entry.begin < value; });
        for (; it != entries.end() && it->begin == begin; ++it) {
            if (visit(it->item)) return true;
        }
        return false;
    }

    std::size_t Size() const { return entries.size(); }

private:
    struct Entry {
        std::uint64_t begin;
        std::uint64_t end;
        Item item;
    };

    std::uint64_t build(std::size_t from, std::size_t to) {
        if (from >= to) return 0;
        const auto mid = from + (to - from) / 2;
        const auto left = build(from, mid);
        const auto right = build(mid + 1, to);
        reach[mid] = std::max({entries[mid].end, left, right});
        return reach[mid];
    }

    void rebuild() {
        reach.assign(entries.size(), 0);
        build(0, entries.size());
        stale = false;
    }

    template <class Visit>
    bool walk(std::size_t from, std::size_t to, std::uint64_t address, std::uint64_t end, Visit& visit) const {
        if (from >= to) return false;
        const auto mid = from + (to - from) / 2;
        if (reach[mid] <= address) return false;
        if (walk(from, mid, address, end, visit)) return true;
        if (entries[mid].begin >= end) return false;
        if (address < entries[mid].end && visit(entries[mid].item)) return true;
        return walk(mid + 1, to, address, end, visit);
    }

    std::vector<Entry> entries;
    std::vector<std::uint64_t> reach;
    bool stale = false;
};

}
