#include <codegen/x86/BitPermutationLowering.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace Codegen {

namespace {

constexpr std::int32_t kFrameSize = 208;
constexpr std::uint8_t kStack = 4;

void _append32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint8_t _rex(bool wide, std::uint8_t reg, std::uint8_t rm) {
    return static_cast<std::uint8_t>(0x40 | (wide ? 8 : 0) | ((reg & 8) >> 1) | ((rm & 8) >> 3));
}

void _stack(StubBodyBuilder& body, std::uint8_t opcode, bool wide, std::uint8_t reg, std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{_rex(wide, reg, kStack), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    _append32(bytes, static_cast<std::uint32_t>(displacement));
    body.Raw(bytes);
}

void _memory(StubBodyBuilder& body, const BitPermutationOperands& operands) {
    const auto& memory = *operands.Memory;
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrameSize;
    const bool address32 = std::find(memory.Prefixes.begin(), memory.Prefixes.end(), 0x67) != memory.Prefixes.end();
    if (!address32 && displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("PEXT/PDEP stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(_rex(operands.Wide, 0, 0) | memory.RexIndexBase));
    bytes.push_back(0x8B);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | memory.Rm));
    if (memory.Rm == 4) bytes.push_back(memory.Sib);
    const auto offset = bytes.size();
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || (mod == 0 && (memory.RipRelative || (memory.Rm == 4 && (memory.Sib & 7) == 5))))
        _append32(bytes, static_cast<std::uint32_t>(displacement));
    if (memory.RipRelative)
        body.RipRelative(bytes, offset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(bytes);
}

void _loop(StubBodyBuilder& body, bool deposit) {
    std::vector<std::uint8_t> bytes{0x31, 0xC9, 0x41, 0xB8, 1, 0, 0, 0};
    const auto loop = bytes.size();
    bytes.insert(bytes.end(), {0x48, 0x85, 0xD2, 0x74, 0});
    const auto doneBranch = bytes.size() - 1;
    bytes.insert(bytes.end(), {0x49, 0x89, 0xD1, 0x49, 0xF7, 0xD9, 0x49, 0x21, 0xD1});
    if (deposit)
        bytes.insert(bytes.end(), {0x4C, 0x85, 0xC0});
    else
        bytes.insert(bytes.end(), {0x4C, 0x85, 0xC8});
    bytes.insert(bytes.end(), {0x74, 0});
    const auto skipBranch = bytes.size() - 1;
    if (deposit)
        bytes.insert(bytes.end(), {0x4C, 0x09, 0xC9});
    else
        bytes.insert(bytes.end(), {0x4C, 0x09, 0xC1});
    bytes[skipBranch] = static_cast<std::uint8_t>(bytes.size() - skipBranch - 1);
    bytes.insert(bytes.end(), {0x49, 0xD1, 0xE0, 0x4C, 0x8D, 0x4A, 0xFF, 0x4C, 0x21, 0xCA, 0xEB, 0});
    bytes.back() = static_cast<std::uint8_t>(loop - bytes.size());
    bytes[doneBranch] = static_cast<std::uint8_t>(bytes.size() - doneBranch - 1);
    body.Raw(bytes);
}

}

std::optional<BitPermutationOperands> DecodeBitPermutation(const std::uint8_t* data, std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && (data[vex] == 0x2E || data[vex] == 0x36 || data[vex] == 0x3E || data[vex] == 0x26 || data[vex] == 0x64 || data[vex] == 0x65 || data[vex] == 0x67)) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF5 || (data[vex + 2] & 3) < 2)
        return std::nullopt;
    if ((data[vex + 2] & 4) != 0)
        throw CodegenException("PEXT/PDEP requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    BitPermutationOperands operands{
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        (data[vex + 2] & 0x80) != 0, (data[vex + 2] & 3) == 3, std::nullopt};
    if ((modrm >> 6) == 3) {
        if (vex + 5 != length) throw CodegenException("PEXT/PDEP register operand has trailing bytes");
    } else {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        if (vex + 4 + operands.Memory->EncodedSize != length)
            throw CodegenException("PEXT/PDEP memory operand has trailing bytes");
    }
    return operands;
}

bool CanLowerBitPermutation(const BitPermutationOperands& operands) {
    if (!operands.Memory) return true;
    const auto& memory = *operands.Memory;
    if (!memory.StackBase || (memory.Sib & 0x38) != 0x20 || (memory.RexIndexBase & 2) != 0
        || std::any_of(memory.Prefixes.begin(), memory.Prefixes.end(), [](const auto prefix) { return prefix == 0x64 || prefix == 0x65 || prefix == 0x67; }))
        return false;
    const auto displacement = static_cast<std::int64_t>(memory.Displacement);
    return displacement + kFrameSize <= std::numeric_limits<std::int32_t>::max()
        && !(displacement < 72 - kFrameSize && displacement + (operands.Wide ? 8 : 4) > -kFrameSize);
}

void EmitBitPermutation(StubBodyBuilder& body, const BitPermutationOperands& operands) {
    if (operands.Memory) {
        const auto& memory = *operands.Memory;
        if (!memory.StackBase || (memory.Sib & 0x38) != 0x20 || (memory.RexIndexBase & 2) != 0
            || std::any_of(memory.Prefixes.begin(), memory.Prefixes.end(), [](const auto prefix) { return prefix == 0x64 || prefix == 0x65 || prefix == 0x67; }))
            throw CodegenException("PEXT/PDEP memory mask requires unsegmented 64-bit RSP addressing without an index");
    }
    if (operands.Memory && operands.Memory->StackBase && (operands.Memory->Sib & 0x38) == 0x20 && (operands.Memory->RexIndexBase & 2) == 0) {
        const auto displacement = static_cast<std::int64_t>(operands.Memory->Displacement);
        if (displacement < 72 - kFrameSize && displacement + (operands.Wide ? 8 : 4) > -kFrameSize)
            throw CodegenException("PEXT/PDEP source overlaps its spill frame");
    }
    _stack(body, 0x8D, true, kStack, 8 - kFrameSize);
    const std::array pushFlags{std::uint8_t{0x9C}};
    body.Raw(pushFlags);
    _stack(body, 0x89, true, 0, 8);
    _stack(body, 0x89, true, 1, 16);
    _stack(body, 0x89, true, 2, 24);
    _stack(body, 0x89, true, 8, 32);
    _stack(body, 0x89, true, 9, 40);
    if (operands.Source != kStack) _stack(body, 0x89, operands.Wide, operands.Source, 48);
    if (!operands.Memory && operands.Mask != kStack) _stack(body, 0x89, operands.Wide, operands.Mask, 56);
    if (operands.Memory) {
        _memory(body, operands);
        _stack(body, 0x89, operands.Wide, 0, 56);
    }
    if (operands.Source == kStack || (!operands.Memory && operands.Mask == kStack)) {
        _stack(body, 0x8D, true, 0, kFrameSize);
        if (operands.Source == kStack) _stack(body, 0x89, operands.Wide, 0, 48);
        if (!operands.Memory && operands.Mask == kStack) _stack(body, 0x89, operands.Wide, 0, 56);
    }
    _stack(body, 0x8B, operands.Wide, 0, 48);
    _stack(body, 0x8B, operands.Wide, 2, 56);
    _loop(body, operands.Deposit);
    _stack(body, 0x89, operands.Wide, 1, 64);
    _stack(body, 0x8B, true, 0, 8);
    _stack(body, 0x8B, true, 1, 16);
    _stack(body, 0x8B, true, 2, 24);
    _stack(body, 0x8B, true, 8, 32);
    _stack(body, 0x8B, true, 9, 40);
    const std::array popFlags{std::uint8_t{0x9D}};
    body.Raw(popFlags);
    if (operands.Destination != kStack) _stack(body, 0x8B, operands.Wide, operands.Destination, 56);
    _stack(body, 0x8D, true, kStack, kFrameSize - 8);
    if (operands.Destination == kStack) _stack(body, 0x8B, operands.Wide, kStack, 64 - kFrameSize);
}

}
