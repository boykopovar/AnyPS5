#include <codegen/x86/AndnLowering.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace Codegen {

namespace {

constexpr std::int32_t kFrameSize = 144;
constexpr std::uint8_t kStack = 4;

bool _prefix(std::uint8_t value) {
    return value == 0x2E || value == 0x36 || value == 0x3E || value == 0x26 || value == 0x64 || value == 0x65 || value == 0x67;
}

void _append32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
}

std::uint8_t _rex(bool wide, std::uint8_t reg, std::uint8_t rm) {
    return static_cast<std::uint8_t>(0x40 | (wide ? 8 : 0) | ((reg & 8) >> 1) | ((rm & 8) >> 3));
}

void _register(StubBodyBuilder& body, std::uint8_t opcode, bool wide, std::uint8_t reg, std::uint8_t rm) {
    const std::array bytes{_rex(wide, reg, rm), opcode, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))};
    body.Raw(bytes);
}

void _stack(StubBodyBuilder& body, std::uint8_t opcode, bool wide, std::uint8_t reg, std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{_rex(wide, reg, kStack), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    _append32(bytes, displacement);
    body.Raw(bytes);
}

std::uint8_t _scratch(const AndnOperands& operands) {
    std::array<bool, 16> used{};
    used[kStack] = used[operands.Destination] = used[operands.InvertedSource] = true;
    if (!operands.Memory) {
        used[operands.Source] = true;
    } else {
        const auto& memory = *operands.Memory;
        if (memory.Rm == 4) {
            const auto base = static_cast<std::uint8_t>((memory.Sib & 7) | ((memory.RexIndexBase & 1) << 3));
            const auto index = static_cast<std::uint8_t>(((memory.Sib >> 3) & 7) | ((memory.RexIndexBase & 2) << 2));
            if (memory.Mod != 0 || (memory.Sib & 7) != 5) used[base] = true;
            if (index != kStack) used[index] = true;
        } else if (!memory.RipRelative) {
            used[memory.Rm | ((memory.RexIndexBase & 1) << 3)] = true;
        }
    }
    for (const auto reg : {11, 10, 9, 8, 0, 1, 2, 3, 5, 6, 7, 12, 13, 14, 15})
        if (!used[reg]) return static_cast<std::uint8_t>(reg);
    throw CodegenException("ANDN has no scratch register");
}

void _memory(StubBodyBuilder& body, const AndnOperands& operands, std::uint8_t scratch) {
    const auto& memory = *operands.Memory;
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrameSize;
    const bool address32 = std::find(memory.Prefixes.begin(), memory.Prefixes.end(), 0x67) != memory.Prefixes.end();
    if (!address32 && displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("ANDN stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(_rex(operands.Wide, scratch, 0) | memory.RexIndexBase));
    bytes.push_back(0x23);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((scratch & 7) << 3) | memory.Rm));
    if (memory.Rm == 4) bytes.push_back(memory.Sib);
    const auto offset = bytes.size();
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || (mod == 0 && (memory.RipRelative || (memory.Rm == 4 && (memory.Sib & 7) == 5))))
        _append32(bytes, static_cast<std::int32_t>(displacement));
    if (memory.RipRelative)
        body.RipRelative(bytes, offset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(bytes);
}

}

std::optional<AndnOperands> DecodeAndn(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && _prefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF2)
        return std::nullopt;
    if ((data[vex + 2] & 7) != 0)
        throw CodegenException("ANDN requires VEX.L=0 and VEX.pp=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    AndnOperands operands{
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    if ((modrm >> 6) == 3) {
        if (vex + 5 != length) throw CodegenException("ANDN register operand has trailing bytes");
    } else {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        if (vex + 4 + operands.Memory->EncodedSize != length)
            throw CodegenException("ANDN memory operand has trailing bytes");
    }
    return operands;
}

void EmitAndn(StubBodyBuilder& body, const AndnOperands& operands) {
    const auto scratch = _scratch(operands);
    _stack(body, 0x8D, true, kStack, -kFrameSize);
    _stack(body, 0x89, true, scratch, 0);
    if (!operands.Memory && operands.Source == kStack) {
        _stack(body, 0x8D, true, scratch, kFrameSize);
        _stack(body, 0x89, true, scratch, 8);
    }
    if (operands.InvertedSource == kStack)
        _stack(body, 0x8D, true, scratch, kFrameSize);
    else
        _register(body, 0x8B, operands.Wide, scratch, operands.InvertedSource);
    _register(body, 0xF7, operands.Wide, 2, scratch);
    if (operands.Memory)
        _memory(body, operands, scratch);
    else if (operands.Source == kStack)
        _stack(body, 0x23, operands.Wide, scratch, 8);
    else
        _register(body, 0x23, operands.Wide, scratch, operands.Source);
    if (operands.Destination == kStack)
        _stack(body, 0x89, operands.Wide, scratch, 8);
    else
        _register(body, 0x8B, operands.Wide, operands.Destination, scratch);
    _stack(body, 0x8B, true, scratch, 0);
    _stack(body, 0x8D, true, kStack, kFrameSize);
    if (operands.Destination == kStack)
        _stack(body, 0x8B, operands.Wide, kStack, 8 - kFrameSize);
}

}
