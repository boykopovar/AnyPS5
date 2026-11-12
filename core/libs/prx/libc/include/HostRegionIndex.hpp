#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_HOSTREGIONINDEX_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_HOSTREGIONINDEX_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace GuestArena {

class HostRegionIndex {
public:
    using Region = std::pair<std::uintptr_t, std::uintptr_t>;

    static bool Linear(const std::vector<Region>& regions, std::uintptr_t start, std::size_t bytes) {
        for (const auto& [regionStart, regionEnd] : regions) {
            if (regionStart < start + bytes && start < regionEnd) return true;
        }
        return false;
    }

    void Build(const std::vector<Region>& regions) {
        merged.clear();
        active = false;
        if (std::any_of(regions.begin(), regions.end(), [](const Region& region) { return region.first >= region.second; })) return;
        std::vector<Region> sorted(regions);
        std::sort(sorted.begin(), sorted.end());
        for (const auto& region : sorted) {
            if (!merged.empty() && region.first <= merged.back().second) merged.back().second = std::max(merged.back().second, region.second);
            else merged.push_back(region);
        }
        active = true;
    }

    bool Active() const { return active; }

    bool Overlaps(const std::vector<Region>& regions, std::uintptr_t start, std::size_t bytes) const {
        const auto stop = start + bytes;
        if (!active || bytes == 0 || stop < start) return Linear(regions, start, bytes);
        const auto next = std::upper_bound(merged.begin(), merged.end(), start, [](std::uintptr_t address, const Region& region) { return address < region.second; });
        return next != merged.end() && next->first < stop;
    }

private:
    std::vector<Region> merged;
    bool active = false;
};

}

#endif
