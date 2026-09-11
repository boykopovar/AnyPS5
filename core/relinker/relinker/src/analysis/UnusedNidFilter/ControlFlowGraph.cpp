#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <unordered_map>
#include <queue>
#include <cstring>

namespace Relinker::UnusedNidFilter {

class ControlFlowGraph : public IControlFlowGraph {
public:
    explicit ControlFlowGraph(std::unordered_set<VirtualAddress> reachable)
        : _reachable(std::move(reachable)) {}

    const std::unordered_set<VirtualAddress>& ReachableVaddrs() const override { return _reachable; }
    bool IsReachable(VirtualAddress vaddr) const override { return _reachable.count(vaddr) > 0; }

private:
    std::unordered_set<VirtualAddress> _reachable;
};

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations
) {
    const Codegen::X64InstructionDecoder decoder;
    std::unordered_set<VirtualAddress> reachable;
    std::queue<VirtualAddress> worklist;

    auto enqueue = [&](VirtualAddress va) {
        if (reachable.count(va) > 0) return;
        if (va < textVaddr || va >= textVaddr + static_cast<VirtualAddress>(text.size())) return;
        reachable.insert(va);
        worklist.push(va);
    };

    enqueue(entryVaddr);
    for (VirtualAddress va : extraEntries) enqueue(va);

    while (!worklist.empty()) {
        VirtualAddress va = worklist.front();
        worklist.pop();

        if (va < textVaddr || va >= textVaddr + static_cast<VirtualAddress>(text.size()))
            throw RelinkerException("CFG: jump target outside text segment", va);

        std::size_t bufOff = static_cast<std::size_t>(va - textVaddr);
        std::size_t available = text.size() - bufOff;
        if (available == 0) throw RelinkerException("CFG: zero available bytes at target", va);

        Codegen::DecodedInstructionInfo info = decoder.DecodeInstruction(text.data() + bufOff, available);

        VirtualAddress nextVaddr = va + static_cast<VirtualAddress>(info.Length);

        switch (info.FlowKind) {
            using enum Codegen::ControlFlowKind;
            case Sequential:
                enqueue(nextVaddr);
                break;
            case ConditionalBranch:
                enqueue(nextVaddr);
                if (info.HasBranchTarget)
                    enqueue(static_cast<VirtualAddress>(static_cast<std::int64_t>(nextVaddr) + info.BranchDisp));
                break;
            case UnconditionalJump:
                if (info.HasRipRelativeDisp) {
                    std::int32_t disp = 0;
                    std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                    VirtualAddress slotVaddr = static_cast<VirtualAddress>(
                        static_cast<std::int64_t>(nextVaddr) + disp
                    );
                    auto resolved = relativeRelocations.TargetOfSlot(slotVaddr);
                    if (resolved.has_value())
                        enqueue(*resolved);
                } else if (info.HasBranchTarget) {
                    enqueue(static_cast<VirtualAddress>(static_cast<std::int64_t>(nextVaddr) + info.BranchDisp));
                }
                break;
            case Call:
                if (info.HasRipRelativeDisp) {
                    std::int32_t disp = 0;
                    std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
                    VirtualAddress slotVaddr = static_cast<VirtualAddress>(
                        static_cast<std::int64_t>(nextVaddr) + disp
                    );
                    auto resolved = relativeRelocations.TargetOfSlot(slotVaddr);
                    if (resolved.has_value())
                        enqueue(*resolved);
                } else if (info.HasBranchTarget) {
                    enqueue(static_cast<VirtualAddress>(static_cast<std::int64_t>(nextVaddr) + info.BranchDisp));
                }
                enqueue(nextVaddr);
                break;
            case IndirectCall:
                enqueue(nextVaddr);
                break;
            case Return:
            case IndirectJump:
            case Trap:
                break;
        }
    }

    return std::make_unique<ControlFlowGraph>(std::move(reachable));
}

}
