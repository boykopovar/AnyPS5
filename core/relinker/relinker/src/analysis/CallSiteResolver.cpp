#include <relinker/analysis/CallSiteResolver.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <algorithm>
#include <utility>

namespace Relinker {

namespace {
using namespace Codegen::X64OpcodeConstants;

std::int32_t readDisp32(const std::vector<std::uint8_t>& text, std::size_t offset) {
    std::uint32_t v = 0;
    std::memcpy(&v, text.data() + offset, 4);
    return static_cast<std::int32_t>(v);
}

std::optional<std::size_t> ripRelativeDispOffset(const std::vector<std::uint8_t>& text, const Codegen::InstructionMatch& m) {
    std::size_t cur = static_cast<std::size_t>(m.Offset);
    std::size_t end = cur + m.Length;
    bool hasRexW = false;
    while (cur < end) {
        std::uint8_t b = text[cur];
        if (b >= RexMin && b <= RexMax) {
            hasRexW = (b & RexWBit) != 0;
            ++cur;
            continue;
        }
        break;
    }
    if (cur + 2 > end) return std::nullopt;
    const std::uint8_t opcode = text[cur];
    if (opcode != OneByteGrp5Rm && (!hasRexW || opcode != OneByteModRmRangeJMax))
        return std::nullopt;
    const std::uint8_t modrm = text[cur + 1];
    const std::uint8_t mod = (modrm >> ModRmModShift) & ModRmModMask;
    const std::uint8_t rm = modrm & ModRmRmMask;
    if (mod != ModRmModIndirect || rm != ModRmRmRipRelative) return std::nullopt;
    const auto operation = (modrm >> ModRmRegShift) & ModRmRegMask;
    if (opcode == OneByteGrp5Rm && operation >= 2 && operation <= 6)
        return cur + 2;
    if (opcode == OneByteModRmRangeJMax && hasRexW)
        return cur + 2;
    return std::nullopt;
}
}

class CallSiteResolver : public ICallSiteResolver {
public:
    std::vector<FileByteOffset> ResolveCallSites(const std::vector<std::uint8_t>& textSection, FileByteOffset textSectionVAddr, VirtualAddress targetGotOrPltAddress, ByteCount targetGotOrPltSize, const std::vector<CodeRange>& unwindRanges) override;
private:
    const std::vector<std::uint8_t>* _cachedTextPtr = nullptr;
    std::vector<std::pair<VirtualAddress, FileByteOffset>> _cachedTargetToInstr;
    FileByteOffset _cachedTextVAddr = 0;
    std::vector<CodeRange> _cachedUnwindRanges;
};

std::vector<FileByteOffset> CallSiteResolver::ResolveCallSites(const std::vector<std::uint8_t>& textSection, const FileByteOffset textSectionVAddr, const VirtualAddress targetGotOrPltAddress, const ByteCount targetGotOrPltSize, const std::vector<CodeRange>& unwindRanges) {
    if (targetGotOrPltSize > std::numeric_limits<VirtualAddress>::max() - targetGotOrPltAddress)
        throw RelinkerException("Relocation target range exceeds the address space");
    if (textSection.size() > std::numeric_limits<FileByteOffset>::max() - textSectionVAddr)
        throw RelinkerException("Text section range exceeds the address space");
    if (textSection.empty()) return {};
    if (_cachedTextPtr != &textSection || _cachedTextVAddr != textSectionVAddr || _cachedUnwindRanges != unwindRanges) {
        auto scanner = Codegen::MakeInstructionScanner({}, unwindRanges);
        _cachedTargetToInstr.clear();
        const auto candidateCount = std::count_if(textSection.begin(), textSection.end(), [](std::uint8_t b) {
            return b == OneByteGrp5Rm || b == OneByteModRmRangeJMax;
        });
        constexpr std::size_t MinRipRelativeInstructionBytes = 6;
        _cachedTargetToInstr.reserve(std::min(static_cast<std::size_t>(candidateCount), textSection.size() / MinRipRelativeInstructionBytes));
        scanner->VisitCodeSection(textSection, 0, textSection.size(), [&](const Codegen::InstructionMatch& m) {
            const auto dispOff = ripRelativeDispOffset(textSection, m);
            if (!dispOff || *dispOff + 4 > textSection.size()) return;
            std::int32_t disp = readDisp32(textSection, *dispOff);
            VirtualAddress instrEnd = textSectionVAddr + m.Offset + static_cast<VirtualAddress>(m.Length);
            VirtualAddress target = instrEnd + static_cast<VirtualAddress>(static_cast<std::int64_t>(disp));
            _cachedTargetToInstr.emplace_back(target, m.Offset);
        });
        std::sort(_cachedTargetToInstr.begin(), _cachedTargetToInstr.end());
        _cachedTextPtr = &textSection;
        _cachedTextVAddr = textSectionVAddr;
        _cachedUnwindRanges = unwindRanges;
    }

    std::vector<FileByteOffset> sites;
    const auto findTarget = [&](VirtualAddress target) {
        return std::lower_bound(_cachedTargetToInstr.begin(), _cachedTargetToInstr.end(),
            std::pair<VirtualAddress, FileByteOffset>{target, 0});
    };
    auto lower = findTarget(targetGotOrPltAddress);
    auto upper = findTarget(targetGotOrPltAddress + targetGotOrPltSize);
    for (auto it = lower; it != upper; ++it)
        sites.push_back(textSectionVAddr + it->second);
    return sites;
}

std::shared_ptr<ICallSiteResolver> MakeCallSiteResolver() { return std::make_shared<CallSiteResolver>(); }

}
