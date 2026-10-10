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
        require(coverage.Exclude(base + block + 4096, 1), "an imported page was not excluded");
        require(!coverage.Covers(base + block, block), "the imported block is still watched");
        require(!coverage.Covers(base + block - 1, 2), "a range crossing into the import is watched");
        require(coverage.Covers(base, block) && coverage.Covers(base + 2 * block, 2 * block + 1), "unrelated blocks lost write watching");
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
        require(!coverage.Restore(base + 1, block - 1) && !coverage.Covers(base, block), "a partial replacement restored an excluded block");
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
        std::cout << "write watch coverage passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
