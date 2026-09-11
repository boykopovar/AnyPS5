#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <cstring>
#include <deque>
#include <limits>
#include <iterator>

namespace Relinker::UnusedNidFilter {

namespace {

struct Instruction {
    VirtualAddress Address;
    Codegen::DecodedInstructionInfo Info;
};

struct Region {
    VirtualAddress Begin;
    VirtualAddress End;
    std::set<VirtualAddress> Edges;
    std::set<VirtualAddress> Imports;
    std::vector<VirtualAddress> Instructions;
    std::size_t IndirectTransfers = 0;
};

bool endsFlow(Codegen::ControlFlowKind kind) {
    using enum Codegen::ControlFlowKind;
    return kind == Return || kind == Trap || kind == UnconditionalJump || kind == IndirectJump;
}

class Analyzer {
public:
    explicit Analyzer(const StrictReachabilityInput& input) : input(input) {}

    StrictReachabilityResult Run() {
        if (input.Text.empty() || input.Text.size() > std::numeric_limits<VirtualAddress>::max() - input.TextVaddr)
            throw RelinkerException("Strict filter: invalid text range");
        if (input.Entries.empty()) throw RelinkerException("Strict filter: no entry points");
        buildRegions();
        buildEdges();
        for (const auto& [slot, target] : input.Pointers) addAddressTaken(target);
        collectDataPointers();
        collectRelativeTables();
        std::deque<VirtualAddress> pending;
        std::set<VirtualAddress> live;
        const auto enqueue = [&](VirtualAddress target) {
            if (input.ImportSlots.contains(target)) result.ImportSlots.insert(target);
            if (!isCode(target)) return;
            const auto begin = owner(target).Begin;
            if (live.insert(begin).second) pending.push_back(begin);
        };
        for (const auto entry : input.Entries) {
            if (!isCode(entry)) throw RelinkerException("Strict filter: entry point lies outside code", entry);
            enqueue(entry);
        }
        for (const auto root : addressTaken) enqueue(root);
        while (!pending.empty()) {
            const auto begin = pending.front();
            pending.pop_front();
            const auto& region = regions.at(begin);
            result.Instructions.insert(region.Instructions.begin(), region.Instructions.end());
            result.ImportSlots.insert(region.Imports.begin(), region.Imports.end());
            result.IndirectTransfers += region.IndirectTransfers;
            for (const auto target : region.Edges) enqueue(target);
        }
        result.LiveRegions = live.size();
        result.TotalRegions = regions.size();
        result.AddressTakenRoots = addressTaken.size();
        return result;
    }

private:
    const StrictReachabilityInput& input;
    Codegen::X64InstructionDecoder decoder;
    std::map<VirtualAddress, Region> regions;
    std::set<VirtualAddress> addressTaken;
    std::set<VirtualAddress> tableBases;
    StrictReachabilityResult result;

    bool isCode(VirtualAddress address) const {
        return address >= input.TextVaddr && address - input.TextVaddr < input.Text.size();
    }

    Region& owner(VirtualAddress address) {
        auto position = regions.upper_bound(address);
        if (position == regions.begin() || std::prev(position)->second.End <= address)
            throw RelinkerException("Strict filter: code address has no region", address);
        return std::prev(position)->second;
    }

    Instruction decode(VirtualAddress address, VirtualAddress end) const {
        const auto offset = static_cast<std::size_t>(address - input.TextVaddr);
        Codegen::DecodedInstructionInfo info;
        try {
            info = decoder.DecodeInstruction(input.Text.data() + offset, static_cast<std::size_t>(end - address));
        } catch (const Codegen::CodegenException& error) {
            throw RelinkerException(std::string("Strict filter: ") + error.what(), address);
        }
        if (info.Length == 0 || info.Length > end - address) throw RelinkerException("Strict filter: instruction crosses a region boundary", address);
        return {address, info};
    }

    bool isZeroPadding(VirtualAddress begin, VirtualAddress end) const {
        const auto first = input.Text.begin() + static_cast<std::ptrdiff_t>(begin - input.TextVaddr);
        const auto last = input.Text.begin() + static_cast<std::ptrdiff_t>(end - input.TextVaddr);
        return std::all_of(first, last, [](std::uint8_t byte) { return byte == 0; });
    }

    void addGap(VirtualAddress begin, VirtualAddress end) {
        VirtualAddress regionBegin = begin;
        for (auto address = begin; address < end;) {
            if (isZeroPadding(address, end)) break;
            const auto instruction = decode(address, end);
            address += instruction.Info.Length;
            if (endsFlow(instruction.Info.FlowKind)) {
                regions.emplace(regionBegin, Region{regionBegin, address, {}, {}, {}});
                regionBegin = address;
            }
        }
        if (regionBegin != end) regions.emplace(regionBegin, Region{regionBegin, end, {}, {}, {}});
    }

    void buildRegions() {
        auto functions = input.Functions;
        std::sort(functions.begin(), functions.end(), [](const auto& left, const auto& right) { return left.Begin < right.Begin; });
        auto previousEnd = input.TextVaddr;
        const auto textEnd = input.TextVaddr + input.Text.size();
        for (const auto& function : functions) {
            if (function.Begin < previousEnd || function.End <= function.Begin || function.End > textEnd)
                throw RelinkerException("Strict filter: invalid or overlapping function range", function.Begin);
            addGap(previousEnd, function.Begin);
            Region region{function.Begin, function.End, {}, {}, {}};
            region.Edges.insert(function.ExtraTargets.begin(), function.ExtraTargets.end());
            regions.emplace(function.Begin, std::move(region));
            previousEnd = function.End;
        }
        addGap(previousEnd, textEnd);
    }

    void addAddressTaken(VirtualAddress target) {
        if (isCode(target) || input.ImportSlots.contains(target)) addressTaken.insert(target);
    }

    void buildEdges() {
        for (auto& [begin, region] : regions) {
            Codegen::ControlFlowKind lastFlow = Codegen::ControlFlowKind::Sequential;
            for (auto address = begin; address < region.End;) {
                if (isZeroPadding(address, region.End)) {
                    lastFlow = Codegen::ControlFlowKind::Sequential;
                    break;
                }
                const auto instruction = decode(address, region.End);
                const auto& info = instruction.Info;
                const auto next = address + info.Length;
                const auto offset = static_cast<std::size_t>(address - input.TextVaddr);
                region.Instructions.push_back(address);
                if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                    const auto target = next + static_cast<VirtualAddress>(info.BranchDisp);
                    if (!isCode(target)) throw RelinkerException("Strict filter: direct branch leaves code", address);
                    region.Edges.insert(target);
                }
                if (info.HasRipRelativeDisp) {
                    std::int32_t displacement;
                    std::memcpy(&displacement, input.Text.data() + offset + info.RipRelativeDispOffset, sizeof(displacement));
                    const auto target = next + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
                    if (input.ImportSlots.contains(target)) region.Imports.insert(target);
                    if (!info.IsTwoByteOpcode && info.Opcode == 0x8D) {
                        addAddressTaken(target);
                        tableBases.insert(target);
                    }
                    if (const auto pointer = input.Pointers.find(target); pointer != input.Pointers.end()) region.Edges.insert(pointer->second);
                }
                using enum Codegen::ControlFlowKind;
                if (info.FlowKind == IndirectCall || info.FlowKind == IndirectJump || ((info.FlowKind == Call || info.FlowKind == UnconditionalJump) && info.HasRipRelativeDisp)) ++region.IndirectTransfers;
                if (!info.IsTwoByteOpcode && ((info.Opcode >= 0xB8 && info.Opcode <= 0xBF) || info.Opcode == 0x68 || info.Opcode == 0xC7)) {
                    for (const auto width : {4u, 8u}) {
                        if (info.Length <= width) continue;
                        std::uint64_t value = 0;
                        std::memcpy(&value, input.Text.data() + offset + info.Length - width, width);
                        addAddressTaken(value);
                    }
                }
                lastFlow = info.FlowKind;
                address = next;
            }
            if (!endsFlow(lastFlow) && isCode(region.End)) region.Edges.insert(region.End);
        }
    }

    void collectDataPointers() {
        for (const auto& data : input.Data) {
            for (std::size_t offset = 0; offset + 8 <= data.Bytes.size(); ++offset) {
                std::uint64_t target;
                std::memcpy(&target, data.Bytes.data() + offset, sizeof(target));
                addAddressTaken(target);
            }
        }
    }

    void collectRelativeTables() {
        for (const auto base : tableBases) {
            for (const auto& data : input.Data) {
                if (base < data.Address || base - data.Address >= data.Bytes.size()) continue;
                for (auto offset = static_cast<std::size_t>(base - data.Address); offset + 4 <= data.Bytes.size(); offset += 4) {
                    std::int32_t displacement;
                    std::memcpy(&displacement, data.Bytes.data() + offset, sizeof(displacement));
                    const auto target = base + static_cast<std::uint64_t>(static_cast<std::int64_t>(displacement));
                    if (!isCode(target)) break;
                    addAddressTaken(target);
                }
            }
        }
    }
};

}

StrictReachabilityResult AnalyzeStrictReachability(const StrictReachabilityInput& input) {
    return Analyzer(input).Run();
}

}
