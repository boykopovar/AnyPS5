#include "prx/libSceAgcDriver/Execution/include/WriteWatchCoverage.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        using AgcDriver::GuestMemory::WriteWatchCoverage;
        constexpr std::uint64_t base = 0x200000000;
        constexpr std::size_t block = 65536;
        WriteWatchCoverage coverage;
        require(!coverage.Covers(base, 1), "an uninitialized arena is watched");
        coverage.Initialize(base, 4 * block + 1);
        require(coverage.Covers(base, 4 * block + 1), "the initialized arena is not watched");
        require(!coverage.Covers(base - 1, 1) && !coverage.Covers(base + 4 * block + 1, 1), "outside memory is watched");
        require(!coverage.Covers(base, 0) && !coverage.Covers(base, std::numeric_limits<std::size_t>::max()), "an empty or overflowing range is watched");
        std::uint64_t spanBegin = 0;
        std::uint64_t spanEnd = 0;
        require(coverage.ExcludedSpan(base + block + 4096, 4096, &spanBegin, &spanEnd) && spanBegin == base + block && spanEnd == base + 2 * block, "the span of an imported page is not its whole block");
        require(coverage.ExcludedSpan(base + 3 * block - 1, 2, &spanBegin, &spanEnd) && spanBegin == base + 2 * block && spanEnd == base + 4 * block, "the span of a boundary import is not both blocks");
        require(coverage.ExcludedSpan(base + 4 * block, std::numeric_limits<std::size_t>::max(), &spanBegin, &spanEnd) && spanBegin == base + 4 * block && spanEnd == base + 4 * block + 1, "the span of the last partial block is not clipped to the arena");
        require(coverage.ExcludedSpan(base - 1, 2, &spanBegin, &spanEnd) && spanBegin == base && spanEnd == base + block, "the span of an import crossing the arena start is not clipped");
        require(!coverage.ExcludedSpan(base - block, block, &spanBegin, &spanEnd) && !coverage.ExcludedSpan(base + 5 * block, block, &spanBegin, &spanEnd) && !coverage.ExcludedSpan(base, 0, &spanBegin, &spanEnd), "an outside or empty import has a span");
        require(coverage.Exclude(base + block + 4096, 1), "an imported page was not excluded");
        require(!coverage.Covers(base + block, block), "the imported block is still watched");
        require(coverage.Covers(base + block, 4096) && coverage.Covers(base + block + 2 * 4096, block - 2 * 4096), "an import unwatched pages of its block outside it");
        require(!coverage.Covers(base + block + 4095, 2) && !coverage.Covers(base + block + 2 * 4096 - 1, 2), "a range crossing into the import is watched");
        require(coverage.Covers(base, block) && coverage.Covers(base + 2 * block, 2 * block + 1), "unrelated blocks lost write watching");
        require(!coverage.Exclude(base + block + 4096, 1), "excluding an imported page again reports a change");
        require(coverage.Exclude(base + block, block) && !coverage.Covers(base + block, 4096), "excluding the rest of an imported block reports no change");
        require(!coverage.Exclude(base + block, block), "excluding an imported block again reports a change");
        require(coverage.Exclude(base + 3 * block - 1, 2), "an import across a block boundary was not excluded");
        require(!coverage.Covers(base + 2 * block, block) && !coverage.Covers(base + 3 * block, block), "a boundary import left one block watched");
        require(coverage.Covers(base, block) && coverage.Covers(base + 4 * block, 1), "a boundary import excluded adjacent blocks");
        require(!coverage.Exclude(base - block, block) && !coverage.Exclude(base + 5 * block, block), "an outside import changed coverage");
        require(coverage.Exclude(base + 4 * block, std::numeric_limits<std::size_t>::max()), "the last partial block was not excluded");
        require(!coverage.Covers(base + 4 * block, 1), "the last partial block is still watched");
        coverage.Initialize(base, 2 * block);
        require(coverage.Exclude(base - 1, 2) && !coverage.Covers(base, block), "an import crossing the arena start was not clipped");
        require(coverage.Covers(base + block, block), "clipping an import excluded the rest of the arena");
        coverage.Initialize(base, 4 * block + 1);
        coverage.Exclude(base, 4 * block + 1);
        require(coverage.Restore(base + 1, block - 1) && coverage.Covers(base + 4096, block - 4096), "a partial replacement did not restore its complete pages");
        require(!coverage.Covers(base, 4096) && !coverage.Covers(base, block), "a partial replacement restored an incompletely remapped page");
        require(coverage.Restore(base + block, block) && coverage.Covers(base + block, block), "fresh private backing stayed excluded");
        require(!coverage.Restore(base + block, block), "restoring an already watched block reports a change");
        require(!coverage.Covers(base, block) && !coverage.Covers(base + 2 * block, block), "restoring one block restored its neighbours");
        require(coverage.Restore(base + block - 1, 2 * block + 2), "a full interior replacement was not restored");
        require(!coverage.Covers(base, block) && coverage.Covers(base + block, 2 * block) && !coverage.Covers(base + 3 * block, block), "a replacement restored partially covered boundary blocks");
        require(!coverage.Restore(base - block, block) && !coverage.Restore(base + 5 * block, block), "an outside replacement changed coverage");
        require(coverage.Restore(base + 3 * block, std::numeric_limits<std::size_t>::max()) && coverage.Covers(base + 3 * block, block + 1), "an oversized replacement was not clipped to the arena");
        require(coverage.Restore(base - block, 2 * block) && coverage.Covers(base, block), "a replacement across the arena start was not clipped");
        coverage.Exclude(base + block + 4096, 1);
        require(!coverage.Covers(base + block, block), "a re-import did not exclude fresh backing again");
        coverage.Initialize(base, 2 * block);
        coverage.Exclude(base, block, 4);
        require(!coverage.Restore(base, block, 3) && !coverage.Restore(base, block, 4), "a stale mapping notification restored a newer import");
        require(coverage.Restore(base, block, 5) && coverage.Covers(base, block), "a newer private mapping did not restore an older exclusion");
        coverage.Exclude(base, block, 5);
        coverage.Exclude(base, block, 7);
        require(!coverage.Restore(base, block, 6) && !coverage.Covers(base, block), "a delayed mapping notification erased a repeated import");
        require(coverage.Restore(base, block, 8), "a fresh replacement did not restore the latest import exclusion");
        coverage.Initialize(base, 2 * block);
        coverage.Exclude(base, block, 10);
        require(coverage.Restore(base + 16384, 16384, 11), "a fresh partial-block private mapping stayed excluded");
        require(coverage.Covers(base + 16384, 16384), "the restored private pages are not watched");
        require(!coverage.Covers(base, 16384) && !coverage.Covers(base + 32768, 32768), "restoring private pages watched an imported neighbour");
        require(!coverage.Covers(base + 16383, 2) && !coverage.Covers(base + 32767, 2), "a boundary query included excluded pages");
        require(!coverage.Restore(base + 32768, 4096, 10), "a stale partial mapping restored an exclusion");
        coverage.Exclude(base + 4096, 1, 12);
        require(!coverage.Covers(base + 4096, 4096) && coverage.Covers(base + 16384, 16384), "a re-import of one page changed the coverage of other pages");
        require(!coverage.Restore(base + 4096, 4096, 11), "a delayed partial mapping restored a newer import");
        require(coverage.Restore(base + 4096, 4096, 13) && coverage.Covers(base + 4096, 4096), "a fresh mapping did not restore a re-imported page");
        constexpr std::size_t page = 4096;
        for (std::size_t first = 0; first < block / page; ++first) {
            for (std::size_t count = 1; count <= block / page - first; ++count) {
                coverage.Initialize(base, 3 * block);
                coverage.Exclude(base + block, block, 20);
                require(coverage.Restore(base + block + first * page, count * page, 21), "a complete-page replacement was not restored");
                for (std::size_t index = 0; index < 3 * block / page; ++index) {
                    const bool expected = index < block / page || index >= 2 * block / page || (index >= block / page + first && index < block / page + first + count);
                    require(coverage.Covers(base + index * page, page) == expected, "partial restoration changed an unrelated page");
                    if (index != 0) {
                        const bool previous = index - 1 < block / page || index - 1 >= 2 * block / page || (index - 1 >= block / page + first && index - 1 < block / page + first + count);
                        require(coverage.Covers(base + index * page - 1, 2) == (previous && expected), "a page boundary proof ignored an excluded page");
                    }
                }
                require(coverage.Covers(base + block, block) == (count == block / page), "partial page coverage proved a whole block");
                require(!coverage.Restore(base + block + first * page, count * page, 21), "restoring watched pages again reports a change");
            }
        }
        coverage.Initialize(base, 2 * block + 17);
        coverage.Exclude(base + 2 * block, 17, 30);
        require(!coverage.Restore(base + 2 * block, 16, 31), "an incomplete terminal page was restored");
        require(coverage.Restore(base + 2 * block, 17, 31) && coverage.Covers(base + 2 * block, 17), "a complete terminal arena page stayed excluded");
        coverage.Initialize(base, 2 * block);
        coverage.Exclude(base, block, 40);
        coverage.Exclude(base + block, block, 43);
        require(coverage.Restore(base + block - page, 2 * page, 42), "a newer boundary page was not restored");
        require(coverage.Covers(base + block - page, page) && !coverage.Covers(base + block, page), "a stale boundary mapping erased a newer neighbouring exclusion");
        constexpr std::size_t tail = 0xc4000;
        coverage.Initialize(base, 14 * block);
        coverage.Exclude(base, tail, 50);
        require(coverage.Restore(base, tail, 51), "an import ending inside a block did not restore its pages");
        require(coverage.Covers(base, tail) && coverage.Covers(base + 12 * block, tail - 12 * block), "an import ending inside a block is not watched");
        require(coverage.Covers(base, 13 * block) && coverage.Covers(base + tail, page), "a restored import ending inside a block left pages excluded");
        coverage.Exclude(base, tail, 52);
        require(!coverage.Covers(base + tail - page, page) && !coverage.Covers(base + tail - 1, 2), "a range ending inside an import is watched");
        require(coverage.Covers(base + tail, 13 * block - tail) && coverage.Covers(base + 13 * block, block), "an import ending inside a block unwatched the pages after it");
        coverage.Initialize(base, 14 * block);
        coverage.Exclude(base + page, tail, 60);
        require(coverage.Restore(base + page, tail, 61) && coverage.Covers(base + page, tail), "an import starting and ending inside blocks is not watched");
        coverage.Exclude(base + page, tail, 62);
        require(coverage.Covers(base, page) && coverage.Covers(base + page + tail, page), "an import starting and ending inside blocks unwatched pages outside it");
        require(!coverage.Covers(base, 2 * page) && !coverage.Covers(base + page + tail - 1, 2), "a range reaching into an import starting and ending inside blocks is watched");
        coverage.Initialize(base, 2 * block);
        coverage.Exclude(base + page, page, 70);
        coverage.Exclude(base + 5 * page, page, 75);
        require(coverage.Restore(base + page, page, 72) && coverage.Covers(base + page, page), "a newer import of another page in the block kept an older page excluded");
        require(!coverage.Covers(base + 5 * page, page), "restoring an older page restored a newer import in the same block");
        require(!coverage.Restore(base + 5 * page, page, 74) && !coverage.Covers(base + 5 * page, page), "a stale mapping restored a newer page in the same block");
        require(coverage.Restore(base + 5 * page, page, 76) && coverage.Covers(base, 2 * block), "a fresh mapping did not restore the newer page");
        coverage.Exclude(base + 3 * page, page, 80);
        for (std::size_t index = 0; index < 2 * block / page; ++index) {
            require(coverage.Covers(base + index * page, page) == (index != 3), "excluding one page changed another page of the arena");
        }
        require(coverage.ExcludedSpan(base + 3 * page, page, &spanBegin, &spanEnd) && spanBegin == base && spanEnd == base + block, "the span of an excluded page is not its whole block");
        std::cout << "write watch coverage passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
