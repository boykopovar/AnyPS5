#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITEWATCHCOVERAGE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITEWATCHCOVERAGE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <limits>
#include <unordered_map>

namespace AgcDriver::GuestMemory {

class WriteWatchCoverage {
public:
    void Initialize(std::uint64_t address, std::size_t bytes) {
        base = address;
        size = bytes;
        excluded.assign(bytes / BlockBytes + (bytes % BlockBytes != 0), 0);
        excludedGenerations.clear();
    }

    bool Covers(std::uint64_t address, std::size_t bytes) const {
        if (bytes == 0 || bytes > size || address < base || address - base > size - bytes) return false;
        const auto first = (address - base) / BlockBytes;
        const auto last = (address - base + bytes - 1) / BlockBytes;
        return std::none_of(excluded.begin() + first, excluded.begin() + last + 1, [](auto generation) { return generation != 0; });
    }

    bool Exclude(std::uint64_t address, std::size_t bytes, std::uint64_t generation = 1) {
        if (bytes == 0 || size == 0) return false;
        if (address < base) {
            if (base - address >= bytes) return false;
            bytes -= base - address;
            address = base;
        }
        const auto offset = address - base;
        if (offset >= size) return false;
        bytes = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, size - offset));
        const auto first = offset / BlockBytes;
        const auto last = (offset + bytes - 1) / BlockBytes;
        bool changed = false;
        for (auto block = first; block <= last; ++block) {
            changed = changed || excluded[block] == 0;
            auto& stamp = excludedGenerations[block];
            stamp = std::max(stamp, std::max<std::uint64_t>(generation, 1));
            excluded[block] = 1;
        }
        return changed;
    }

    bool Restore(std::uint64_t address, std::size_t bytes, std::uint64_t generation = std::numeric_limits<std::uint64_t>::max()) {
        if (bytes == 0 || size == 0) return false;
        if (address < base) {
            if (base - address >= bytes) return false;
            bytes -= base - address;
            address = base;
        }
        const auto offset = address - base;
        if (offset >= size) return false;
        bytes = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, size - offset));
        const auto first = offset / BlockBytes + (offset % BlockBytes != 0);
        const auto stop = offset + bytes;
        const auto end = stop / BlockBytes + (stop == size && stop % BlockBytes != 0);
        bool changed = false;
        for (auto block = first; block < end; ++block) {
            if (excluded[block] != 0 && excludedGenerations.at(block) < generation) {
                changed = true;
                excluded[block] = 0;
                excludedGenerations.erase(block);
            }
        }
        return changed;
    }

private:
    static constexpr std::size_t BlockBytes = 65536;
    std::uint64_t base = 0;
    std::size_t size = 0;
    std::vector<std::uint8_t> excluded;
    std::unordered_map<std::uint64_t, std::uint64_t> excludedGenerations;
};

}

#endif
