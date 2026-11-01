#include <codegen/x86/MulxLowering.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace Codegen {

namespace {

constexpr std::int32_t kFrameSize = 192;
constexpr std::uint8_t kStack = 4;

void _append32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint8_t _rex(const bool wide, const std::uint8_t reg, const std::uint8_t rm) {
    return static_cast<std::uint8_t>(0x40 | (wide ? 8 : 0) | ((reg & 8) >> 1) | ((rm & 8) >> 3));
}

void _stack(StubBodyBuilder& body, const std::uint8_t opcode, const bool wide, const std::uint8_t reg, const std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{_rex(wide, reg, kStack), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    _append32(bytes, static_cast<std::uint32_t>(displacement));
    body.Raw(bytes);
}

void _loadSource(StubBodyBuilder& body, const MulxOperands& operands) {
    if (!operands.Memory) {
        if (operands.Source == kStack) {
            _stack(body, 0x8D, operands.Wide, 0, kFrameSize);
        } else {
            const std::array bytes{_rex(operands.Wide, 0, operands.Source), std::uint8_t{0x8B}, static_cast<std::uint8_t>(0xC0 | (operands.Source & 7))};
            body.Raw(bytes);
        }
        return;
    }
    const auto& memory = *operands.Memory;
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrameSize;
    const bool address32 = std::find(memory.Prefixes.begin(), memory.Prefixes.end(), 0x67) != memory.Prefixes.end();
    if (!address32 && displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("MULX stack displacement does not fit after spilling");
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

}

std::optional<MulxOperands> DecodeMulx(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && (data[vex] == 0x2E || data[vex] == 0x36 || data[vex] == 0x3E || data[vex] == 0x26 || data[vex] == 0x64 || data[vex] == 0x65 || data[vex] == 0x67)) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF6 || (data[vex + 2] & 3) != 3)
        return std::nullopt;
    if ((data[vex + 2] & 4) != 0)
        throw CodegenException("MULX requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    MulxOperands operands{
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    if ((modrm >> 6) == 3) {
        if (vex + 5 != length) throw CodegenException("MULX register operand has trailing bytes");
    } else {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        if (vex + 4 + operands.Memory->EncodedSize != length)
            throw CodegenException("MULX memory operand has trailing bytes");
    }
    return operands;
}

void EmitMulx(StubBodyBuilder& body, const MulxOperands& operands) {
    if (operands.Memory && operands.Memory->StackBase && (operands.Memory->Sib & 0x38) == 0x20 && (operands.Memory->RexIndexBase & 2) == 0) {
        const auto displacement = static_cast<std::int64_t>(operands.Memory->Displacement);
        if (displacement < 24 - kFrameSize && displacement + (operands.Wide ? 8 : 4) > -kFrameSize)
            throw CodegenException("MULX source overlaps its spill frame");
    }
    _stack(body, 0x8D, true, kStack, 8 - kFrameSize);
    const std::array pushFlags{std::uint8_t{0x9C}};
    body.Raw(pushFlags);
    _stack(body, 0x89, true, 0, 8);
    _stack(body, 0x89, true, 2, 16);
    _loadSource(body, operands);
    const std::array multiply{_rex(operands.Wide, 0, 2), std::uint8_t{0xF7}, std::uint8_t{0xE2}};
    body.Raw(multiply);
    _stack(body, 0x89, true, 0, 24);
    _stack(body, 0x89, true, 2, 32);
    _stack(body, 0x8B, true, 0, 8);
    _stack(body, 0x8B, true, 2, 16);
    if (operands.LowDestination != kStack)
        _stack(body, 0x8B, operands.Wide, operands.LowDestination, 24);
    if (operands.HighDestination != kStack)
        _stack(body, 0x8B, operands.Wide, operands.HighDestination, 32);
    const std::array popFlags{std::uint8_t{0x9D}};
    body.Raw(popFlags);
    _stack(body, 0x8D, true, kStack, kFrameSize - 8);
    if (operands.HighDestination == kStack || operands.LowDestination == kStack)
        _stack(body, 0x8B, operands.Wide, kStack, (operands.HighDestination == kStack ? 32 : 24) - kFrameSize);
}

}
