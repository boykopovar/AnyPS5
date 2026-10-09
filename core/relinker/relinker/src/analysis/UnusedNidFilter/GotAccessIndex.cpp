#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <unordered_set>
#include <cstring>

namespace Relinker::UnusedNidFilter {

class GotAccessIndex : public IGotAccessIndex {
public:
    explicit GotAccessIndex(std::unordered_set<VirtualAddress> accessed)
        : _accessed(std::move(accessed)) {}

    bool IsGotSlotAccessed(VirtualAddress gotSlotVaddr) const override {
        return _accessed.count(gotSlotVaddr) > 0;
    }

private:
    std::unordered_set<VirtualAddress> _accessed;
};

std::unique_ptr<IGotAccessIndex> BuildGotAccessIndex(
    const IControlFlowGraph& cfg,
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr
) {
    const Codegen::X64InstructionDecoder decoder;
    std::unordered_set<VirtualAddress> accessed;

    for (VirtualAddress va : cfg.ReachableVaddrs()) {
        if (va < textVaddr || va >= textVaddr + static_cast<VirtualAddress>(text.size()))
            continue;
        std::size_t bufOff = static_cast<std::size_t>(va - textVaddr);
        std::size_t available = text.size() - bufOff;
        if (available == 0) continue;

        Codegen::DecodedInstructionInfo info = decoder.DecodeInstruction(text.data() + bufOff, available);
        if (!info.HasRipRelativeDisp) continue;

        VirtualAddress nextVaddr = va + static_cast<VirtualAddress>(info.Length);
        std::int32_t disp = 0;
        std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
        VirtualAddress target = static_cast<VirtualAddress>(
            static_cast<std::int64_t>(nextVaddr) + disp
        );
        accessed.insert(target);
    }

    return std::make_unique<GotAccessIndex>(std::move(accessed));
}

}
