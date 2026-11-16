#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_KEPTUNITS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_KEPTUNITS_HPP

#include <algorithm>
#include <cstdint>
#include <map>
#include <span>

namespace AgcDriver::Graphics {

struct UnitLayout {
    std::uint64_t base = 0;
    std::uint64_t unitBytes = 0;
    std::uint64_t totalBytes = 0;

    std::uint64_t Begin(std::uint32_t unit) const {
        return base + static_cast<std::uint64_t>(unit) * unitBytes;
    }

    std::uint64_t End(std::uint32_t unit) const {
        return Begin(unit) + std::min<std::uint64_t>(unitBytes, totalBytes - static_cast<std::uint64_t>(unit) * unitBytes);
    }
};

inline void StampUnkeptUnits(const std::map<std::uint64_t, std::uint64_t>& kept, const UnitLayout& layout, std::span<const std::uint64_t> generations, std::span<std::uint64_t> stamps, std::uint32_t first, std::uint32_t count, std::uint64_t stamp) {
    if (count == 0) return;
    auto at = kept.lower_bound(layout.Begin(first));
    if (at == kept.end() || at->first >= layout.End(first + count - 1)) {
        std::fill_n(stamps.begin() + first, count, stamp);
        return;
    }
    for (std::uint32_t unit = first; unit < first + count; ++unit) {
        if (at != kept.end() && at->first < layout.Begin(unit)) at = kept.lower_bound(layout.Begin(unit));
        const auto end = layout.End(unit);
        bool held = false;
        for (; at != kept.end() && at->first < end; ++at) held = held || at->second == generations[unit];
        if (!held) stamps[unit] = stamp;
    }
}

}

#endif
