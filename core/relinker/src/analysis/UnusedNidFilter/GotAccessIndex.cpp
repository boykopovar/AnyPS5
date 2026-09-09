#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <unordered_set>
#include <cstring>

namespace Relinker::UnusedNidFilter {

namespace {

constexpr std::uint8_t OneByteMovLoadRm8 = 0x8A;
constexpr std::uint8_t OneByteMovLoadRm = 0x8B;
constexpr std::uint8_t Grp5RegCallIndirect = 2;
constexpr std::uint8_t Grp5RegJmpIndirect = 4;

bool ReadsMemoryOperandAsPointer(const Codegen::DecodedInstructionInfo& info) {
    if (info.IsTwoByteOpcode) return false;
    if (info.Opcode == OneByteMovLoadRm8 || info.Opcode == OneByteMovLoadRm) return true;
    if (info.Opcode == Codegen::X64OpcodeConstants::OneByteGrp5Rm)
        return info.ModRmRegField == Grp5RegCallIndirect || info.ModRmRegField == Grp5RegJmpIndirect;
    return false;
}

}

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
        if (!ReadsMemoryOperandAsPointer(info)) continue;

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
