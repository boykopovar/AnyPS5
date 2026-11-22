#include <codegen/x86/LaneSplitLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::int32_t kRedZone = 128;
constexpr std::int32_t kVectorSize = 32;
constexpr std::int32_t kMaskSlot = kVectorSize;
constexpr std::int32_t kWorkSlot = kMaskSlot + 8;
constexpr std::int32_t kMaskFrame = kRedZone + kWorkSlot + 8;
constexpr Form kMoveMask128{Map0F, Prefix66, false, false, 0xD7};
constexpr const char* kName = "256-bit integer lowering";

bool _in(const std::uint8_t value, const std::uint8_t low, const std::uint8_t high) {
    return value >= low && value <= high;
}

std::optional<LaneSplitShape> _shape(const std::uint8_t map, const std::uint8_t prefix, const std::uint8_t opcode, const std::uint8_t extension) {
    if (map == Map0F && opcode == 0x70 && (prefix == Prefix66 || prefix == PrefixF3 || prefix == PrefixF2))
        return LaneSplitShape::UnaryImmediate;
    if (prefix != Prefix66) return std::nullopt;
    if (map == Map0F) {
        if (_in(opcode, 0x60, 0x6D) || _in(opcode, 0x74, 0x76) || opcode == 0xD4 || opcode == 0xD5 || _in(opcode, 0xD8, 0xE0) || _in(opcode, 0xE3, 0xE5) || _in(opcode, 0xE8, 0xEF) || _in(opcode, 0xF4, 0xF6) || _in(opcode, 0xF8, 0xFE))
            return LaneSplitShape::Binary;
        if (_in(opcode, 0xD1, 0xD3) || opcode == 0xE1 || opcode == 0xE2 || _in(opcode, 0xF1, 0xF3))
            return LaneSplitShape::ShiftCount;
        if ((opcode == 0x71 || opcode == 0x72) && (extension == 2 || extension == 4 || extension == 6))
            return LaneSplitShape::ShiftImmediate;
        if (opcode == 0x73 && (extension == 2 || extension == 3 || extension == 6 || extension == 7))
            return LaneSplitShape::ShiftImmediate;
        if (opcode == 0xD7)
            return LaneSplitShape::MoveMask;
        return std::nullopt;
    }
    if (map == Map0F38) {
        if (_in(opcode, 0x00, 0x0B) || opcode == 0x28 || opcode == 0x29 || opcode == 0x2B || _in(opcode, 0x37, 0x40))
            return LaneSplitShape::Binary;
        if (_in(opcode, 0x1C, 0x1E))
            return LaneSplitShape::Unary;
        return std::nullopt;
    }
    if (map != Map0F3A) return std::nullopt;
    if (opcode == 0x0E || opcode == 0x0F)
        return LaneSplitShape::BinaryImmediate;
    if (opcode == 0x42)
        return LaneSplitShape::LaneImmediate;
    if (opcode == 0x4C)
        return LaneSplitShape::Blend;
    return std::nullopt;
}

bool _hasImmediate(const LaneSplitShape shape) {
    return shape == LaneSplitShape::BinaryImmediate || shape == LaneSplitShape::ShiftImmediate || shape == LaneSplitShape::UnaryImmediate || shape == LaneSplitShape::LaneImmediate || shape == LaneSplitShape::Blend;
}

std::optional<std::uint8_t> _lowImmediate(const LaneSplitOperands& operands) {
    if (operands.Shape == LaneSplitShape::LaneImmediate) return static_cast<std::uint8_t>(*operands.Immediate & 7);
    if (operands.Shape == LaneSplitShape::Blend) return static_cast<std::uint8_t>(*operands.Immediate & 0xF0);
    return operands.Immediate;
}

std::optional<std::uint8_t> _highImmediate(const LaneSplitOperands& operands, const std::uint8_t maskHigh) {
    if (operands.Shape == LaneSplitShape::LaneImmediate) return static_cast<std::uint8_t>((*operands.Immediate >> 3) & 7);
    if (operands.Shape == LaneSplitShape::Blend) return static_cast<std::uint8_t>(maskHigh << 4);
    return operands.Immediate;
}

void _half(StubBodyBuilder& body, const LaneSplitOperands& operands, const std::uint8_t destination, const std::uint8_t first, const std::uint8_t second, const std::optional<std::uint8_t> immediate) {
    const Form form{operands.Map, operands.Prefix, operands.Wide, false, operands.Opcode};
    switch (operands.Shape) {
    case LaneSplitShape::Binary:
    case LaneSplitShape::BinaryImmediate:
    case LaneSplitShape::ShiftCount:
    case LaneSplitShape::LaneImmediate:
    case LaneSplitShape::Blend:
        Register(body, form, destination, first, second, immediate);
        return;
    case LaneSplitShape::ShiftImmediate:
        Register(body, form, operands.Extension, destination, first, immediate);
        return;
    case LaneSplitShape::Unary:
    case LaneSplitShape::UnaryImmediate:
        Register(body, form, destination, 0, second, immediate);
        return;
    case LaneSplitShape::MoveMask:
        throw CodegenException("256-bit byte mask has no vector destination to split");
    }
}

void _storeWord(StubBodyBuilder& body, const std::uint8_t reg, const std::int32_t displacement) {
    auto bytes = StackBytes(0x89, false, reg, displacement);
    bytes.insert(bytes.begin(), 0x66);
    body.Raw(bytes);
}

void _moveMask(StubBodyBuilder& body, const LaneSplitOperands& operands) {
    const bool toStack = operands.Destination == Stack;
    const std::uint8_t work = toStack ? std::uint8_t{0} : operands.Destination;
    const std::uint8_t high = operands.Second == 15 ? std::uint8_t{14} : std::uint8_t{15};
    StackSlot(body, 0x8D, true, Stack, -kMaskFrame);
    VexEncoding::StackOperand(body, MoveUnaligned256Store, high, 0, 0);
    if (toStack) StackSlot(body, 0x89, true, work, kWorkSlot);
    ExtractUpper(body, high, operands.Second);
    Register(body, kMoveMask128, work, 0, high);
    _storeWord(body, work, kMaskSlot + 2);
    Register(body, kMoveMask128, work, 0, operands.Second);
    _storeWord(body, work, kMaskSlot);
    if (toStack) StackSlot(body, 0x8B, true, work, kWorkSlot);
    VexEncoding::StackOperand(body, MoveUnaligned256Load, high, 0, 0);
    StackSlot(body, 0x8B, false, operands.Destination, kMaskSlot);
    if (!toStack) StackSlot(body, 0x8D, true, Stack, kMaskFrame);
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
    const auto extensions = threeByte ? static_cast<std::uint8_t>(~data[vex + 1]) : static_cast<std::uint8_t>(~data[vex + 1] & 0x80);
    const auto map = threeByte ? static_cast<std::uint8_t>(data[vex + 1] & 0x1F) : Map0F;
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
    if (*shape == LaneSplitShape::MoveMask && ((modrm >> 6) != 3 || operands.First != 0)) return std::nullopt;
    if (*shape == LaneSplitShape::Blend && operands.Wide) return std::nullopt;
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
    if (operands.Shape == LaneSplitShape::MoveMask) {
        _moveMask(body, operands);
        return;
    }
    const bool blend = operands.Shape == LaneSplitShape::Blend;
    const auto mask = blend ? static_cast<std::uint8_t>(*operands.Immediate >> 4) : std::uint8_t{0};
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    if (operands.Shape != LaneSplitShape::Unary && operands.Shape != LaneSplitShape::UnaryImmediate) used[operands.First] = true;
    if (!operands.Memory && operands.Shape != LaneSplitShape::ShiftImmediate) used[operands.Second] = true;
    if (blend) used[mask] = true;
    std::array<std::uint8_t, 4> scratch{};
    const std::size_t count = blend ? 4 : 3;
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < count; --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const auto high = scratch[0];
    const auto secondHigh = scratch[1];
    const auto loaded = scratch[2];
    const auto maskHigh = scratch[3];
    const auto frame = kRedZone + kVectorSize * static_cast<std::int32_t>(count);
    StackSlot(body, 0x8D, true, Stack, -frame);
    for (std::size_t index = 0; index < count; ++index)
        VexEncoding::StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index) * kVectorSize);
    auto second = operands.Second;
    if (operands.Memory) {
        VexEncoding::Memory(body, operands.Shape == LaneSplitShape::ShiftCount ? MoveUnaligned128Load : MoveUnaligned256Load, loaded, 0, *operands.Memory, frame, kName);
        second = loaded;
    }
    const auto highImmediate = _highImmediate(operands, maskHigh);
    switch (operands.Shape) {
    case LaneSplitShape::Blend:
        ExtractUpper(body, maskHigh, mask);
        [[fallthrough]];
    case LaneSplitShape::Binary:
    case LaneSplitShape::BinaryImmediate:
    case LaneSplitShape::LaneImmediate:
        ExtractUpper(body, high, operands.First);
        ExtractUpper(body, secondHigh, second);
        _half(body, operands, high, high, secondHigh, highImmediate);
        break;
    case LaneSplitShape::ShiftCount:
        ExtractUpper(body, high, operands.First);
        _half(body, operands, high, high, second, highImmediate);
        break;
    case LaneSplitShape::ShiftImmediate:
        ExtractUpper(body, high, operands.First);
        _half(body, operands, high, high, high, highImmediate);
        break;
    case LaneSplitShape::Unary:
    case LaneSplitShape::UnaryImmediate:
        ExtractUpper(body, high, second);
        _half(body, operands, high, 0, high, highImmediate);
        break;
    case LaneSplitShape::MoveMask:
        break;
    }
    _half(body, operands, operands.Destination, operands.First, second, _lowImmediate(operands));
    InsertUpper(body, operands.Destination, operands.Destination, high);
    for (std::size_t index = 0; index < count; ++index)
        VexEncoding::StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index) * kVectorSize);
    StackSlot(body, 0x8D, true, Stack, frame);
}

}
