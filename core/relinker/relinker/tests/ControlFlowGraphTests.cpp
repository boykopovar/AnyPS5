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

}

}

int main() {
    try {
        CheckFallthrough();
        CheckBranches();
        CheckCalls();
        CheckRelocatedTransfers();
        CheckCodeAddresses();
        CheckSpeculativeRoots();
        CheckBorrowedReachability();
        CheckAddressOverflow();
        CheckHighVirtualAddress();
        CheckRelativeJumpTables();
        std::cout << "Control-flow graph tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "control_flow_graph_tests: " << error.what() << '\n';
        return 1;
    }
}
