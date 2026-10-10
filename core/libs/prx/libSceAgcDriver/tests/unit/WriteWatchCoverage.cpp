#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/WriteWatchCoverage.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using AgcDriver::GuestMemory::WriteWatchCoverage;
using Testing::Case;
using Testing::Require;

constexpr std::uint64_t Base = 0x200000000;
constexpr std::size_t Block = 65536;
constexpr std::size_t Everything = std::numeric_limits<std::size_t>::max();

WriteWatchCoverage Arena(std::size_t bytes) {
    WriteWatchCoverage coverage;
    coverage.Initialize(Base, bytes);
    return coverage;
}

WriteWatchCoverage FullyExcludedArena() {
    auto coverage = Arena(4 * Block + 1);
    coverage.Exclude(Base, 4 * Block + 1);
    return coverage;
}

const Case uninitialized{"Covers_UninitializedArena_IsFalse", [] {
    const WriteWatchCoverage coverage;
    Require(!coverage.Covers(Base, 1), "an uninitialized arena is watched");
}};

const Case initialized{"Initialize_Arena_CoversExactlyTheArena", [] {
    const auto coverage = Arena(4 * Block + 1);
    Require(coverage.Covers(Base, 4 * Block + 1), "the initialized arena is not watched");
    Require(!coverage.Covers(Base - 1, 1) && !coverage.Covers(Base + 4 * Block + 1, 1), "outside memory is watched");
    Require(!coverage.Covers(Base, 0) && !coverage.Covers(Base, Everything), "an empty or overflowing range is watched");
}};

const Case importedPage{"Exclude_ImportedPage_UnwatchesOnlyItsBlock", [] {
    auto coverage = Arena(4 * Block + 1);
    Require(coverage.Exclude(Base + Block + 4096, 1), "an imported page was not excluded");
    Require(!coverage.Covers(Base + Block, Block), "the imported block is still watched");
    Require(!coverage.Covers(Base + Block - 1, 2), "a range crossing into the import is watched");
    Require(coverage.Covers(Base, Block) && coverage.Covers(Base + 2 * Block, 2 * Block + 1), "unrelated blocks lost write watching");
    Require(!coverage.Exclude(Base + Block, Block), "excluding an imported block again reports a change");
}};

const Case boundaryImport{"Exclude_ImportAcrossBlockBoundary_UnwatchesBothBlocks", [] {
    auto coverage = Arena(4 * Block + 1);
    Require(coverage.Exclude(Base + Block + 4096, 1), "an imported page was not excluded");
    Require(coverage.Exclude(Base + 3 * Block - 1, 2), "an import across a block boundary was not excluded");
    Require(!coverage.Covers(Base + 2 * Block, Block) && !coverage.Covers(Base + 3 * Block, Block), "a boundary import left one block watched");
    Require(coverage.Covers(Base, Block) && coverage.Covers(Base + 4 * Block, 1), "a boundary import excluded adjacent blocks");
}};

const Case outsideImport{"Exclude_OutsideArena_LeavesCoverageUnchanged", [] {
    auto coverage = Arena(4 * Block + 1);
    Require(!coverage.Exclude(Base - Block, Block) && !coverage.Exclude(Base + 5 * Block, Block), "an outside import changed coverage");
    Require(coverage.Covers(Base, 4 * Block + 1), "an outside import unwatched the arena");
}};

const Case lastPartialBlock{"Exclude_OversizedRangeFromLastPartialBlock_UnwatchesIt", [] {
    auto coverage = Arena(4 * Block + 1);
    Require(coverage.Exclude(Base + 4 * Block, Everything), "the last partial block was not excluded");
    Require(!coverage.Covers(Base + 4 * Block, 1), "the last partial block is still watched");
}};

const Case arenaStartImport{"Exclude_ImportCrossingArenaStart_IsClippedToTheArena", [] {
    auto coverage = Arena(2 * Block);
    Require(coverage.Exclude(Base - 1, 2) && !coverage.Covers(Base, Block), "an import crossing the arena start was not clipped");
    Require(coverage.Covers(Base + Block, Block), "clipping an import excluded the rest of the arena");
}};

const Case partialReplacement{"Restore_PartialBlockReplacement_KeepsTheBlockExcluded", [] {
    auto coverage = FullyExcludedArena();
    Require(!coverage.Restore(Base + 1, Block - 1) && !coverage.Covers(Base, Block), "a partial replacement restored an excluded block");
}};

const Case fullBlockReplacement{"Restore_FullBlockReplacement_RewatchesOnlyThatBlock", [] {
    auto coverage = FullyExcludedArena();
    Require(coverage.Restore(Base + Block, Block) && coverage.Covers(Base + Block, Block), "fresh private backing stayed excluded");
    Require(!coverage.Restore(Base + Block, Block), "restoring an already watched block reports a change");
    Require(!coverage.Covers(Base, Block) && !coverage.Covers(Base + 2 * Block, Block), "restoring one block restored its neighbours");
}};

const Case interiorReplacement{"Restore_InteriorReplacement_RewatchesOnlyFullyReplacedBlocks", [] {
    auto coverage = FullyExcludedArena();
    Require(coverage.Restore(Base + Block - 1, 2 * Block + 2), "a full interior replacement was not restored");
    Require(!coverage.Covers(Base, Block) && coverage.Covers(Base + Block, 2 * Block) && !coverage.Covers(Base + 3 * Block, Block),
            "a replacement restored partially covered boundary blocks");
}};

const Case outsideReplacement{"Restore_OutsideArena_LeavesCoverageUnchanged", [] {
    auto coverage = FullyExcludedArena();
    Require(!coverage.Restore(Base - Block, Block) && !coverage.Restore(Base + 5 * Block, Block), "an outside replacement changed coverage");
}};

const Case clippedReplacement{"Restore_ReplacementPastArenaEdges_IsClippedToTheArena", [] {
    auto coverage = FullyExcludedArena();
    Require(coverage.Restore(Base + 3 * Block, Everything) && coverage.Covers(Base + 3 * Block, Block + 1), "an oversized replacement was not clipped to the arena");
    Require(coverage.Restore(Base - Block, 2 * Block) && coverage.Covers(Base, Block), "a replacement across the arena start was not clipped");
}};

const Case reimport{"Exclude_ReimportAfterRestore_UnwatchesFreshBackingAgain", [] {
    auto coverage = FullyExcludedArena();
    Require(coverage.Restore(Base + Block, Block), "fresh private backing stayed excluded");
    coverage.Exclude(Base + Block + 4096, 1);
    Require(!coverage.Covers(Base + Block, Block), "a re-import did not exclude fresh backing again");
}};

const Case staleNotification{"Restore_StaleMappingGeneration_KeepsNewerImport", [] {
    auto coverage = Arena(2 * Block);
    coverage.Exclude(Base, Block, 4);
    Require(!coverage.Restore(Base, Block, 3) && !coverage.Restore(Base, Block, 4), "a stale mapping notification restored a newer import");
    Require(coverage.Restore(Base, Block, 5) && coverage.Covers(Base, Block), "a newer private mapping did not restore an older exclusion");
}};

const Case delayedNotification{"Restore_DelayedNotificationAfterRepeatedImport_KeepsLatestExclusion", [] {
    auto coverage = Arena(2 * Block);
    coverage.Exclude(Base, Block, 5);
    coverage.Exclude(Base, Block, 7);
    Require(!coverage.Restore(Base, Block, 6) && !coverage.Covers(Base, Block), "a delayed mapping notification erased a repeated import");
    Require(coverage.Restore(Base, Block, 8), "a fresh replacement did not restore the latest import exclusion");
}};

} // namespace
