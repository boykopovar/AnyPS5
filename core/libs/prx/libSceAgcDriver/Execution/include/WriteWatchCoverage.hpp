#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITEWATCHCOVERAGE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_WRITEWATCHCOVERAGE_HPP

#include <algorithm>
#include <array>
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
        const auto firstPage = (address - base) % BlockBytes / PageBytes;
        const auto lastPage = (address - base + bytes - 1) % BlockBytes / PageBytes;
        if (first == last) return (excluded[first] & pageMask(firstPage, lastPage + 1)) == 0;
        if ((excluded[first] & pageMask(firstPage, PagesPerBlock)) != 0 || (excluded[last] & pageMask(0, lastPage + 1)) != 0) return false;
        return std::none_of(excluded.begin() + first + 1, excluded.begin() + last, [](auto mask) { return mask != 0; });
    }

    bool ExcludedSpan(std::uint64_t address, std::size_t bytes, std::uint64_t* begin, std::uint64_t* end) const {
        std::size_t first = 0;
        std::size_t last = 0;
        if (!excludedBlocks(address, bytes, first, last)) return false;
        *begin = base + first * BlockBytes;
        *end = std::min<std::uint64_t>(base + (last + 1) * BlockBytes, base + size);
        return true;
    }
    bool Exclude(std::uint64_t address, std::size_t bytes, std::uint64_t generation = 1) {
        std::size_t first = 0;
        std::size_t last = 0;
        if (!excludedBlocks(address, bytes, first, last)) return false;
        const auto offset = std::max(address, base) - base;
        const auto firstPage = offset / PageBytes;
        const auto endPage = (std::min<std::uint64_t>(address + bytes - base, size) + PageBytes - 1) / PageBytes;
        bool changed = false;
        for (auto block = first; block <= last; ++block) {
            const auto from = block == first ? firstPage % PagesPerBlock : 0;
            const auto to = block == last ? (endPage - 1) % PagesPerBlock + 1 : PagesPerBlock;
            const auto mask = pageMask(from, to);
            changed = changed || (excluded[block] & mask) != mask;
            auto& stamps = excludedGenerations[block];
            for (auto page = from; page < to; ++page) stamps[page] = std::max(stamps[page], std::max<std::uint64_t>(generation, 1));
            excluded[block] |= mask;
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
        const auto firstPage = offset / PageBytes + (offset % PageBytes != 0);
        const auto stop = offset + bytes;
        const auto endPage = stop / PageBytes + (stop == size && stop % PageBytes != 0);
        if (firstPage >= endPage) return false;
        const auto first = firstPage / PagesPerBlock;
        const auto last = (endPage - 1) / PagesPerBlock;
        bool changed = false;
        for (auto block = first; block <= last; ++block) {
            const auto from = block == first ? firstPage % PagesPerBlock : 0;
            const auto to = block == last ? (endPage - 1) % PagesPerBlock + 1 : PagesPerBlock;
            if ((excluded[block] & pageMask(from, to)) == 0) continue;
            const auto& stamps = excludedGenerations.at(block);
            for (auto page = from; page < to; ++page) {
                const auto bit = static_cast<std::uint16_t>(1u << page);
                if ((excluded[block] & bit) == 0 || stamps[page] >= generation) continue;
                excluded[block] &= static_cast<std::uint16_t>(~bit);
                changed = true;
            }
            if (excluded[block] == 0) excludedGenerations.erase(block);
        }
        return changed;
    }

private:
    static std::uint16_t pageMask(std::size_t first, std::size_t end) {
        return static_cast<std::uint16_t>(((1u << end) - 1u) & ~((1u << first) - 1u));
    }
    bool excludedBlocks(std::uint64_t address, std::size_t bytes, std::size_t& first, std::size_t& last) const {
        if (bytes == 0 || size == 0) return false;
        if (address < base) {
            if (base - address >= bytes) return false;
            bytes -= base - address;
            address = base;
        }
        const auto offset = address - base;
        if (offset >= size) return false;
        bytes = static_cast<std::size_t>(std::min<std::uint64_t>(bytes, size - offset));
        first = offset / BlockBytes;
        last = (offset + bytes - 1) / BlockBytes;
        return true;
    }
    static constexpr std::size_t BlockBytes = 65536;
    static constexpr std::size_t PageBytes = 4096;
    static constexpr std::size_t PagesPerBlock = BlockBytes / PageBytes;
    std::uint64_t base = 0;
    std::size_t size = 0;
    std::vector<std::uint16_t> excluded;
    std::unordered_map<std::uint64_t, std::array<std::uint64_t, PagesPerBlock>> excludedGenerations;
};

}

#endif
