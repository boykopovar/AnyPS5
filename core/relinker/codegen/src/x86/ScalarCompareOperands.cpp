#include <codegen/x86/ScalarCompareOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>

namespace Codegen {

namespace {

constexpr std::uint8_t kVexMap0F = 0x01;
constexpr std::uint8_t kPpF3 = 0x02;
constexpr std::uint8_t kCompareOpcode = 0xC2;
constexpr std::uint8_t kPredicateLeOs = 0x02;

bool _segmentPrefix(const std::uint8_t prefix) {
    using namespace X64OpcodeConstants;
    return prefix == PrefixSegCs || prefix == PrefixSegSs || prefix == PrefixSegDs || prefix == PrefixSegEs || prefix == PrefixSegFs || prefix == PrefixSegGs;
}

}

std::optional<ScalarCompareOperands> DecodeScalarCompare(const std::uint8_t* data, const std::size_t length) {
    using namespace X64OpcodeConstants;
    std::size_t pos = 0;
    while (pos < length && _segmentPrefix(data[pos]))
        ++pos;
    std::size_t modRmOffset = 0;
    if (pos < length && data[pos] == OneByteVex2) {
        if (pos + 4 > length || (data[pos + 1] & 0x03) < kPpF3 || data[pos + 2] != kCompareOpcode)
            return std::nullopt;
        modRmOffset = pos + 3;
    } else if (pos < length && data[pos] == OneByteVex3) {
        if (pos + 5 > length || (data[pos + 1] & Vex3MapMask) != kVexMap0F || (data[pos + 2] & 0x03) < kPpF3 || data[pos + 3] != kCompareOpcode)
            return std::nullopt;
        modRmOffset = pos + 4;
    } else {
        bool singlePrecision = false;
        bool doublePrecision = false;
        for (; pos < length; ++pos) {
            if (data[pos] == PrefixRep)
                singlePrecision = true;
            else if (data[pos] == PrefixRepne)
                doublePrecision = true;
            else if (!_segmentPrefix(data[pos]))
                break;
        }
        if (singlePrecision == doublePrecision)
            return std::nullopt;
        if (pos < length && data[pos] >= RexMin && data[pos] <= RexMax)
            ++pos;
        if (pos + 3 > length || data[pos] != TwoByteOpcodeEscape || data[pos + 1] != kCompareOpcode)
            return std::nullopt;
        modRmOffset = pos + 2;
    }
    const auto modrm = data[modRmOffset];
    const auto mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    const auto rm = static_cast<std::uint8_t>(modrm & ModRmRmMask);
    auto cursor = modRmOffset + 1;
    std::optional<std::uint8_t> sib;
    if (mod != ModRmModRegister && rm == ModRmRmSibPresent) {
        if (cursor >= length)
            return std::nullopt;
        sib = data[cursor++];
    }
    const bool ripRelative = mod == ModRmModIndirect && rm == ModRmRmRipRelative;
    const bool absolute = mod == ModRmModIndirect && sib.has_value() && (*sib & SibBaseMask) == SibBaseDisp32;
    const std::size_t displacementSize = mod == ModRmModDisp8 ? Disp8Size : (mod == ModRmModDisp32 || ripRelative || absolute) ? Disp32Size : 0;
    if (cursor + displacementSize + ImmSize8 != length || data[length - 1] <= kPredicateLeOs)
        return std::nullopt;
    ScalarCompareOperands operands;
    if (ripRelative) {
        std::uint32_t displacement = 0;
        for (std::size_t index = 0; index < Disp32Size; ++index)
            displacement |= static_cast<std::uint32_t>(data[cursor + index]) << (index * 8);
        operands.RipDisplacementOffset = cursor;
        operands.RipTarget = static_cast<std::int64_t>(length) + static_cast<std::int32_t>(displacement);
    }
    return operands;
}

}
