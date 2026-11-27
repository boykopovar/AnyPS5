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
    std::map<VirtualAddress, bool> Edges;
    std::set<VirtualAddress> Imports;
    std::set<VirtualAddress> Instructions;
    std::set<VirtualAddress> TrustedInstructions;
    std::size_t IndirectTransfers = 0;
    bool UnknownSegmentAccess = false;
};

bool endsFlow(Codegen::ControlFlowKind kind) {
    using enum Codegen::ControlFlowKind;
    return kind == Return || kind == Trap || kind == UnconditionalJump || kind == IndirectJump;
}

void readRelativeTable(std::span<const std::uint8_t> bytes, VirtualAddress address, VirtualAddress base, VirtualAddress textVaddr, std::size_t textSize, std::vector<VirtualAddress>& targets) {
    if (base < address || base - address >= bytes.size()) return;
    const auto begin = static_cast<std::size_t>(base - address);
    for (auto offset = begin; offset + 4 <= bytes.size(); offset += 4) {
        std::int32_t displacement;
        std::memcpy(&displacement, bytes.data() + offset, sizeof(displacement));
        const auto target = base + static_cast<std::uint64_t>(static_cast<std::int64_t>(displacement));
        if (target < textVaddr || target - textVaddr >= textSize || (target >= base && target - base < offset + 4 - begin)) break;
        targets.push_back(target);
    }
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
        std::size_t previousTableBaseCount;
        do {
            previousTableBaseCount = tableBases.size();
            collectRelativeTables();
            buildEdgesFromRoots();
        } while (tableBases.size() != previousTableBaseCount);
        std::sort(addressTaken.begin(), addressTaken.end());
        addressTaken.erase(std::unique(addressTaken.begin(), addressTaken.end()), addressTaken.end());
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
        bool unknownSegmentAccess = false;
        while (!pending.empty()) {
            const auto begin = pending.front();
            pending.pop_front();
            const auto& region = regions.at(begin);
            result.Instructions.insert(region.Instructions.begin(), region.Instructions.end());
            result.ImportSlots.insert(region.Imports.begin(), region.Imports.end());
            unknownSegmentAccess |= region.UnknownSegmentAccess;
            result.IndirectTransfers += region.IndirectTransfers;
            for (const auto& edge : region.Edges) enqueue(edge.first);
        }
        if (unknownSegmentAccess) result.ImportSlots = input.ImportSlots;
        result.LiveRegions = live.size();
        result.TotalRegions = regions.size();
        result.AddressTakenRoots = addressTaken.size();
        return result;
    }

private:
    const StrictReachabilityInput& input;
    Codegen::X64InstructionDecoder decoder;
    std::map<VirtualAddress, Region> regions;
    std::vector<StrictCodeRegion> sortedFunctions;
    std::map<VirtualAddress, bool> decodedRoots;
    std::vector<VirtualAddress> addressTaken;
    std::set<VirtualAddress> tableBases;
    StrictReachabilityResult result;
    Region* lastOwner = nullptr;

    void addEdge(Region& region, VirtualAddress target, bool trusted) {
        const auto [position, inserted] = region.Edges.emplace(target, trusted);
        if (!inserted) position->second = position->second || trusted;
    }

    bool isCode(VirtualAddress address) const {
        return address >= input.TextVaddr && address - input.TextVaddr < input.Text.size();
    }

    VirtualAddress rootLimit(VirtualAddress target) const {
        const auto next = std::upper_bound(sortedFunctions.begin(), sortedFunctions.end(), target,
            [](VirtualAddress address, const StrictCodeRegion& function) { return address < function.Begin; });
        if (next != sortedFunctions.begin()) {
            const auto& previous = *(next - 1);
            if (target < previous.End) return previous.End;
        }
        if (next != sortedFunctions.end()) return next->Begin;
        return input.TextVaddr + input.Text.size();
    }

    Region& owner(VirtualAddress address) {
        if (lastOwner != nullptr && address >= lastOwner->Begin && address < lastOwner->End) return *lastOwner;
        auto position = regions.upper_bound(address);
        if (position == regions.begin() || std::prev(position)->second.End <= address)
            throw RelinkerException("Strict filter: code address has no region", address);
        lastOwner = &std::prev(position)->second;
        return *lastOwner;
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

    VirtualAddress trailingZeroStart(VirtualAddress begin, VirtualAddress end) const {
        auto pos = input.Text.data() + static_cast<std::ptrdiff_t>(end - input.TextVaddr);
        const auto first = input.Text.data() + static_cast<std::ptrdiff_t>(begin - input.TextVaddr);
        while (pos != first && *(pos - 1) == 0) --pos;
        return input.TextVaddr + static_cast<VirtualAddress>(pos - input.Text.data());
    }

    void addGap(VirtualAddress begin, VirtualAddress end) {
        const auto effectiveEnd = trailingZeroStart(begin, end);
        VirtualAddress regionBegin = begin;
        for (auto address = begin; address < effectiveEnd;) {
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
        sortedFunctions = input.Functions;
        std::sort(sortedFunctions.begin(), sortedFunctions.end(), [](const auto& left, const auto& right) { return left.Begin < right.Begin; });
        auto previousEnd = input.TextVaddr;
        const auto textEnd = input.TextVaddr + input.Text.size();
        for (const auto& function : sortedFunctions) {
            if (function.Begin < previousEnd || function.End <= function.Begin || function.End > textEnd)
                throw RelinkerException("Strict filter: invalid or overlapping function range", function.Begin);
            addGap(previousEnd, function.Begin);
            Region region{function.Begin, function.End, {}, {}, {}};
            for (const auto target : function.ExtraTargets) addEdge(region, target, true);
            regions.emplace(function.Begin, std::move(region));
            previousEnd = function.End;
        }
        addGap(previousEnd, textEnd);
    }

    void addAddressTaken(VirtualAddress target) {
        if (isCode(target) || input.ImportSlots.contains(target)) addressTaken.push_back(target);
    }

    void decodeFrom(VirtualAddress begin, Region& region, VirtualAddress limit, bool tolerateInvalid, bool stopAtTerminal) {
        const auto effectiveEnd = trailingZeroStart(begin, limit);
        Codegen::ControlFlowKind lastFlow = Codegen::ControlFlowKind::Sequential;
        auto address = begin;
        bool incomplete = false;
        while (address < effectiveEnd) {
            Instruction instruction;
            try {
                instruction = decode(address, limit);
            } catch (const RelinkerException&) {
                if (!tolerateInvalid) throw;
                region.UnknownSegmentAccess = true;
                incomplete = true;
                break;
            }
            const auto& info = instruction.Info;
            const auto next = address + info.Length;
            const auto offset = static_cast<std::size_t>(address - input.TextVaddr);
            const auto newInstruction = region.Instructions.insert(address).second;
            if (!tolerateInvalid) region.TrustedInstructions.insert(address);
            lastFlow = info.FlowKind;
            if (info.SegmentPrefix != 0 && info.HasMemoryOperand) region.UnknownSegmentAccess = true;
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const bool overflows = (info.BranchDisp < 0 && next < static_cast<VirtualAddress>(-info.BranchDisp))
                    || (info.BranchDisp > 0 && std::numeric_limits<VirtualAddress>::max() - next < static_cast<VirtualAddress>(info.BranchDisp));
                if (overflows) {
                    if (!tolerateInvalid) throw RelinkerException("Strict filter: direct branch leaves code", address);
                    region.UnknownSegmentAccess = true;
                    incomplete = true;
                    break;
                }
                const auto target = next + static_cast<VirtualAddress>(info.BranchDisp);
                if (!isCode(target)) {
                    if (!tolerateInvalid) throw RelinkerException("Strict filter: direct branch leaves code", address);
                    region.UnknownSegmentAccess = true;
                    incomplete = true;
                    break;
                }
                addEdge(region, target, !tolerateInvalid);
            }
            if (info.HasRipRelativeDisp) {
                std::int32_t displacement = 0;
                std::memcpy(&displacement, input.Text.data() + offset + info.RipRelativeDispOffset, sizeof(displacement));
                auto target = next + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
                if (info.AddressSize32) target = static_cast<std::uint32_t>(target);
                if (info.SegmentPrefix == 0 || !info.HasMemoryOperand) {
                    if (input.ImportSlots.contains(target)) region.Imports.insert(target);
                    if (!info.IsTwoByteOpcode && info.Opcode == 0x8D) {
                        addAddressTaken(target);
                        tableBases.insert(target);
                    }
                    if (const auto pointer = input.Pointers.find(target); pointer != input.Pointers.end()) addEdge(region, pointer->second, false);
                }
            }
            using enum Codegen::ControlFlowKind;
            if (newInstruction && (info.FlowKind == IndirectCall || info.FlowKind == IndirectJump || ((info.FlowKind == Call || info.FlowKind == UnconditionalJump) && info.HasRipRelativeDisp))) ++region.IndirectTransfers;
            if (!info.IsTwoByteOpcode && ((info.Opcode >= 0xB8 && info.Opcode <= 0xBF) || info.Opcode == 0x68 || info.Opcode == 0xC7)) {
                for (const auto width : {4u, 8u}) {
                    if (info.Length <= width) continue;
                    std::uint64_t value = 0;
                    std::memcpy(&value, input.Text.data() + offset + info.Length - width, width);
                    addAddressTaken(value);
                }
            }
            address = next;
            if (stopAtTerminal && endsFlow(info.FlowKind)) break;
        }
        if (!incomplete && address < limit && (!stopAtTerminal || !endsFlow(lastFlow))) lastFlow = Codegen::ControlFlowKind::Sequential;
        if (!incomplete && !endsFlow(lastFlow) && isCode(limit)) addEdge(region, limit, !tolerateInvalid);
    }

    void buildEdges() {
        for (auto& [begin, region] : regions) {
            decodedRoots.emplace(begin, true);
            decodeFrom(begin, region, region.End, false, false);
        }
    }

    void buildEdgesFromRoots() {
        std::deque<VirtualAddress> pending;
        std::map<VirtualAddress, bool> queued;
        const auto enqueueRoot = [&](VirtualAddress target, bool trusted) {
            if (!isCode(target)) return;
            const auto [position, inserted] = queued.emplace(target, trusted);
            if (inserted) pending.push_back(target);
            else position->second = position->second || trusted;
        };
        for (const auto entry : input.Entries) enqueueRoot(entry, true);
        for (const auto& [slot, target] : input.Pointers) enqueueRoot(target, false);
        for (const auto& function : input.Functions)
            for (const auto target : function.ExtraTargets) enqueueRoot(target, true);
        for (const auto& [begin, region] : regions)
            for (const auto& [target, trusted] : region.Edges) enqueueRoot(target, trusted);
        std::size_t addressTakenIndex = 0;
        const auto enqueueAddressTaken = [&] {
            while (addressTakenIndex < addressTaken.size()) enqueueRoot(addressTaken[addressTakenIndex++], false);
        };
        enqueueAddressTaken();
        while (!pending.empty()) {
            const auto target = pending.front();
            pending.pop_front();
            const auto trusted = queued.at(target);
            queued.erase(target);
            const auto [decoded, inserted] = decodedRoots.emplace(target, trusted);
            if (!inserted) {
                if (decoded->second || !trusted) continue;
                decoded->second = true;
            }
            auto& region = owner(target);
            if (region.TrustedInstructions.contains(target) || (region.Instructions.contains(target) && !trusted)) continue;
            decodeFrom(target, region, rootLimit(target), !trusted, true);
            for (const auto& [edge, edgeTrusted] : region.Edges) enqueueRoot(edge, edgeTrusted);
            enqueueAddressTaken();
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
            for (auto slot = base; input.ImportSlots.contains(slot) || input.Pointers.contains(slot); slot += sizeof(std::uint64_t)) addAddressTaken(slot);
            for (const auto target : ReadRelativeTableTargets(input.Data, base, input.TextVaddr, input.Text.size())) addAddressTaken(target);
            for (const auto target : ReadRelativeTableTargets(input.Text, base, input.TextVaddr)) addAddressTaken(target);
        }
    }
};

}

StrictReachabilityResult AnalyzeStrictReachability(const StrictReachabilityInput& input) {
    return Analyzer(input).Run();
}

std::vector<VirtualAddress> ReadRelativeTableTargets(const std::vector<StrictDataRegion>& data, VirtualAddress base, VirtualAddress textVaddr, std::size_t textSize) {
    std::vector<VirtualAddress> targets;
    if (textSize == 0 || textSize > std::numeric_limits<VirtualAddress>::max() - textVaddr) {
        throw RelinkerException("Strict filter: invalid text range");
    }
    for (const auto& region : data) readRelativeTable(region.Bytes, region.Address, base, textVaddr, textSize, targets);
    return targets;
}

std::vector<VirtualAddress> ReadRelativeTableTargets(std::span<const std::uint8_t> text, VirtualAddress base, VirtualAddress textVaddr) {
    std::vector<VirtualAddress> targets;
    if (text.empty() || text.size() > std::numeric_limits<VirtualAddress>::max() - textVaddr) {
        throw RelinkerException("Relative jump table: invalid text range");
    }
    readRelativeTable(text, textVaddr, base, textVaddr, text.size(), targets);
    return targets;
}

}
