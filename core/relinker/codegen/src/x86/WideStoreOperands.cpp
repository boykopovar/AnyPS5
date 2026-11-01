#include <codegen/x86/WideStoreOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <limits>

namespace Codegen {

namespace {

constexpr std::uint8_t kVexMap0F = 0x01;
constexpr std::uint8_t kVexLength256 = 0x04;
constexpr std::uint8_t kVexUnusedRegister = 0x0F;
constexpr std::uint8_t kPpNone = 0x00;
constexpr std::uint8_t kPp66 = 0x01;
constexpr std::uint8_t kPpF3 = 0x02;
constexpr std::uint8_t kVextractf128 = 0x19;
constexpr std::uint8_t kVextractf128Payload = 0x7D;
constexpr std::uint8_t kUpperHalf = 0x01;
constexpr std::int64_t kHalfBytes = 16;

bool _unalignedStore(const std::uint8_t pp, const std::uint8_t opcode) {
    return ((pp == kPpNone || pp == kPp66) && opcode == 0x11) || (pp == kPpF3 && opcode == 0x7F);
}

bool _legacyPrefixAllowed(const std::uint8_t prefix) {
    using namespace X64OpcodeConstants;
    return prefix == PrefixSegCs || prefix == PrefixSegSs || prefix == PrefixSegDs || prefix == PrefixSegEs || prefix == PrefixSegFs || prefix == PrefixSegGs || prefix == PrefixAddressSize;
}

void _appendU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (std::size_t index = 0; index < X64OpcodeConstants::Disp32Size; ++index)
        bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
}

}

std::optional<WideStoreOperands> DecodeVexWideStore(const std::uint8_t* data, const std::size_t length) {
    using namespace X64OpcodeConstants;
    std::size_t pos = 0;
    bool addressSize32 = false;
    while (pos < length && _legacyPrefixAllowed(data[pos])) {
        addressSize32 = addressSize32 || data[pos] == PrefixAddressSize;
        ++pos;
    }
    const auto vex = pos;
    if (vex + 2 > length)
        return std::nullopt;
    std::uint8_t inverted = 0;
    std::uint8_t payload = 0;
    if (data[vex] == OneByteVex2) {
        inverted = static_cast<std::uint8_t>((data[vex + 1] & 0x80) | 0x60);
        payload = data[vex + 1];
        pos = vex + Vex2PrefixLength;
    } else if (data[vex] == OneByteVex3) {
        if (vex + Vex3PrefixLength > length || (data[vex + 1] & Vex3MapMask) != kVexMap0F)
            return std::nullopt;
        inverted = static_cast<std::uint8_t>(data[vex + 1] & 0xE0);
        payload = data[vex + 2];
        pos = vex + Vex3PrefixLength;
    } else {
        return std::nullopt;
    }
    if ((payload & kVexLength256) == 0 || ((payload >> 3) & 0x0F) != kVexUnusedRegister)
        return std::nullopt;
    if (pos + 2 > length || !_unalignedStore(static_cast<std::uint8_t>(payload & 0x03), data[pos]))
        return std::nullopt;
    const auto modRmOffset = pos + 1;
    const auto modrm = data[modRmOffset];
    const auto mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    const auto rm = static_cast<std::uint8_t>(modrm & ModRmRmMask);
    if (mod == ModRmModRegister)
        return std::nullopt;
    auto cursor = modRmOffset + 1;
    std::optional<std::uint8_t> sib;
    if (rm == ModRmRmSibPresent) {
        if (cursor >= length)
            return std::nullopt;
        sib = data[cursor++];
    }
    const bool ripRelative = mod == ModRmModIndirect && rm == ModRmRmRipRelative;
    const bool absolute = mod == ModRmModIndirect && sib.has_value() && (*sib & SibBaseMask) == SibBaseDisp32;
    const bool absoluteNoIndex = absolute && (*sib & SibIndexMask) == SibIndexNone && (inverted & 0x40) != 0;
    const std::size_t displacementSize = mod == ModRmModDisp8 ? Disp8Size : (mod == ModRmModDisp32 || ripRelative || absolute) ? Disp32Size : 0;
    if (cursor + displacementSize != length)
        return std::nullopt;
    std::int64_t displacement = 0;
    if (displacementSize == Disp8Size) {
        displacement = static_cast<std::int8_t>(data[cursor]);
    } else if (displacementSize == Disp32Size) {
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < Disp32Size; ++index)
            value |= static_cast<std::uint32_t>(data[cursor + index]) << (index * 8);
        displacement = static_cast<std::int32_t>(value);
    }

    WideStoreOperands operands;
    operands.Low.Bytes.assign(data, data + length);
    operands.Low.Bytes[data[vex] == OneByteVex2 ? vex + 1 : vex + 2] &= static_cast<std::uint8_t>(~kVexLength256);
    if (ripRelative) {
        operands.Low.RipDisplacementOffset = cursor;
        operands.Low.RipTarget = static_cast<std::int64_t>(length) + displacement;
    }

    auto& high = operands.High.Bytes;
    high.assign(data, data + vex);
    high.insert(high.end(), {OneByteVex3, static_cast<std::uint8_t>(inverted | Vex3Map0F3A), kVextractf128Payload, kVextractf128});
    const auto reg = static_cast<std::uint8_t>(modrm & (ModRmRegMask << ModRmRegShift));
    std::int64_t highDisplacement = displacement + kHalfBytes;
    std::uint8_t highMod = mod;
    std::size_t highDisplacementSize = displacementSize;
    if (!ripRelative && !absolute) {
        if (mod == ModRmModIndirect || (mod == ModRmModDisp8 && highDisplacement > std::numeric_limits<std::int8_t>::max())) {
            highMod = highDisplacement > std::numeric_limits<std::int8_t>::max() ? ModRmModDisp32 : ModRmModDisp8;
            highDisplacementSize = highMod == ModRmModDisp8 ? Disp8Size : Disp32Size;
        }
    }
    const bool address32Absolute = addressSize32 && absoluteNoIndex;
    const auto unsignedHighDisplacement = static_cast<std::uint64_t>(static_cast<std::uint32_t>(displacement)) + kHalfBytes;
    if (!ripRelative) {
        if (address32Absolute) {
            if (unsignedHighDisplacement > std::numeric_limits<std::uint32_t>::max())
                throw CodegenException("256-bit store displacement leaves no room for its upper half");
        } else if (highDisplacement > std::numeric_limits<std::int32_t>::max()) {
            throw CodegenException("256-bit store displacement leaves no room for its upper half");
        }
    }
    high.push_back(static_cast<std::uint8_t>((highMod << ModRmModShift) | reg | rm));
    if (sib)
        high.push_back(*sib);
    const auto highDisplacementOffset = high.size();
    if (highDisplacementSize == Disp8Size)
        high.push_back(static_cast<std::uint8_t>(static_cast<std::int8_t>(highDisplacement)));
    else if (ripRelative)
        _appendU32(high, 0);
    else if (highDisplacementSize == Disp32Size)
        _appendU32(high, static_cast<std::uint32_t>(highDisplacement));
    high.push_back(kUpperHalf);
    if (ripRelative) {
        operands.High.RipDisplacementOffset = highDisplacementOffset;
        operands.High.RipTarget = static_cast<std::int64_t>(length) + displacement + kHalfBytes;
    }
    return operands;
}

}
