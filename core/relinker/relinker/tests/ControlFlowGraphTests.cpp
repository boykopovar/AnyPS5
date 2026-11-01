#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/RelativeJumpTables.hpp>
#include <codegen/CodegenException.hpp>
#include <cstring>
#include <iostream>
#include <initializer_list>
#include <map>
#include <stdexcept>
#include <string>
#include <limits>
#include <chrono>
#include <cstdlib>
#include <new>

namespace {

bool failAllocations = false;

}

void* operator new(std::size_t size) {
    if (failAllocations) throw std::bad_alloc();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

namespace {

using namespace Relinker::UnusedNidFilter;
using Relinker::VirtualAddress;
using Relinker::RelinkerException;
using Bytes = std::vector<std::uint8_t>;
constexpr VirtualAddress TextAddress = 0xec00001000;

class Pointers : public IRelativeRelocationIndex {
public:
    std::map<VirtualAddress, VirtualAddress> Slots;

    std::optional<VirtualAddress> TargetOfSlot(VirtualAddress slot) const override {
        const auto position = Slots.find(slot);
        if (position == Slots.end()) return std::nullopt;
        return position->second;
    }

    std::vector<VirtualAddress> Targets() const override {
        std::vector<VirtualAddress> targets;
        for (const auto& [slot, target] : Slots) targets.push_back(target);
        return targets;
    }
};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void RequireOffsets(const IControlFlowGraph& graph, std::initializer_list<std::size_t> offsets) {
    std::unordered_set<VirtualAddress> expected;
    for (const auto offset : offsets) expected.insert(TextAddress + offset);
    Require(graph.ReachableVaddrs() == expected, "Unexpected reachable instruction set");
    for (const auto address : expected) Require(graph.IsReachable(address), "Missing reachable instruction");
    Require(!graph.IsReachable(TextAddress - 1), "Address before code is reachable");
}

void SetDisplacement(Bytes& text, std::size_t offset, std::size_t next, VirtualAddress target) {
    const auto displacement = static_cast<std::int32_t>(target - (TextAddress + next));
    std::memcpy(text.data() + offset, &displacement, sizeof(displacement));
}

void CheckFallthrough() {
    const Pointers pointers;
    const Bytes text{0x90, 0x48, 0x89, 0xc0, 0xc3, 0x90};
    const auto graph = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*graph, {0, 1, 4});
    const auto end = BuildControlFlowGraph(Bytes{0x90, 0x90}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*end, {0, 1});
    const auto empty = BuildControlFlowGraph(Bytes{}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*empty, {});
    const auto outside = BuildControlFlowGraph(text, TextAddress, TextAddress - 1, {TextAddress + text.size()}, pointers);
    RequireOffsets(*outside, {});
}

void CheckBranches() {
    const Pointers pointers;
    const Bytes text{0x75, 0x03, 0x90, 0xeb, 0x03, 0x90, 0xeb, 0xfa, 0xc3, 0x90};
    const auto graph = BuildControlFlowGraph(text, TextAddress, TextAddress, {TextAddress, TextAddress + 2}, pointers);
    RequireOffsets(*graph, {0, 2, 3, 5, 6, 8});
    const auto self = BuildControlFlowGraph(Bytes{0xeb, 0xfe, 0x90}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*self, {0});
    const auto outside = BuildControlFlowGraph(Bytes{0xeb, 0x7f, 0x90}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*outside, {0});
    const auto trap = BuildControlFlowGraph(Bytes{0x0f, 0x0b, 0x90}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*trap, {0});
}

void CheckCalls() {
    const Pointers pointers;
    const Bytes text{0xe8, 0x03, 0, 0, 0, 0x90, 0xc3, 0x90, 0xe8, 0xf3, 0xff, 0xff, 0xff, 0xc3};
    const auto graph = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*graph, {0, 5, 6, 8, 13});
    const auto indirect = BuildControlFlowGraph(Bytes{0xff, 0xd0, 0x90, 0xff, 0xe0, 0xc3}, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*indirect, {0, 2, 3});
}

void CheckCallsBeforeZeroPadding() {
    const Pointers pointers;
    Bytes padded(0x21, 0);
    padded[0] = 0xe8;
    SetDisplacement(padded, 1, 5, TextAddress + 0x20);
    const Bytes next{0x75, 0x64, 0x55, 0x48, 0x89, 0xe5, 0xc3};
    std::copy(next.begin(), next.end(), padded.begin() + 16);
    padded[0x20] = 0xc3;
    RequireOffsets(*BuildControlFlowGraph(padded, TextAddress, TextAddress, {}, pointers), {0, 0x20});
    RequireOffsets(*BuildControlFlowGraph(padded, TextAddress, TextAddress, {TextAddress + 16}, pointers), {0, 16, 18, 19, 22, 0x20});

    Bytes indirect(0x11, 0);
    indirect[0] = 0xff;
    indirect[1] = 0xd0;
    indirect[16] = 0xc3;
    RequireOffsets(*BuildControlFlowGraph(indirect, TextAddress, TextAddress, {}, pointers), {0});
}

void CheckCallsThatStillFallThrough() {
    const Pointers pointers;
    Bytes boundary(0x13, 0x90);
    boundary[11] = 0xe8;
    SetDisplacement(boundary, 12, 16, TextAddress + 0x12);
    boundary[16] = 0;
    boundary[17] = 0;
    boundary[18] = 0xc3;
    RequireOffsets(*BuildControlFlowGraph(boundary, TextAddress, TextAddress, {}, pointers), {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 16, 18});

    Bytes partial(0x11, 0x90);
    partial[0] = 0xe8;
    SetDisplacement(partial, 1, 5, TextAddress + 0x10);
    partial[5] = 0;
    partial[6] = 0;
    partial[7] = 0xc3;
    partial[0x10] = 0xc3;
    RequireOffsets(*BuildControlFlowGraph(partial, TextAddress, TextAddress, {}, pointers), {0, 5, 7, 0x10});

    Bytes truncated{0xe8, 0, 0, 0, 0, 0, 0xc3};
    SetDisplacement(truncated, 1, 5, TextAddress + 6);
    RequireOffsets(*BuildControlFlowGraph(truncated, TextAddress, TextAddress, {}, pointers), {0, 5, 6});
}

void CheckRelocatedTransfers() {
    constexpr auto slot = TextAddress + 0x1000;
    Bytes text{0xff, 0x15, 0, 0, 0, 0, 0x90, 0xc3, 0xff, 0x25, 0, 0, 0, 0, 0x90, 0x90, 0xc3};
    SetDisplacement(text, 2, 6, slot);
    SetDisplacement(text, 10, 14, slot + 8);
    Pointers pointers;
    pointers.Slots = {{slot, TextAddress + 8}, {slot + 8, TextAddress + 16}};
    const auto graph = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*graph, {0, 6, 7, 8, 16});
    const Pointers missing;
    const auto unresolved = BuildControlFlowGraph(text, TextAddress, TextAddress, {TextAddress + 8}, missing);
    RequireOffsets(*unresolved, {0, 6, 7, 8});
}

void CheckSegmentRelativeTransfers() {
    for (const auto segment : {0x00, 0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65}) {
        for (const bool address32 : {false, true}) {
            for (const bool call : {false, true}) {
                Bytes text;
                if (segment != 0) text.push_back(static_cast<std::uint8_t>(segment));
                if (address32) text.push_back(0x67);
                text.insert(text.end(), {0xff, static_cast<std::uint8_t>(call ? 0x15 : 0x25), 0, 0, 0, 0});
                const auto next = text.size();
                text.push_back(0xc3);
                text.resize(32, 0x90);
                text.push_back(0xc3);
                const auto slot = TextAddress + 0x1000;
                SetDisplacement(text, next - 4, next, slot);
                Pointers pointers;
                pointers.Slots = {{address32 ? static_cast<std::uint32_t>(slot) : slot, TextAddress + 32}};
                const auto graph = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
                std::unordered_set<VirtualAddress> expected{TextAddress};
                if (call) expected.insert(TextAddress + next);
                if (segment != 0x64 && segment != 0x65) expected.insert(TextAddress + 32);
                Require(graph->ReachableVaddrs() == expected, "Segment-relative transfer followed a flat relocation slot");
                if (segment == 0x64 || segment == 0x65) {
                    text[32] = 0x0f;
                    const auto unmapped = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
                    Require(unmapped->ReachableVaddrs() == expected, "Segment-relative transfer decoded unrelated bytes");
                    const auto explicitRoot = BuildControlFlowGraph(Bytes(text.begin(), text.begin() + 32), TextAddress,
                        TextAddress, {TextAddress + 31}, pointers);
                    expected.insert(TextAddress + 31);
                    Require(explicitRoot->ReachableVaddrs() == expected, "Segment-relative transfer discarded an explicit root");
                }
            }
        }
    }
}

void CheckAddressSizeOverride() {
    constexpr auto slot = TextAddress + 0x1000;
    Bytes text{0x67, 0xff, 0x15, 0, 0, 0, 0, 0x90, 0xc3, 0x67, 0xff, 0x25, 0, 0, 0, 0, 0x90, 0x90, 0xc3};
    SetDisplacement(text, 3, 7, slot);
    SetDisplacement(text, 12, 16, slot + 8);
    Pointers pointers;
    pointers.Slots = {{static_cast<std::uint32_t>(slot), TextAddress + 9}, {static_cast<std::uint32_t>(slot + 8), TextAddress + 18}};
    RequireOffsets(*BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers), {0, 7, 8, 9, 18});
    RequireOffsets(*BuildControlFlowGraph(text, TextAddress, TextAddress + 9, {}, pointers), {9, 18});

    Bytes lea(17, 0x90);
    const Bytes opcode{0x67, 0x48, 0x8d, 0x05};
    std::copy(opcode.begin(), opcode.end(), lea.begin());
    SetDisplacement(lea, 4, 8, TextAddress + 16);
    lea[8] = lea[16] = 0xc3;
    const Pointers missing;
    RequireOffsets(*BuildControlFlowGraph(lea, TextAddress, TextAddress, {}, missing, true), {0, 8});
    RequireOffsets(*BuildAddressTakenControlFlowGraph(lea, TextAddress, TextAddress, {}, missing, {}), {0, 8});
}

void CheckCodeAddresses() {
    Bytes text(17, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + 16);
    text[7] = text[16] = 0xc3;
    const Pointers pointers;
    const auto ordinary = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*ordinary, {0, 7});
    const auto addresses = BuildControlFlowGraph(text, TextAddress, TextAddress, {}, pointers, true);
    RequireOffsets(*addresses, {0, 7, 16});
    for (const std::uint8_t segment : {0x64, 0x65}) {
        auto prefixed = text;
        prefixed.insert(prefixed.begin(), segment);
        const auto graph = BuildControlFlowGraph(prefixed, TextAddress, TextAddress, {}, pointers, true);
        RequireOffsets(*graph, {0, 8, 17});
    }
}

void CheckSpeculativeRoots() {
    constexpr auto table = TextAddress + 0x1000;
    Bytes text(32, 0xc3);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, table);
    text[16] = 0x90;
    text[31] = 0x0f;
    Bytes entries(12);
    const std::int32_t targets[]{static_cast<std::int32_t>(TextAddress + 16 - table),
                                 static_cast<std::int32_t>(TextAddress + 31 - table), 0};
    std::memcpy(entries.data(), targets, sizeof(targets));
    Pointers pointers;
    pointers.Slots = {{table + 16, TextAddress + 24}, {table + 24, TextAddress + 31}};
    const std::vector<StrictDataRegion> data{{table, entries}};
    const auto graph = BuildAddressTakenControlFlowGraph(text, TextAddress, TextAddress, {}, pointers, data);
    RequireOffsets(*graph, {0, 7, 16, 17, 24});
    bool threw = false;
    try {
        BuildAddressTakenControlFlowGraph(text, TextAddress, TextAddress, {TextAddress + 31}, pointers, data);
    } catch (const Codegen::CodegenException&) {
        threw = true;
    }
    Require(threw, "Malformed mandatory root was treated as speculative");
}

void CheckTrailingZeroRoots() {
    const Pointers pointers;
    const Bytes lone{0x00};
    bool threw = false;
    try {
        BuildControlFlowGraph(lone, TextAddress, TextAddress, {}, pointers);
    } catch (const Codegen::CodegenException&) {
        threw = true;
    }
    Require(threw, "A lone zero instruction at the entry was not decoded");

    const Bytes trailing{0xc3, 0x00};
    threw = false;
    try {
        BuildControlFlowGraph(trailing, TextAddress, TextAddress, {TextAddress + 1}, pointers);
    } catch (const Codegen::CodegenException&) {
        threw = true;
    }
    Require(threw, "A zero instruction at an extra entry was not decoded");

    const Bytes jump{0xeb, 0x00, 0x00};
    threw = false;
    try {
        BuildControlFlowGraph(jump, TextAddress, TextAddress, {}, pointers);
    } catch (const Codegen::CodegenException&) {
        threw = true;
    }
    Require(threw, "A branch into a truncated zero instruction was not decoded");

    const Bytes pair{0x00, 0x00};
    const auto complete = BuildControlFlowGraph(pair, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*complete, {0});

    const Bytes immediate{0xb8, 0x00, 0x00, 0x00, 0x00};
    const auto zeroImmediate = BuildControlFlowGraph(immediate, TextAddress, TextAddress, {}, pointers);
    RequireOffsets(*zeroImmediate, {0});
}

void CheckBorrowedReachability() {
    const Pointers pointers;
    const Bytes text{0x90, 0x90, 0xc3, 0x90, 0xc3};
    std::unordered_set<VirtualAddress> reachable{TextAddress + 1};
    const auto first = BuildControlFlowGraph(std::span<const std::uint8_t>(text), TextAddress, TextAddress, {}, pointers, false, reachable);
    RequireOffsets(*first, {0, 1});
    Require(&first->ReachableVaddrs() == &reachable, "Borrowed graph copied the result set");
    const auto second = BuildControlFlowGraph(std::span<const std::uint8_t>(text), TextAddress, TextAddress + 3,
                                             {TextAddress + 3, TextAddress + 4}, pointers, false, reachable);
    RequireOffsets(*second, {0, 1, 3, 4});
    RequireOffsets(*first, {0, 1, 3, 4});
}

void CheckReachedMap() {
    const Pointers pointers;
    const Bytes text{0x90, 0x90, 0xc3, 0x90, 0xc3};
    const std::span<const std::uint8_t> code(text);
    std::vector<std::uint8_t> reached(text.size(), 0);
    reached[1] = 1;
    Require(ExtendControlFlowGraph(code, TextAddress, TextAddress, {}, pointers, false, reached) == std::vector<VirtualAddress>{TextAddress},
            "The walk did not stop at an instruction the map already marks");
    Require(ExtendControlFlowGraph(code, TextAddress, TextAddress + 3, {TextAddress + 4, TextAddress + 3}, pointers, false, reached) == std::vector<VirtualAddress>{TextAddress + 3, TextAddress + 4},
            "Newly reached instructions were not returned once each in address order");
    Require(reached == std::vector<std::uint8_t>{1, 1, 0, 1, 1}, "The map does not mark exactly the reached instructions");
    Require(ExtendControlFlowGraph(code, TextAddress, TextAddress, {TextAddress + 3}, pointers, false, reached).empty(), "A walk of reached code returned instructions");
    std::vector<std::uint8_t> shortMap(text.size() - 1, 0);
    bool threw = false;
    try {
        ExtendControlFlowGraph(code, TextAddress, TextAddress, {}, pointers, false, shortMap);
    } catch (const RelinkerException&) {
        threw = true;
    }
    Require(threw, "A map shorter than the text was accepted");
}

void CheckAllocationFailureDuringWalk() {
    const Pointers pointers;
    Bytes text(8192, 0x90);
    text.push_back(0xc3);
    std::unordered_set<VirtualAddress> reachable;
    bool threw = false;
    try {
        BuildControlFlowGraph(std::span<const std::uint8_t>(text), TextAddress, TextAddress, {}, pointers, false, reachable, [](std::uint64_t count) {
            if (count >= 4096) failAllocations = true;
        });
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    failAllocations = false;
    Require(threw, "An allocation failure during the walk did not reach the caller as std::bad_alloc");
}

void CheckAddressOverflow() {
    const Pointers pointers;
    const Bytes text{0x90, 0xc3};
    bool threw = false;
    try {
        BuildControlFlowGraph(text, std::numeric_limits<VirtualAddress>::max(), std::numeric_limits<VirtualAddress>::max(), {}, pointers);
    } catch (const RelinkerException&) {
        threw = true;
    }
    Require(threw, "Text section address space overflow was not rejected");

    threw = false;
    try {
        Relinker::FindRelativeJumpTables(text, std::numeric_limits<VirtualAddress>::max(), {});
    } catch (const RelinkerException&) {
        threw = true;
    }
    Require(threw, "Relative jump table text section address space overflow was not rejected");

    threw = false;
    try {
        std::size_t tableEnd = 0;
        Relinker::FindTrailingRelativeJumpTable(text, std::numeric_limits<VirtualAddress>::max(), &tableEnd);
    } catch (const RelinkerException&) {
        threw = true;
    }
    Require(threw, "Trailing relative jump table address space overflow was not rejected");

    const auto lastAddress = std::numeric_limits<VirtualAddress>::max() - text.size();
    Require(Relinker::FindRelativeJumpTables(text, lastAddress, {}).empty(), "Relative jump tables at the end of the address space were rejected");
    Require(Relinker::FindTrailingRelativeJumpTable(text, lastAddress) == text.size(), "Trailing relative jump table at the end of the address space was rejected");
}

void CheckHighVirtualAddress() {
    constexpr VirtualAddress highVaddr = 0xf00000001000;
    const Pointers pointers;
    Bytes text{0xeb, 0x02, 0x90, 0x90, 0xc3};
    const auto graph = BuildControlFlowGraph(text, highVaddr, highVaddr, {}, pointers);
    std::unordered_set<VirtualAddress> expected{highVaddr, highVaddr + 4};
    Require(graph->ReachableVaddrs() == expected, "Unexpected reachable instruction set at high virtual address");
    Require(graph->IsReachable(highVaddr), "Missing high entry address");
    Require(graph->IsReachable(highVaddr + 4), "Missing high branch target address");
}

void CheckRelativeJumpTables() {
    constexpr std::size_t tableOffset = 16;
    Bytes text(32, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + tableOffset);
    text[7] = 0xff;
    text[8] = 0xe0;
    const std::int32_t targets[]{-16, -9};
    std::memcpy(text.data() + tableOffset, targets, sizeof(targets));
    Require(Relinker::FindTrailingRelativeJumpTable(text) == tableOffset, "Relative jump table was not found");

    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functions{{TextAddress, TextAddress + 9}, {TextAddress + text.size(), TextAddress + text.size() + 8}};
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    Require(tables.size() == 1, "Expected one relative jump table");
    Require(tables[0].Function == TextAddress && tables[0].Begin == TextAddress + tableOffset && tables[0].End == TextAddress + text.size(), "Relative jump table range was incorrect");

    Bytes oneEntry = text;
    const std::int32_t singleTarget[]{-16};
    std::memcpy(oneEntry.data() + tableOffset, singleTarget, sizeof(singleTarget));
    const std::int32_t invalidSecondTarget[]{0x100};
    std::memcpy(oneEntry.data() + tableOffset + 4, invalidSecondTarget, sizeof(invalidSecondTarget));
    Require(Relinker::FindTrailingRelativeJumpTable(oneEntry) == oneEntry.size(), "Single-entry table was accepted");

    Bytes invalidTarget = text;
    const std::int32_t invalidTargets[]{0x100, 0x104};
    std::memcpy(invalidTarget.data() + tableOffset, invalidTargets, sizeof(invalidTargets));
    Require(Relinker::FindTrailingRelativeJumpTable(invalidTarget) == invalidTarget.size(), "Table with non-instruction targets was accepted");

    Bytes nonPadding = text;
    nonPadding.back() = 0x55;
    Require(Relinker::FindTrailingRelativeJumpTable(nonPadding) == nonPadding.size(), "Table followed by non-padding bytes was accepted");

    Bytes codeAfter = text;
    codeAfter.resize(48, 0xcc);
    codeAfter[32] = 0x55;
    codeAfter[33] = 0x5d;
    codeAfter[34] = 0xc3;
    std::size_t tableEnd = 0;
    Require(Relinker::FindTrailingRelativeJumpTable(codeAfter, TextAddress, &tableEnd) == tableOffset && tableEnd == 32, "Table padded to 16 bytes before more code was not found");
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> codeAfterFunctions{{TextAddress, TextAddress + 9}, {TextAddress + codeAfter.size(), TextAddress + codeAfter.size() + 8}};
    const auto codeAfterTables = Relinker::FindRelativeJumpTables(codeAfter, TextAddress, codeAfterFunctions);
    Require(codeAfterTables.size() == 1 && codeAfterTables[0].Begin == TextAddress + tableOffset && codeAfterTables[0].End == TextAddress + 32, "Table before more code did not end at its padding");

    std::size_t shiftedEnd = 0;
    Require(Relinker::FindTrailingRelativeJumpTable(nonPadding, TextAddress + 1, &shiftedEnd) == tableOffset && shiftedEnd == 31, "Table boundary ignored the code address");
    const auto shifted = Relinker::FindRelativeJumpTables(nonPadding, TextAddress + 1, {{TextAddress + 1, TextAddress + 10}, {TextAddress + 33, TextAddress + 41}});
    Require(shifted.size() == 1 && shifted[0].Begin == TextAddress + 17 && shifted[0].End == TextAddress + 32, "Function address was not passed to the table search");
    Require(Relinker::FindTrailingRelativeJumpTable(nonPadding, TextAddress + 8) == tableOffset, "Table ending on a 16-byte boundary was rejected");

    Bytes padded = text;
    padded.resize(48, 0xcc);
    const auto paddedTables = Relinker::FindRelativeJumpTables(padded, TextAddress, {{TextAddress, TextAddress + 9}, {TextAddress + 48, TextAddress + 56}});
    Require(paddedTables.size() == 1 && paddedTables[0].End == TextAddress + 48, "Padding to the end of the range was cut at the boundary");

    Bytes nopAtRangeEnd = text;
    const std::uint8_t fourByteNop[]{0x0f, 0x1f, 0x40, 0x00};
    std::memcpy(nopAtRangeEnd.data() + 28, fourByteNop, sizeof(fourByteNop));
    const auto cut = std::span<const std::uint8_t>(nopAtRangeEnd).first(31);
    Require(Relinker::FindTrailingRelativeJumpTable(cut) == cut.size(), "Padding check read past the inspected range");

    Bytes longNops = text;
    longNops.resize(48);
    const std::uint8_t fifteenByteNop[]{0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x2e, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
    const std::uint8_t nineByteNop[]{0x66, 0x0f, 0x1f, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::memcpy(longNops.data() + 24, fifteenByteNop, sizeof(fifteenByteNop));
    std::memcpy(longNops.data() + 39, nineByteNop, sizeof(nineByteNop));
    std::size_t longNopsEnd = 0;
    Require(Relinker::FindTrailingRelativeJumpTable(longNops, TextAddress, &longNopsEnd) == tableOffset && longNopsEnd == 48, "Table followed by a NOP across the boundary was not found");
    Bytes longNopsCodeAfter = longNops;
    longNopsCodeAfter.resize(64, 0xcc);
    longNopsCodeAfter[48] = 0x55;
    longNopsCodeAfter[49] = 0x5d;
    longNopsCodeAfter[50] = 0xc3;
    Require(Relinker::FindTrailingRelativeJumpTable(longNopsCodeAfter, TextAddress) == longNopsCodeAfter.size(), "Table ending inside a NOP was accepted");

    Bytes cutShort = text;
    cutShort.resize(56, 0xcc);
    const std::int32_t cutShortTargets[]{-16, -7, -6, -5, 0x20, 0x20};
    std::memcpy(cutShort.data() + tableOffset, cutShortTargets, sizeof(cutShortTargets));
    cutShort[48] = 0x55;
    cutShort[49] = 0x5d;
    cutShort[50] = 0xc3;
    Require(Relinker::FindRelativeJumpTables(cutShort, TextAddress, {{TextAddress, TextAddress + cutShort.size()}}).empty(), "Table followed by more of its function was accepted");
}

void CheckEmbeddedDataTables() {
    constexpr std::size_t tableOffset = 24;
    Bytes text(48, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + tableOffset);
    text[7] = 0xc3;
    for (std::size_t i = 8; i < tableOffset; ++i) text[i] = 0xcc;
    text[tableOffset] = 0x0f;
    text[tableOffset + 1] = 0x78;
    text[tableOffset + 2] = 0x33;
    text[tableOffset + 3] = 0x1e;

    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functions{
        {TextAddress, TextAddress + 8},
        {TextAddress + text.size(), TextAddress + text.size() + 8}
    };
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    Require(tables.size() == 1, "Expected one embedded data table");
    Require(tables[0].Function == TextAddress && tables[0].Begin == TextAddress + tableOffset && tables[0].End == TextAddress + text.size(), "Embedded data table range was incorrect");

    Bytes withBranch = text;
    withBranch[7] = 0xe8;
    SetDisplacement(withBranch, 8, 12, TextAddress + tableOffset);
    withBranch[12] = 0xc3;
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functionsWithBranch{
        {TextAddress, TextAddress + 13},
        {TextAddress + withBranch.size(), TextAddress + withBranch.size() + 8}
    };
    const auto tablesWithBranch = Relinker::FindRelativeJumpTables(withBranch, TextAddress, functionsWithBranch);
    Require(tablesWithBranch.empty(), "Branch target was incorrectly recognized as a data table");

    Bytes withValidCode = text;
    withValidCode[tableOffset] = 0x48;
    withValidCode[tableOffset + 1] = 0x89;
    withValidCode[tableOffset + 2] = 0xc0;
    withValidCode[tableOffset + 3] = 0xc3;
    for (std::size_t i = tableOffset + 4; i < withValidCode.size(); ++i) withValidCode[i] = 0xcc;
    const auto tablesWithValidCode = Relinker::FindRelativeJumpTables(withValidCode, TextAddress, functions);
    Require(tablesWithValidCode.empty(), "Valid unannotated code in gap was incorrectly recognized as a data table");

    Bytes withTransitiveBranch = text;
    withTransitiveBranch[7] = 0xe8;
    SetDisplacement(withTransitiveBranch, 8, 12, TextAddress + 16);
    withTransitiveBranch[12] = 0xc3;
    for (std::size_t i = 13; i < 16; ++i) withTransitiveBranch[i] = 0xcc;
    withTransitiveBranch[16] = 0xe9;
    SetDisplacement(withTransitiveBranch, 17, 21, TextAddress + tableOffset);
    for (std::size_t i = 21; i < tableOffset; ++i) withTransitiveBranch[i] = 0xcc;
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functionsTransitive{
        {TextAddress, TextAddress + 13},
        {TextAddress + withTransitiveBranch.size(), TextAddress + withTransitiveBranch.size() + 8}
    };
    const auto tablesTransitive = Relinker::FindRelativeJumpTables(withTransitiveBranch, TextAddress, functionsTransitive);
    Require(tablesTransitive.empty(), "Transitive branch target was incorrectly recognized as a data table");

    Bytes withTrailingCode(64, 0x90);
    withTrailingCode[0] = 0x48;
    withTrailingCode[1] = 0x8d;
    withTrailingCode[2] = 0x05;
    SetDisplacement(withTrailingCode, 3, 7, TextAddress + 16);
    withTrailingCode[7] = 0xc3;
    for (std::size_t i = 8; i < 16; ++i) withTrailingCode[i] = 0xcc;
    withTrailingCode[16] = 0x0f;
    withTrailingCode[17] = 0x78;
    withTrailingCode[18] = 0x11;
    withTrailingCode[19] = 0x22;
    withTrailingCode[20] = 0x33;
    withTrailingCode[21] = 0x44;
    for (std::size_t i = 22; i < 32; ++i) withTrailingCode[i] = 0xcc;
    withTrailingCode[32] = 0xb8;
    withTrailingCode[33] = 0x3b;
    withTrailingCode[34] = 0x00;
    withTrailingCode[35] = 0x00;
    withTrailingCode[36] = 0x00;
    withTrailingCode[37] = 0x0f;
    withTrailingCode[38] = 0x05;
    withTrailingCode[39] = 0xc3;
    for (std::size_t i = 40; i < withTrailingCode.size(); ++i) withTrailingCode[i] = 0x90;
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functionsTrailing{
        {TextAddress, TextAddress + 8},
        {TextAddress + withTrailingCode.size(), TextAddress + withTrailingCode.size() + 8}
    };
    const auto tablesTrailing = Relinker::FindRelativeJumpTables(withTrailingCode, TextAddress, functionsTrailing);
    Require(tablesTrailing.size() == 1, "Expected one bounded data table before trailing code");
    Require(tablesTrailing[0].Begin == TextAddress + 16 && tablesTrailing[0].End <= TextAddress + 32, "Mask overran into trailing code");

    Bytes withCallbackSyscall(48, 0x90);
    withCallbackSyscall[0] = 0x48;
    withCallbackSyscall[1] = 0x8d;
    withCallbackSyscall[2] = 0x05;
    SetDisplacement(withCallbackSyscall, 3, 7, TextAddress + 16);
    withCallbackSyscall[7] = 0xc3;
    for (std::size_t i = 8; i < 16; ++i) withCallbackSyscall[i] = 0xcc;
    withCallbackSyscall[16] = 0xb8;
    withCallbackSyscall[17] = 0x3b;
    withCallbackSyscall[18] = 0x00;
    withCallbackSyscall[19] = 0x00;
    withCallbackSyscall[20] = 0x00;
    withCallbackSyscall[21] = 0x0f;
    withCallbackSyscall[22] = 0x05;
    withCallbackSyscall[23] = 0x0f;
    withCallbackSyscall[24] = 0x78;
    withCallbackSyscall[25] = 0x11;
    withCallbackSyscall[26] = 0x22;
    withCallbackSyscall[27] = 0x33;
    withCallbackSyscall[28] = 0x44;
    withCallbackSyscall[29] = 0xc3;
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functionsCallback{
        {TextAddress, TextAddress + 8},
        {TextAddress + withCallbackSyscall.size(), TextAddress + withCallbackSyscall.size() + 8}
    };
    const auto tablesCallback = Relinker::FindRelativeJumpTables(withCallbackSyscall, TextAddress, functionsCallback);
    Require(tablesCallback.empty(), "Callback with syscall and undecodable tail was incorrectly recognized as a data table");

    Bytes whirlpoolText(4224, 0x90);
    whirlpoolText[0] = 0x48;
    whirlpoolText[1] = 0x8d;
    whirlpoolText[2] = 0x2d;
    SetDisplacement(whirlpoolText, 3, 7, TextAddress + 32);
    whirlpoolText[7] = 0xc3;
    for (std::size_t i = 8; i < 32; ++i) whirlpoolText[i] = 0xcc;
    static constexpr std::uint8_t whirlpoolPrefix[]{
        0x18, 0x18, 0x60, 0x18, 0xc0, 0x78, 0x30, 0xd8, 0x18, 0x18, 0x60, 0x18, 0xc0, 0x78, 0x30, 0xd8,
        0x0f, 0x78, 0x33, 0x1e
    };
    std::memcpy(whirlpoolText.data() + 32, whirlpoolPrefix, sizeof(whirlpoolPrefix));
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functionsWhirlpool{
        {TextAddress, TextAddress + 8},
        {TextAddress + whirlpoolText.size(), TextAddress + whirlpoolText.size() + 8}
    };
    const auto tablesWhirlpool = Relinker::FindRelativeJumpTables(whirlpoolText, TextAddress, functionsWhirlpool);
    Require(tablesWhirlpool.size() == 1, "Expected Whirlpool-style table to be recognized");
    Require(tablesWhirlpool[0].Begin == TextAddress + 32, "Whirlpool table start was incorrect");
}

void CheckDecodedPaddingSuffix() {
    Bytes text(0x38, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + 0x10);
    text[7] = 0xc3;
    for (std::size_t i = 8; i < 0x10; ++i) text[i] = 0xcc;
    static constexpr std::uint8_t suffix[]{0x0f, 0x78, 0xcc, 0xb8, 0x09, 0x00, 0x00, 0x00, 0x0f, 0x05, 0x00, 0x01, 0x90};
    std::memcpy(text.data() + 0x10, suffix, sizeof(suffix));
    text[0x30] = 0xc3;
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functions{
        {TextAddress, TextAddress + 8},
        {TextAddress + 0x30, TextAddress + 0x38}
    };
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    Require(tables.size() == 1, "Expected one decoded data table before a padding-suffixed syscall");
    Require(tables[0].Begin == TextAddress + 0x10 && tables[0].End == TextAddress + 0x12, "Mask extended over the syscall after a decoded padding suffix");
}

void CheckStubBeforeUndecodableData() {
    Bytes text(0x38, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + 0x10);
    text[7] = 0xc3;
    for (std::size_t i = 8; i < 0x10; ++i) text[i] = 0xcc;
    static constexpr std::uint8_t suffix[]{
        0x0f, 0x78, 0xcc, 0xb8, 0x09, 0x00, 0x00, 0x00,
        0x0f, 0x05, 0xc3, 0x0f, 0x78, 0x33, 0x1e
    };
    std::memcpy(text.data() + 0x10, suffix, sizeof(suffix));
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functions{
        {TextAddress, TextAddress + 8},
        {TextAddress + text.size(), TextAddress + text.size() + 8}
    };
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    Require(tables.size() == 1, "Expected one data table before a syscall stub followed by undecodable data");
    Require(tables[0].Begin == TextAddress + 0x10, "Data table start before a syscall stub was incorrect");
    Require(tables[0].End <= TextAddress + 0x13, "Mask covered the syscall stub before undecodable data");
}

void CheckTableWithPaddingAndReturnBytes() {
    constexpr std::size_t gap = 0x800;
    Bytes text(0x10 + gap, 0x90);
    text[0] = 0x48;
    text[1] = 0x8d;
    text[2] = 0x05;
    SetDisplacement(text, 3, 7, TextAddress + 0x10);
    text[7] = 0xc3;
    for (std::size_t i = 8; i < 0x10; ++i) text[i] = 0xcc;
    static constexpr std::uint8_t row[]{
        0x18, 0x18, 0x60, 0x18, 0xc0, 0x78, 0x30, 0xd8,
        0x18, 0x18, 0x60, 0x18, 0xc0, 0x78, 0x30, 0xd8,
        0x0f, 0x78, 0x33, 0x1e
    };
    for (std::size_t at = 0x10; at + sizeof(row) <= text.size(); at += sizeof(row))
        std::memcpy(text.data() + at, row, sizeof(row));
    static constexpr std::uint8_t paddedChain[]{0x00, 0xcc, 0x90, 0x48, 0x89, 0xc0, 0xc3};
    static constexpr std::uint8_t alignedChain[]{0x00, 0xcc, 0x90, 0x00, 0x48, 0x89, 0xc0, 0xc3};
    std::memcpy(text.data() + 0x10 + 0x3b8, paddedChain, sizeof(paddedChain));
    std::memcpy(text.data() + 0x10 + 0x4ac, alignedChain, sizeof(alignedChain));
    const std::vector<std::pair<VirtualAddress, VirtualAddress>> functions{
        {TextAddress, TextAddress + 8},
        {TextAddress + text.size(), TextAddress + text.size() + 8}
    };
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    Require(tables.size() == 1, "Expected a table with padding and return bytes to be recognized");
    Require(tables[0].Begin == TextAddress + 0x10, "Table with padding and return bytes started at the wrong address");
    Require(tables[0].End == TextAddress + text.size(), "Mask stopped inside a table with padding and return bytes");
}

void CheckLargeZeroGaps() {
    constexpr std::size_t stride = 0x10000;
    constexpr std::size_t count = 32;
    constexpr std::size_t functionSize = 8;
    constexpr std::size_t codeFill = 0x8000;
    Bytes text(stride * count, 0x00);
    std::vector<std::pair<VirtualAddress, VirtualAddress>> functions;
    for (std::size_t index = 0; index < count; ++index) {
        const auto begin = index * stride;
        text[begin] = 0x48;
        text[begin + 1] = 0x8d;
        text[begin + 2] = 0x05;
        SetDisplacement(text, begin + 3, begin + 7, TextAddress + begin + functionSize);
        text[begin + 7] = 0xc3;
        const auto table = begin + functionSize;
        text[table] = 0x0f;
        text[table + 1] = 0x78;
        text[table + 2] = 0x33;
        text[table + 3] = 0x1e;
        for (auto at = table + 4; at + 2 < begin + codeFill; at += 3) {
            text[at] = 0x48;
            text[at + 1] = 0x89;
            text[at + 2] = 0xc0;
        }
        functions.emplace_back(TextAddress + begin, TextAddress + begin + functionSize);
    }
    functions.emplace_back(TextAddress + stride * count, TextAddress + stride * count + functionSize);

    const auto started = std::chrono::steady_clock::now();
    const auto tables = Relinker::FindRelativeJumpTables(text, TextAddress, functions);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    Require(tables.size() == count, "Expected one data table per large gap");
    for (std::size_t index = 0; index < tables.size(); ++index) {
        Require(tables[index].Function == TextAddress + index * stride, "Large gap table was attached to the wrong function");
        Require(tables[index].Begin == TextAddress + index * stride + functionSize, "Large gap table start was incorrect");
        Require(tables[index].End == TextAddress + (index + 1) * stride, "Large gap table end was incorrect");
    }
    Require(elapsed < std::chrono::seconds(10), "Large gap analysis took too long");
}

}

int main() {
    try {
        CheckFallthrough();
        CheckBranches();
        CheckCalls();
        CheckCallsBeforeZeroPadding();
        CheckCallsThatStillFallThrough();
        CheckRelocatedTransfers();
        CheckSegmentRelativeTransfers();
        CheckAddressSizeOverride();
        CheckCodeAddresses();
        CheckSpeculativeRoots();
        CheckTrailingZeroRoots();
        CheckBorrowedReachability();
        CheckReachedMap();
        CheckAllocationFailureDuringWalk();
        CheckAddressOverflow();
        CheckHighVirtualAddress();
        CheckRelativeJumpTables();
        CheckEmbeddedDataTables();
        CheckDecodedPaddingSuffix();
        CheckStubBeforeUndecodableData();
        CheckTableWithPaddingAndReturnBytes();
        CheckLargeZeroGaps();
        std::cout << "Control-flow graph tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "control_flow_graph_tests: " << error.what() << '\n';
        return 1;
    }
}
