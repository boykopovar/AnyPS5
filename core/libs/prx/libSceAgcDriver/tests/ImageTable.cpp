#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ImageTable.hpp"

void RunImageTableTests() {
    using namespace AgcDriver::Graphics;
    ImageTable table(2);
    Require(table.Capacity() == 2 && !table.Enabled(), "slot-only image table has invalid capacity or Vulkan state");

    const auto first = table.Acquire(2, 0x11, 1);
    const auto second = table.Acquire(5, 0x22, 1);
    Require(first == 0 && second == 1, "image table did not allocate its slots in stable order");
    Require(table.Acquire(2, 0x11, 2) == first, "an unchanged logical image moved while resident");
    const auto initialDirty = table.DirtyRuns();
    Require(initialDirty.size() == 1 && initialDirty[0].first == 0 && initialDirty[0].count == 2, "adjacent dirty image slots were not coalesced");
    table.ClearDirty(initialDirty);
    Require(table.DirtyRuns().empty(), "cleared image slots remained dirty");

    Require(!table.Acquire(7, 0x33, 3).has_value(), "image table reused a slot before its last frame completed");
    Require(table.GetSlot(*second).state == ImageTable::SlotState::Retiring && table.PhysicalSlot(5) == ImageTable::InvalidSlot, "an evicted image slot was not retired and unmapped");
    table.CompleteFrame(1);
    const auto replacement = table.Acquire(7, 0x33, 3);
    Require(replacement == second && table.GetSlot(*replacement).state == ImageTable::SlotState::Resident, "completed image slot was not reclaimed for a later allocation");

    ImageTable::StagedDescriptor staged{*replacement, table.GetSlot(*replacement).version, {}, std::make_shared<int>(1)};
    table.Staging(1).Add(std::move(staged));
    auto drained = table.Staging(1).Take();
    Require(drained.size() == 1 && table.Staging(1).Size() == 0 && drained[0].slot == *replacement && drained[0].owner != nullptr, "frame staging did not preserve and drain descriptor ownership");

    const auto staleWrites = table.DirtyRuns();
    table.CompleteFrame(3);
    Require(table.Acquire(7, 0x44, 4) == replacement, "a completed slot was not reused for changed image content");
    table.ClearDirty(staleWrites);
    const auto changedWrites = table.DirtyRuns();
    Require(changedWrites.size() == 1 && changedWrites[0].first == *replacement, "clearing an older dirty snapshot erased a newer slot version");

    ImageTable logicalCapacityTest(1);
    Require(logicalCapacityTest.Acquire(ImageTable::LogicalCapacity - 1, 0x55, 1) == 0, "the highest logical table index was not accepted");
}
