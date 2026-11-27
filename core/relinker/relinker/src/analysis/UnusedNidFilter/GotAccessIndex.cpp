#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <unordered_set>
#include <cstring>
#include <limits>

namespace Relinker::UnusedNidFilter {

class GotAccessIndex : public IGotAccessIndex {
public:
    GotAccessIndex(std::unordered_set<VirtualAddress> accessed, bool unknownSegmentAccess, bool tlsAccessed, std::vector<TlsImageRange> tlsImages)
        : _accessed(std::move(accessed)), unknownSegmentAccess(unknownSegmentAccess), tlsAccessed(tlsAccessed), tlsImages(std::move(tlsImages)) {}

    bool IsGotSlotAccessed(VirtualAddress gotSlotVaddr) const override {
        if (unknownSegmentAccess || _accessed.count(gotSlotVaddr) > 0) return true;
        if (!tlsAccessed) return false;
        for (const auto& image : tlsImages)
            if (gotSlotVaddr >= image.Begin && gotSlotVaddr < image.End) return true;
        return false;
    }

private:
    std::unordered_set<VirtualAddress> _accessed;
    bool unknownSegmentAccess;
    bool tlsAccessed;
    std::vector<TlsImageRange> tlsImages;
};

std::unique_ptr<IGotAccessIndex> BuildGotAccessIndex(
    const IControlFlowGraph& cfg,
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    const std::vector<TlsImageRange>& tlsImages,
    bool tlsModeled
) {
    if (text.size() > std::numeric_limits<VirtualAddress>::max() - textVaddr) {
        throw RelinkerException("Text section range exceeds the address space");
    }
    const Codegen::X64InstructionDecoder decoder;
    std::unordered_set<VirtualAddress> accessed;
    bool unknownSegmentAccess = false;
    bool tlsAccessed = false;

    for (VirtualAddress va : cfg.ReachableVaddrs()) {
        if (va < textVaddr || va - textVaddr >= text.size())
            continue;
        std::size_t bufOff = static_cast<std::size_t>(va - textVaddr);
        std::size_t available = text.size() - bufOff;
        if (available == 0) continue;

        Codegen::DecodedInstructionInfo info = decoder.DecodeInstruction(text.data() + bufOff, available);
        if (info.SegmentPrefix != 0 && info.HasMemoryOperand) {
            if (info.SegmentPrefix == Codegen::X64OpcodeConstants::PrefixSegFs && tlsModeled) tlsAccessed = true;
            else unknownSegmentAccess = true;
            continue;
        }
        if (!info.HasRipRelativeDisp) continue;

        VirtualAddress nextVaddr = va + static_cast<VirtualAddress>(info.Length);
        std::int32_t disp = 0;
        std::memcpy(&disp, text.data() + bufOff + info.RipRelativeDispOffset, 4);
        if (!info.AddressSize32) {
            if (disp < 0 && nextVaddr < static_cast<VirtualAddress>(-static_cast<std::int64_t>(disp))) continue;
            if (disp > 0 && std::numeric_limits<VirtualAddress>::max() - nextVaddr < static_cast<VirtualAddress>(disp)) continue;
        }
        VirtualAddress target = nextVaddr + static_cast<VirtualAddress>(static_cast<std::int64_t>(disp));
        if (info.AddressSize32) target = static_cast<std::uint32_t>(target);
        accessed.insert(target);
    }

    return std::make_unique<GotAccessIndex>(std::move(accessed), unknownSegmentAccess, tlsAccessed, tlsImages);
}

}
