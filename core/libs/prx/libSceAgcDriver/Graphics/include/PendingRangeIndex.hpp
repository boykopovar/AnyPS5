#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

template <class Item>
class PendingRangeIndex {
public:
    struct Entry {
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
        Item item{};
        std::size_t position = 0;
    };

    void Insert(std::uint64_t begin, std::uint64_t end, Item item, std::size_t position) {
        const auto at = std::upper_bound(entries.begin(), entries.end(), std::pair{begin, position}, [](const std::pair<std::uint64_t, std::size_t>& key, const Entry& entry) { return key.first != entry.begin ? key.first < entry.begin : key.second < entry.position; });
        const auto index = static_cast<std::size_t>(at - entries.begin());
        entries.insert(at, {begin, end, item, position});
        reach.insert(reach.begin() + static_cast<std::ptrdiff_t>(index), 0);
        recompute(index);
    }

    bool Erase(std::uint64_t begin, const Item& item) {
        auto at = std::lower_bound(entries.begin(), entries.end(), begin, [](const Entry& entry, std::uint64_t value) { return entry.begin < value; });
        for (; at != entries.end() && at->begin == begin; ++at) {
            if (!(at->item == item)) continue;
            const auto index = static_cast<std::size_t>(at - entries.begin());
            entries.erase(at);
            reach.erase(reach.begin() + static_cast<std::ptrdiff_t>(index));
            recompute(index);
            return true;
        }
        return false;
    }

    const std::vector<Entry>& Entries() const { return entries; }

    template <class Visit>
    bool AnyOverlapping(std::uint64_t address, std::size_t bytes, Visit&& visit) const {
        if (bytes == 0 || entries.empty()) return false;
        const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
        auto index = static_cast<std::size_t>(std::lower_bound(entries.begin(), entries.end(), end, [](const Entry& entry, std::uint64_t value) { return entry.begin < value; }) - entries.begin());
        while (index != 0) {
            --index;
            if (reach[index] <= address) break;
            const auto& entry = entries[index];
            if (entry.end > address && entry.begin < end && visit(entry.item)) return true;
        }
        return false;
    }

    template <class Match>
    const Entry* FirstByPosition(std::uint64_t address, std::size_t bytes, Match&& match) const {
        const Entry* first = nullptr;
        if (bytes == 0 || entries.empty()) return nullptr;
        const auto end = bytes > std::numeric_limits<std::uint64_t>::max() - address ? std::numeric_limits<std::uint64_t>::max() : address + bytes;
        auto index = static_cast<std::size_t>(std::lower_bound(entries.begin(), entries.end(), end, [](const Entry& entry, std::uint64_t value) { return entry.begin < value; }) - entries.begin());
        while (index != 0) {
            --index;
            if (reach[index] <= address) break;
            const auto& entry = entries[index];
            if (entry.end > address && entry.begin < end && match(entry.item) && (first == nullptr || entry.position < first->position)) first = &entry;
        }
        return first;
    }

private:
    void recompute(std::size_t from) {
        std::uint64_t furthest = from == 0 ? 0 : reach[from - 1];
        for (std::size_t index = from; index < entries.size(); ++index) {
            furthest = std::max(furthest, entries[index].end);
            reach[index] = furthest;
        }
    }

    std::vector<Entry> entries;
    std::vector<std::uint64_t> reach;
};

}
