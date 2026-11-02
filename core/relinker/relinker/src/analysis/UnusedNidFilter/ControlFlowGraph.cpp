#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <limits>

namespace Relinker::UnusedNidFilter {

class ControlFlowGraph : public IControlFlowGraph {
public:
    explicit ControlFlowGraph(std::unordered_set<VirtualAddress> reachable)
        : _owned(std::move(reachable)), _borrowed(nullptr) {}

    explicit ControlFlowGraph(const std::unordered_set<VirtualAddress>* reachable)
        : _borrowed(reachable) {}

    const std::unordered_set<VirtualAddress>& ReachableVaddrs() const override {
        return _borrowed ? *_borrowed : _owned;
    }
    bool IsReachable(VirtualAddress vaddr) const override {
        return (_borrowed ? *_borrowed : _owned).count(vaddr) > 0;
    }

private:
    std::unordered_set<VirtualAddress> _owned;
    const std::unordered_set<VirtualAddress>* _borrowed = nullptr;
};

namespace {

constexpr std::uint8_t Reached = 1;
constexpr std::uint8_t Queued = 2;

void walkGraph(std::span<const std::uint8_t> text, VirtualAddress textVaddr, VirtualAddress entryVaddr, const std::vector<VirtualAddress>& extraEntries, const IRelativeRelocationIndex& relativeRelocations, const std::vector<StrictDataRegion>* data, bool followCodeAddresses, std::vector<std::uint8_t>& state, const std::unordered_set<VirtualAddress>* outside, std::size_t reachedCount, std::vector<VirtualAddress>& added, const std::function<void(std::uint64_t)>& progress, const std::function<bool(VirtualAddress)>& isData) {
    if (text.size() > std::numeric_limits<VirtualAddress>::max() - textVaddr)
        throw RelinkerException("Text section range exceeds the address space");
    std::size_t codeSize = text.size();
    while (codeSize > 0 && text[codeSize - 1] == 0) --codeSize;
    const Codegen::X64InstructionDecoder decoder;
    std::vector<std::size_t> queued;
    struct ClearQueued {
        std::vector<std::uint8_t>& state;
        std::vector<std::size_t>& queued;
        ~ClearQueued() {
            for (const auto offset : queued)
                if (state[offset] == Queued) state[offset] = 0;
        }
    } clearQueued{state, queued};
    std::queue<VirtualAddress> worklist;
    std::vector<VirtualAddress> addressTaken;
    bool speculative = false;

    auto enqueue = [&](VirtualAddress va) {
        const bool inText = va >= textVaddr && va - textVaddr < text.size();
        if (inText ? state[static_cast<std::size_t>(va - textVaddr)] == Reached : outside != nullptr && outside->count(va) > 0) return;
        if (isData && isData(va)) return;
        if (!inText) return;
        const auto offset = static_cast<std::size_t>(va - textVaddr);
        if (state[offset] == Queued) return;
        state[offset] = Queued;
        queued.push_back(offset);
        worklist.push(va);
    };

    auto zeroPaddingFollows = [&](VirtualAddress va) {
        const auto offset = static_cast<std::size_t>(va - textVaddr);
        const auto aligned = (offset + 15) & ~std::size_t{15};
        return aligned != offset && aligned <= text.size() && std::all_of(text.begin() + offset, text.begin() + aligned, [](std::uint8_t value) { return value == 0; });
    };

    auto takeAddress = [&](VirtualAddress va) {
        if (data == nullptr) return;
        if (speculative)
            enqueue(va);
        else
            addressTaken.push_back(va);
    };

    enqueue(entryVaddr);
    for (VirtualAddress va : extraEntries) enqueue(va);
    for (VirtualAddress va : relativeRelocations.Targets()) takeAddress(va);

    while (!worklist.empty() || !speculative) {
        if (worklist.empty()) {
            speculative = true;
            for (VirtualAddress va : addressTaken) enqueue(va);
            continue;
        }

        VirtualAddress va = worklist.front();
        worklist.pop();
        const VirtualAddress blockEntry = va;

        while (va >= textVaddr && va - textVaddr < text.size() && (va == blockEntry || va - textVaddr < codeSize) && state[static_cast<std::size_t>(va - textVaddr)] != Reached && !(isData && isData(va))) {
            std::size_t bufOff = static_cast<std::size_t>(va - textVaddr);
            std::size_t available = text.size() - bufOff;
            if (available == 0) throw RelinkerException("CFG: zero available bytes at target", va);

            Codegen::DecodedInstructionInfo info;
            if (speculative) {
                try {
                    info = decoder.DecodeInstruction(text.data() + bufOff, available);
                } catch (const Codegen::CodegenException&) {
                    break;
                }
            } else {
                info = decoder.DecodeInstruction(text.data() + bufOff, available);
            }
            state[bufOff] = Reached;
            added.push_back(va);
            ++reachedCount;
            if (progress && reachedCount % 4096 == 0) progress(reachedCount);

            VirtualAddress nextVaddr = va + static_cast<VirtualAddress>(info.Length);

            if (followCodeAddresses && info.HasRipRelativeDisp && text[bufOff + info.OpcodeOffset] == 0x8d && (info.RexPrefix & 8)) {
                std::int32_t displacement = 0;
                std::memcpy(&displacement, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                auto target = nextVaddr + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
                if (info.AddressSize32) target = static_cast<std::uint32_t>(target);
                enqueue(target);
            }

            if (data != nullptr && info.HasRipRelativeDisp && !info.IsTwoByteOpcode && info.Opcode == 0x8D) {
                std::int32_t displacement = 0;
                std::memcpy(&displacement, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                auto base = nextVaddr + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
                if (info.AddressSize32) base = static_cast<std::uint32_t>(base);
                takeAddress(base);
                for (VirtualAddress target : ReadRelativeTableTargets(*data, base, textVaddr, text.size())) takeAddress(target);
            }

            bool fallThrough = true;
            switch (info.FlowKind) {
                using enum Codegen::ControlFlowKind;
                case Sequential:
                    break;
                case ConditionalBranch:
                    if (info.HasBranchTarget)
                        enqueue(nextVaddr + static_cast<VirtualAddress>(info.BranchDisp));
                    break;
                case UnconditionalJump:
                    fallThrough = false;
                    if (info.HasRipRelativeDisp) {
                        std::int32_t disp = 0;
                        std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                        VirtualAddress slotVaddr = nextVaddr + static_cast<VirtualAddress>(static_cast<std::int64_t>(disp));
                        if (info.AddressSize32) slotVaddr = static_cast<std::uint32_t>(slotVaddr);
                        auto resolved = info.SegmentPrefix == 0 ? relativeRelocations.TargetOfSlot(slotVaddr) : std::nullopt;
                        if (resolved.has_value())
                            enqueue(*resolved);
                    } else if (info.HasBranchTarget) {
                        enqueue(nextVaddr + static_cast<VirtualAddress>(info.BranchDisp));
                    }
                    break;
                case Call:
                    if (info.HasRipRelativeDisp) {
                        std::int32_t disp = 0;
                        std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                        VirtualAddress slotVaddr = nextVaddr + static_cast<VirtualAddress>(static_cast<std::int64_t>(disp));
                        if (info.AddressSize32) slotVaddr = static_cast<std::uint32_t>(slotVaddr);
                        auto resolved = info.SegmentPrefix == 0 ? relativeRelocations.TargetOfSlot(slotVaddr) : std::nullopt;
                        if (resolved.has_value())
                            enqueue(*resolved);
                    } else if (info.HasBranchTarget) {
                        enqueue(nextVaddr + static_cast<VirtualAddress>(info.BranchDisp));
                    }
                    fallThrough = !zeroPaddingFollows(nextVaddr);
                    break;
                case IndirectCall:
                    fallThrough = !zeroPaddingFollows(nextVaddr);
                    break;
                case Return:
                case IndirectJump:
                case Trap:
                    fallThrough = false;
                    break;
            }
            if (!fallThrough) break;
            va = nextVaddr;
        }
    }
}

void buildGraph(std::span<const std::uint8_t> text, VirtualAddress textVaddr, VirtualAddress entryVaddr, const std::vector<VirtualAddress>& extraEntries, const IRelativeRelocationIndex& relativeRelocations, const std::vector<StrictDataRegion>* data, bool followCodeAddresses, std::unordered_set<VirtualAddress>& reachable, const std::function<void(std::uint64_t)>& progress, const std::function<bool(VirtualAddress)>& isData) {
    if (text.size() > std::numeric_limits<VirtualAddress>::max() - textVaddr)
        throw RelinkerException("Text section range exceeds the address space");
    std::vector<std::uint8_t> state(text.size(), 0);
    for (const VirtualAddress va : reachable)
        if (va >= textVaddr && va - textVaddr < text.size()) state[static_cast<std::size_t>(va - textVaddr)] = Reached;
    std::vector<VirtualAddress> added;
    try {
        walkGraph(text, textVaddr, entryVaddr, extraEntries, relativeRelocations, data, followCodeAddresses, state, &reachable, reachable.size(), added, progress, isData);
    } catch (...) {
        reachable.insert(added.begin(), added.end());
        throw;
    }
    reachable.insert(added.begin(), added.end());
}

}

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(std::span<const std::uint8_t> text, VirtualAddress textVaddr, VirtualAddress entryVaddr, const std::vector<VirtualAddress>& extraEntries, const IRelativeRelocationIndex& relativeRelocations, bool followCodeAddresses, std::unordered_set<VirtualAddress>& reachable, const std::function<void(std::uint64_t)>& progress, const std::function<bool(VirtualAddress)>& isData) {
    buildGraph(text, textVaddr, entryVaddr, extraEntries, relativeRelocations, nullptr, followCodeAddresses, reachable, progress, isData);
    return std::make_unique<ControlFlowGraph>(&reachable);
}

std::vector<VirtualAddress> ExtendControlFlowGraph(std::span<const std::uint8_t> text, VirtualAddress textVaddr, VirtualAddress entryVaddr, const std::vector<VirtualAddress>& extraEntries, const IRelativeRelocationIndex& relativeRelocations, bool followCodeAddresses, std::vector<std::uint8_t>& reached, const std::function<void(std::uint64_t)>& progress, const std::function<bool(VirtualAddress)>& isData) {
    if (text.size() > std::numeric_limits<VirtualAddress>::max() - textVaddr)
        throw RelinkerException("Text section range exceeds the address space");
    if (reached.size() != text.size()) throw RelinkerException("CFG: the reached map does not cover the text", textVaddr);
    std::vector<VirtualAddress> added;
    walkGraph(text, textVaddr, entryVaddr, extraEntries, relativeRelocations, nullptr, followCodeAddresses, reached, nullptr, static_cast<std::size_t>(std::count(reached.begin(), reached.end(), Reached)), added, progress, isData);
    std::sort(added.begin(), added.end());
    return added;
}

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    std::span<const std::uint8_t> text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    bool followCodeAddresses
) {
    std::unordered_set<VirtualAddress> reachable;
    BuildControlFlowGraph(text, textVaddr, entryVaddr, extraEntries, relativeRelocations, followCodeAddresses, reachable);
    return std::make_unique<ControlFlowGraph>(std::move(reachable));
}

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    bool followCodeAddresses
) {
    return BuildControlFlowGraph(std::span<const std::uint8_t>(text.data(), text.size()), textVaddr, entryVaddr, extraEntries, relativeRelocations, followCodeAddresses);
}

std::unique_ptr<IControlFlowGraph> BuildAddressTakenControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    const std::vector<StrictDataRegion>& data
) {
    std::unordered_set<VirtualAddress> reachable;
    buildGraph(std::span<const std::uint8_t>(text.data(), text.size()), textVaddr, entryVaddr, extraEntries, relativeRelocations, &data, false, reachable, {}, {});
    return std::make_unique<ControlFlowGraph>(std::move(reachable));
}

}
