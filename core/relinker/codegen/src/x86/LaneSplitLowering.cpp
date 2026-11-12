#include <codegen/x86/LaneSplitLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::uint8_t kMap0F = 1;
constexpr std::uint8_t kMap0F38 = 2;
constexpr std::uint8_t kMap0F3A = 3;
constexpr std::uint8_t kPrefix66 = 1;
constexpr std::uint8_t kPrefixF3 = 2;
constexpr std::uint8_t kPrefixF2 = 3;
constexpr std::int32_t kSaveSize = 96;
constexpr std::int32_t kFrame = 128 + kSaveSize;

bool _in(const std::uint8_t value, const std::uint8_t low, const std::uint8_t high) {
    return value >= low && value <= high;
}

std::optional<LaneSplitShape> _shape(const std::uint8_t map, const std::uint8_t prefix, const std::uint8_t opcode, const std::uint8_t extension) {
    if (map == kMap0F && opcode == 0x70 && (prefix == kPrefix66 || prefix == kPrefixF3 || prefix == kPrefixF2))
        return LaneSplitShape::UnaryImmediate;
    if (prefix != kPrefix66) return std::nullopt;
    if (map == kMap0F) {
        if (_in(opcode, 0x60, 0x6D) || _in(opcode, 0x74, 0x76) || opcode == 0xD4 || opcode == 0xD5 || _in(opcode, 0xD8, 0xE0) || _in(opcode, 0xE3, 0xE5) || _in(opcode, 0xE8, 0xEF) || _in(opcode, 0xF4, 0xF6) || _in(opcode, 0xF8, 0xFE))
            return LaneSplitShape::Binary;
        if (_in(opcode, 0xD1, 0xD3) || opcode == 0xE1 || opcode == 0xE2 || _in(opcode, 0xF1, 0xF3))
            return LaneSplitShape::ShiftCount;
        if ((opcode == 0x71 || opcode == 0x72) && (extension == 2 || extension == 4 || extension == 6))
            return LaneSplitShape::ShiftImmediate;
        if (opcode == 0x73 && (extension == 2 || extension == 3 || extension == 6 || extension == 7))
            return LaneSplitShape::ShiftImmediate;
        return std::nullopt;
    }
    if (map == kMap0F38) {
        if (_in(opcode, 0x00, 0x0B) || opcode == 0x28 || opcode == 0x29 || opcode == 0x2B || _in(opcode, 0x37, 0x40))
            return LaneSplitShape::Binary;
        if (_in(opcode, 0x1C, 0x1E))
            return LaneSplitShape::Unary;
        return std::nullopt;
    }
    if (map == kMap0F3A && (opcode == 0x0E || opcode == 0x0F))
        return LaneSplitShape::BinaryImmediate;
    return std::nullopt;
}

bool _hasImmediate(const LaneSplitShape shape) {
    return shape == LaneSplitShape::BinaryImmediate || shape == LaneSplitShape::ShiftImmediate || shape == LaneSplitShape::UnaryImmediate;
}

std::vector<std::uint8_t> _vexHeader(const std::uint8_t map, const bool wide, const std::uint8_t prefix, const bool vector256, const std::uint8_t reg, const std::uint8_t vvvv, const bool indexExtended, const bool baseExtended) {
    return {0xC4,
        static_cast<std::uint8_t>(((reg & 8) != 0 ? 0 : 0x80) | (indexExtended ? 0 : 0x40) | (baseExtended ? 0 : 0x20) | map),
        static_cast<std::uint8_t>((wide ? 0x80 : 0) | ((~vvvv & 15) << 3) | (vector256 ? 4 : 0) | prefix)};
}

void _vexRegister(StubBodyBuilder& body, const std::uint8_t map, const bool wide, const std::uint8_t prefix, const bool vector256, const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint8_t rm, const std::optional<std::uint8_t> immediate) {
    auto bytes = _vexHeader(map, wide, prefix, vector256, reg, vvvv, false, (rm & 8) != 0);
    bytes.push_back(opcode);
    bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7)));
    if (immediate) bytes.push_back(*immediate);
    body.Raw(bytes);
}

void _vexStack(StubBodyBuilder& body, const std::uint8_t opcode, const bool vector256, const std::uint8_t reg, const std::int32_t displacement) {
    auto bytes = _vexHeader(kMap0F, false, kPrefixF3, vector256, reg, 0, false, false);
    bytes.insert(bytes.end(), {opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
    Append32(bytes, displacement);
    body.Raw(bytes);
}

void _vexLoad(StubBodyBuilder& body, const bool vector256, const std::uint8_t reg, const MemoryOperand& memory) {
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrame;
    if (displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("256-bit integer lowering stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    const auto header = _vexHeader(kMap0F, false, kPrefixF3, vector256, reg, 0, (memory.RexIndexBase & 2) != 0, (memory.RexIndexBase & 1) != 0);
    bytes.insert(bytes.end(), header.begin(), header.end());
    bytes.push_back(0x6F);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((reg & 7) << 3) | memory.Rm));
    if (memory.Rm == 4) bytes.push_back(memory.Sib);
    const auto offset = bytes.size();
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || (mod == 0 && (memory.RipRelative || (memory.Rm == 4 && (memory.Sib & 7) == 5))))
        Append32(bytes, static_cast<std::int32_t>(displacement));
    if (memory.RipRelative)
        body.RipRelative(bytes, offset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(bytes);
}

void _extractHigh(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source) {
    _vexRegister(body, kMap0F3A, false, kPrefix66, true, 0x19, source, 0, destination, std::uint8_t{1});
}

void _insertHigh(StubBodyBuilder& body, const std::uint8_t destination, const std::uint8_t source) {
    _vexRegister(body, kMap0F3A, false, kPrefix66, true, 0x18, destination, destination, source, std::uint8_t{1});
}

void _half(StubBodyBuilder& body, const LaneSplitOperands& operands, const std::uint8_t destination, const std::uint8_t first, const std::uint8_t second) {
    switch (operands.Shape) {
    case LaneSplitShape::Binary:
    case LaneSplitShape::BinaryImmediate:
    case LaneSplitShape::ShiftCount:
        _vexRegister(body, operands.Map, operands.Wide, operands.Prefix, false, operands.Opcode, destination, first, second, operands.Immediate);
        return;
    case LaneSplitShape::ShiftImmediate:
        _vexRegister(body, operands.Map, operands.Wide, operands.Prefix, false, operands.Opcode, operands.Extension, destination, first, operands.Immediate);
        return;
    case LaneSplitShape::Unary:
    case LaneSplitShape::UnaryImmediate:
        _vexRegister(body, operands.Map, operands.Wide, operands.Prefix, false, operands.Opcode, destination, 0, second, operands.Immediate);
        return;
    }
}

}

std::optional<LaneSplitOperands> DecodeLaneSplit(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    std::vector<std::uint8_t> prefixes;
    while (vex < length && LegacyPrefix(data[vex])) prefixes.push_back(data[vex++]);
    if (vex + 4 > length || (data[vex] != 0xC4 && data[vex] != 0xC5)) return std::nullopt;
    const bool threeByte = data[vex] == 0xC4;
    if (threeByte && vex + 5 > length) return std::nullopt;
    const auto payload = threeByte ? data[vex + 2] : data[vex + 1];
    const auto extensions = threeByte ? static_cast<std::uint8_t>(~data[vex + 1]) : static_cast<std::uint8_t>((~data[vex + 1] & 0x80) | 0);
    const auto map = threeByte ? static_cast<std::uint8_t>(data[vex + 1] & 0x1F) : kMap0F;
    if ((payload & 4) == 0) return std::nullopt;
    const auto opcodeOffset = vex + (threeByte ? 3 : 2);
    const auto opcode = data[opcodeOffset];
    const auto modrm = data[opcodeOffset + 1];
    const auto extension = static_cast<std::uint8_t>((modrm >> 3) & 7);
    const auto shape = _shape(map, static_cast<std::uint8_t>(payload & 3), opcode, extension);
    if (!shape) return std::nullopt;
    LaneSplitOperands operands{
        *shape, map, static_cast<std::uint8_t>(payload & 3), threeByte && (payload & 0x80) != 0, opcode,
        static_cast<std::uint8_t>(extension | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~payload >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        extension, std::nullopt, std::nullopt};
    std::size_t end = opcodeOffset + 2;
    if ((modrm >> 6) != 3) {
        if (*shape == LaneSplitShape::ShiftImmediate) throw CodegenException("256-bit shift by immediate requires a register source");
        operands.Memory = DecodeMemoryOperand(data, length, opcodeOffset + 1, static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5)), prefixes);
        end = opcodeOffset + 1 + operands.Memory->EncodedSize;
    }
    if (*shape == LaneSplitShape::ShiftImmediate) {
        operands.Destination = operands.First;
        operands.First = operands.Second;
    }
    if (_hasImmediate(*shape)) {
        if (end >= length) throw CodegenException("256-bit integer instruction is missing its immediate");
        operands.Immediate = data[end++];
    }
    if (end != length) throw CodegenException("256-bit integer instruction has trailing bytes");
    return operands;
}

void EmitLaneSplit(StubBodyBuilder& body, const LaneSplitOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    if (operands.Shape != LaneSplitShape::Unary && operands.Shape != LaneSplitShape::UnaryImmediate) used[operands.First] = true;
    if (!operands.Memory && operands.Shape != LaneSplitShape::ShiftImmediate) used[operands.Second] = true;
    std::array<std::uint8_t, 3> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < scratch.size(); --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const auto high = scratch[0];
    const auto secondHigh = scratch[1];
    const auto loaded = scratch[2];
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        _vexStack(body, 0x7F, true, scratch[index], static_cast<std::int32_t>(index * 32));
    auto second = operands.Second;
    if (operands.Memory) {
        _vexLoad(body, operands.Shape != LaneSplitShape::ShiftCount, loaded, *operands.Memory);
        second = loaded;
    }
    switch (operands.Shape) {
    case LaneSplitShape::Binary:
    case LaneSplitShape::BinaryImmediate:
        _extractHigh(body, high, operands.First);
        _extractHigh(body, secondHigh, second);
        _half(body, operands, high, high, secondHigh);
        break;
    case LaneSplitShape::ShiftCount:
        _extractHigh(body, high, operands.First);
        _half(body, operands, high, high, second);
        break;
    case LaneSplitShape::ShiftImmediate:
        _extractHigh(body, high, operands.First);
        _half(body, operands, high, high, high);
        break;
    case LaneSplitShape::Unary:
    case LaneSplitShape::UnaryImmediate:
        _extractHigh(body, high, second);
        _half(body, operands, high, 0, high);
        break;
    }
    _half(body, operands, operands.Destination, operands.First, second);
    _insertHigh(body, operands.Destination, high);
    for (std::size_t index = 0; index < scratch.size(); ++index)
        _vexStack(body, 0x6F, true, scratch[index], static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
