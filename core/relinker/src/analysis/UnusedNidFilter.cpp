#include <relinker/analysis/UnusedNidFilter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace Relinker {

namespace {

using namespace Codegen::X64OpcodeConstants;

std::int32_t readDisp32(const std::vector<std::uint8_t>& text, std::size_t offset) {
    std::uint32_t v = 0;
    std::memcpy(&v, text.data() + offset, 4);
    return static_cast<std::int32_t>(v);
}

bool isRipRelativeGrp5(const std::vector<std::uint8_t>& text, const Codegen::InstructionMatch& m) {
    std::size_t cur = static_cast<std::size_t>(m.Offset);
    std::size_t end = cur + m.Length;
    while (cur < end) {
        std::uint8_t b = text[cur];
        if (b >= RexMin && b <= RexMax) { ++cur; continue; }
        if (b == OneByteGrp5Rm && cur + 2 <= end) {
            std::uint8_t modrm = text[cur + 1];
            std::uint8_t mod = (modrm >> ModRmModShift) & ModRmModMask;
            std::uint8_t reg = (modrm >> ModRmRegShift) & ModRmRegMask;
            std::uint8_t rm = modrm & ModRmRmMask;
            return mod == ModRmModIndirect && rm == ModRmRmRipRelative && (reg == 2 || reg == 4);
        }
        break;
    }
    return false;
}

std::size_t grp5DispOffset(const std::vector<std::uint8_t>& text, const Codegen::InstructionMatch& m) {
    std::size_t cur = static_cast<std::size_t>(m.Offset);
    while (text[cur] >= RexMin && text[cur] <= RexMax) ++cur;
    return cur + 2;
}

bool isRipRelativeMovR64(const std::vector<std::uint8_t>& text, const Codegen::InstructionMatch& m) {
    std::size_t cur = static_cast<std::size_t>(m.Offset);
    std::size_t end = cur + m.Length;
    bool hasRexW = false;
    while (cur < end) {
        std::uint8_t b = text[cur];
        if (b >= RexMin && b <= RexMax) { if (b & RexWBit) hasRexW = true; ++cur; continue; }
        if (hasRexW && b == OneByteModRmRangeJMax && cur + 2 <= end) {
            std::uint8_t modrm = text[cur + 1];
            std::uint8_t mod = (modrm >> ModRmModShift) & ModRmModMask;
            std::uint8_t rm = modrm & ModRmRmMask;
            return mod == ModRmModIndirect && rm == ModRmRmRipRelative;
        }
        break;
    }
    return false;
}

std::size_t movR64DispOffset(const std::vector<std::uint8_t>& text, const Codegen::InstructionMatch& m) {
    std::size_t cur = static_cast<std::size_t>(m.Offset);
    while (text[cur] >= RexMin && text[cur] <= RexMax) ++cur;
    return cur + 2;
}

}

class UnusedNidFilter : public IUnusedNidFilter {
public:
    std::vector<NidReference> Filter(const std::vector<NidReference>& nidRefs, const std::vector<std::uint8_t>& textSection, VirtualAddress textVAddr) override {
        if (textSection.empty()) return nidRefs;
        auto scanner = Codegen::MakeInstructionScanner();
        auto instructions = scanner->ScanCodeSection(textSection, textVAddr, textSection.size());

        std::unordered_map<VirtualAddress, VirtualAddress> ripTargetToInstrVAddr;
        std::unordered_multimap<VirtualAddress, VirtualAddress> callTargetToInstrVAddr;
        for (const auto& m : instructions) {
            std::size_t dispOff = 0;
            bool isRipInstr = false;
            if (isRipRelativeGrp5(textSection, m)) { dispOff = grp5DispOffset(textSection, m); isRipInstr = true; }
            else if (isRipRelativeMovR64(textSection, m)) { dispOff = movR64DispOffset(textSection, m); isRipInstr = true; }
            if (isRipInstr && dispOff + 4 <= textSection.size()) {
                std::int32_t disp = readDisp32(textSection, dispOff);
                VirtualAddress instrEnd = m.Offset + static_cast<VirtualAddress>(m.Length);
                VirtualAddress target = static_cast<VirtualAddress>(static_cast<std::int64_t>(instrEnd) + disp);
                ripTargetToInstrVAddr.emplace(target, m.Offset);
            }
            if (m.Length >= 5) {
                std::size_t off = static_cast<std::size_t>(m.Offset);
                if (textSection[off] == OneByteCallRel32 && off + 5 <= textSection.size()) {
                    std::int32_t disp = readDisp32(textSection, off + 1);
                    VirtualAddress instrEnd = m.Offset + static_cast<VirtualAddress>(m.Length);
                    VirtualAddress target = static_cast<VirtualAddress>(static_cast<std::int64_t>(instrEnd) + disp);
                    callTargetToInstrVAddr.emplace(target, m.Offset);
                }
            }
        }

        std::vector<NidReference> result;
        for (const auto& ref : nidRefs) {
            if (ref.RelocationTypeValue == 7) {
                result.push_back(ref);
            } else {
                if (ripTargetToInstrVAddr.count(ref.RelocationAddress) > 0)
                    result.push_back(ref);
            }
        }
        return result;
    }
};

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter() { return std::make_shared<UnusedNidFilter>(); }

}
