#include <codegen/x86/PermuteLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr Form kLoadLow{Map0F, PrefixF2, false, false, 0x10};
constexpr Form kLoadHigh{Map0F, Prefix66, false, false, 0x16};
constexpr Form kSwapLanes{Map0F3A, Prefix66, false, true, 0x06};
constexpr Form kInLanePermute{Map0F38, Prefix66, false, true, 0x0C};
constexpr Form kShiftDwords{Map0F, Prefix66, false, false, 0x72};
constexpr Form kCompareDwords{Map0F, Prefix66, false, false, 0x76};
constexpr Form kExclusiveOr{Map0F, Prefix66, false, false, 0xEF};
constexpr Form kBlendBySign{Map0F3A, Prefix66, false, true, 0x4A};
constexpr std::uint8_t kShiftLeftExtension = 6;

template<std::size_t Count>
std::array<std::uint8_t, Count> _scratch(const std::array<bool, 16>& used) {
    std::array<std::uint8_t, Count> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < Count; --reg)
        if (!used[reg]) scratch[taken++] = reg;
    return scratch;
}

template<std::size_t Count>
void _save(StubBodyBuilder& body, const std::array<std::uint8_t, Count>& scratch, const std::int32_t frame) {
    StackSlot(body, 0x8D, true, Stack, -frame);
    for (std::size_t index = 0; index < Count; ++index)
        StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
}

template<std::size_t Count>
void _restore(StubBodyBuilder& body, const std::array<std::uint8_t, Count>& scratch, const std::int32_t frame) {
    for (std::size_t index = 0; index < Count; ++index)
        StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, frame);
}

void _immediate(StubBodyBuilder& body, const PermuteOperands& operands) {
    constexpr std::int32_t kSlot = 96;
    constexpr std::int32_t kFrame = 128 + kSlot + 32;
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    if (!operands.Memory) used[operands.Source] = true;
    const auto scratch = _scratch<3>(used);
    _save(body, scratch, kFrame);
    auto source = operands.Source;
    if (operands.Memory) {
        Memory(body, MoveUnaligned256Load, scratch[0], 0, *operands.Memory, kFrame, operands.Name);
        source = scratch[0];
    }
    StackOperand(body, MoveUnaligned256Store, source, 0, kSlot);
    const auto element = [&](unsigned index) { return static_cast<std::int32_t>(kSlot + 8 * ((operands.Immediate >> (2 * index)) & 3)); };
    StackOperand(body, kLoadLow, scratch[1], 0, element(0));
    StackOperand(body, kLoadHigh, scratch[1], scratch[1], element(1));
    StackOperand(body, kLoadLow, scratch[2], 0, element(2));
    StackOperand(body, kLoadHigh, scratch[2], scratch[2], element(3));
    InsertUpper(body, operands.Destination, scratch[1], scratch[2]);
    _restore(body, scratch, kFrame);
}

void _variable(StubBodyBuilder& body, const PermuteOperands& operands) {
    constexpr std::int32_t kFrame = 128 + 6 * 32;
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Index] = true;
    if (!operands.Memory) used[operands.Source] = true;
    const auto scratch = _scratch<6>(used);
    const auto swapped = scratch[1];
    const auto own = scratch[2];
    const auto mask = scratch[3];
    const auto upperMask = scratch[4];
    const auto signs = scratch[5];
    _save(body, scratch, kFrame);
    auto source = operands.Source;
    if (operands.Memory) {
        Memory(body, MoveUnaligned256Load, scratch[0], 0, *operands.Memory, kFrame, operands.Name);
        source = scratch[0];
    }
    Register(body, kSwapLanes, swapped, source, source, std::uint8_t{1});
    Register(body, kInLanePermute, own, source, operands.Index);
    Register(body, kInLanePermute, swapped, swapped, operands.Index);
    Register(body, kShiftDwords, kShiftLeftExtension, mask, operands.Index, std::uint8_t{29});
    ExtractUpper(body, upperMask, operands.Index);
    Register(body, kShiftDwords, kShiftLeftExtension, upperMask, upperMask, std::uint8_t{29});
    Register(body, kCompareDwords, signs, signs, signs);
    Register(body, kShiftDwords, kShiftLeftExtension, signs, signs, std::uint8_t{31});
    Register(body, kExclusiveOr, upperMask, upperMask, signs);
    InsertUpper(body, mask, mask, upperMask);
    Register(body, kBlendBySign, operands.Destination, own, swapped, static_cast<std::uint8_t>(mask << 4));
    _restore(body, scratch, kFrame);
}

}

std::optional<PermuteOperands> DecodePermute(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    std::vector<std::uint8_t> prefixes;
    while (vex < length && LegacyPrefix(data[vex])) prefixes.push_back(data[vex++]);
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 2] & 7) != (4 | Prefix66)) return std::nullopt;
    const auto map = static_cast<std::uint8_t>(data[vex + 1] & 0x1F);
    const bool wide = (data[vex + 2] & 0x80) != 0;
    const auto opcode = data[vex + 3];
    const char* name = nullptr;
    bool variable = false;
    if (map == Map0F3A && wide && opcode == 0x00) name = "VPERMQ";
    else if (map == Map0F3A && wide && opcode == 0x01) name = "VPERMPD";
    else if (map == Map0F38 && !wide && opcode == 0x36) { name = "VPERMD"; variable = true; }
    else if (map == Map0F38 && !wide && opcode == 0x16) { name = "VPERMPS"; variable = true; }
    else return std::nullopt;
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    PermuteOperands operands{name, variable,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        0, std::nullopt};
    std::size_t end = vex + 5;
    if ((modrm >> 6) != 3) {
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5)), prefixes);
        end = vex + 4 + operands.Memory->EncodedSize;
    }
    if (!variable) {
        if (end >= length) throw CodegenException(std::string(name) + " is missing its immediate");
        operands.Immediate = data[end++];
    }
    if (end != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return operands;
}

void EmitPermute(StubBodyBuilder& body, const PermuteOperands& operands) {
    if (operands.Variable)
        _variable(body, operands);
    else
        _immediate(body, operands);
}

}
