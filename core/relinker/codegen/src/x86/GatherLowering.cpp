#include <codegen/x86/GatherLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::int32_t kFrame = 240;
constexpr std::int32_t kSavedRegister = 8;
constexpr std::int32_t kDestinationSlot = 16;
constexpr std::int32_t kIndexSlot = 48;
constexpr std::int32_t kMaskSlot = 80;
constexpr Form kClear{Map0F, Prefix66, false, false, 0xEF};

const char* _name(const std::uint8_t opcode, const bool wide) {
    switch (opcode) {
    case 0x90: return wide ? "VPGATHERDQ" : "VPGATHERDD";
    case 0x91: return wide ? "VPGATHERQQ" : "VPGATHERQD";
    case 0x92: return wide ? "VGATHERDPD" : "VGATHERDPS";
    case 0x93: return wide ? "VGATHERQPD" : "VGATHERQPS";
    default: return nullptr;
    }
}

std::vector<std::uint8_t> _stack(const std::uint8_t rex, const std::uint8_t opcode, const std::uint8_t reg, const std::int32_t displacement) {
    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>(rex | ((reg & 8) >> 1)), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24};
    Append32(bytes, displacement);
    return bytes;
}

bool _hasBase(const MemoryOperand& memory) {
    return memory.Mod != 0 || (memory.Sib & 7) != 5;
}

std::uint8_t _base(const MemoryOperand& memory) {
    return static_cast<std::uint8_t>((memory.Sib & 7) | ((memory.RexIndexBase & 1) << 3));
}

std::uint8_t _scratch(const MemoryOperand& memory) {
    for (const std::uint8_t reg : {0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 14, 15})
        if (!_hasBase(memory) || reg != _base(memory)) return reg;
    throw CodegenException("gather has no scratch register");
}

std::vector<std::uint8_t> _address(const GatherOperands& operands, const std::uint8_t reg) {
    const auto& memory = operands.Memory;
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrame + 8;
    if (displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException(std::string(operands.Name) + " stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? std::uint8_t{2} : memory.Mod;
    auto bytes = memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(0x48 | ((reg & 8) >> 1) | ((reg & 8) >> 2) | (memory.RexIndexBase & 1)));
    bytes.push_back(0x8D);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | ((reg & 7) << 3) | 4));
    bytes.push_back(static_cast<std::uint8_t>((memory.Sib & 0xC7) | ((reg & 7) << 3)));
    if (mod == 1) bytes.push_back(static_cast<std::uint8_t>(displacement));
    if (mod == 2 || !_hasBase(memory)) Append32(bytes, static_cast<std::int32_t>(displacement));
    return bytes;
}

std::vector<std::uint8_t> _load(const GatherOperands& operands, const std::uint8_t reg) {
    auto bytes = operands.Memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(0x40 | (operands.QwordElement ? 8 : 0) | ((reg & 8) >> 1) | ((reg & 8) >> 3)));
    bytes.push_back(0x8B);
    bytes.push_back(static_cast<std::uint8_t>(((reg & 7) << 3) | (reg & 7)));
    return bytes;
}

std::vector<std::uint8_t> _element(const GatherOperands& operands, const std::uint8_t reg, const std::size_t index) {
    const std::int32_t elementSize = operands.QwordElement ? 8 : 4;
    const std::int32_t indexSize = operands.QwordIndex ? 8 : 4;
    const auto position = static_cast<std::int32_t>(index);
    std::vector<std::uint8_t> work;
    const auto append = [&work](const std::vector<std::uint8_t>& bytes) { work.insert(work.end(), bytes.begin(), bytes.end()); };
    append(_stack(0x48, operands.QwordIndex ? 0x8B : 0x63, reg, kIndexSlot + position * indexSize));
    append(_address(operands, reg));
    append(_load(operands, reg));
    append(_stack(operands.QwordElement ? 0x48 : 0x40, 0x89, reg, kDestinationSlot + position * elementSize));
    if (work.size() > 127) throw CodegenException(std::string(operands.Name) + " element load does not fit a short branch");
    std::vector<std::uint8_t> bytes{0xF6, 0x84, 0x24};
    Append32(bytes, kMaskSlot + position * elementSize + elementSize - 1);
    bytes.insert(bytes.end(), {0x80, 0x74, static_cast<std::uint8_t>(work.size())});
    bytes.insert(bytes.end(), work.begin(), work.end());
    return bytes;
}

}

std::optional<GatherOperands> DecodeGather(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    std::vector<std::uint8_t> prefixes;
    while (vex < length && LegacyPrefix(data[vex])) prefixes.push_back(data[vex++]);
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != Map0F38 || (data[vex + 2] & 3) != Prefix66) return std::nullopt;
    const bool wide = (data[vex + 2] & 0x80) != 0;
    const auto* name = _name(data[vex + 3], wide);
    if (!name) return std::nullopt;
    const auto modrm = data[vex + 4];
    if ((modrm >> 6) == 3 || (modrm & 7) != 4) return std::nullopt;
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    auto memory = DecodeMemoryOperand(data, length, vex + 4, static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5)), prefixes);
    if (vex + 4 + memory.EncodedSize != length) throw CodegenException(std::string(name) + " has trailing bytes");
    GatherOperands operands{name, (data[vex + 2] & 4) != 0, (data[vex + 3] & 1) != 0, wide,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>(((memory.Sib >> 3) & 7) | ((memory.RexIndexBase & 2) << 2)),
        std::move(memory)};
    if (operands.Destination == operands.Mask || operands.Destination == operands.Index || operands.Mask == operands.Index) return std::nullopt;
    return operands;
}

void EmitGather(StubBodyBuilder& body, const GatherOperands& operands) {
    const std::size_t elementSize = operands.QwordElement ? 8 : 4;
    const std::size_t count = (operands.Vector256 ? 32 : 16) / std::max<std::size_t>(elementSize, operands.QwordIndex ? 8 : 4);
    const std::size_t gathered = count * elementSize;
    const auto reg = _scratch(operands.Memory);
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    body.Raw(std::vector<std::uint8_t>{0x9C});
    body.Raw(_stack(0x48, 0x89, reg, kSavedRegister));
    StackOperand(body, MoveUnaligned256Store, operands.Destination, 0, kDestinationSlot);
    StackOperand(body, MoveUnaligned256Store, operands.Index, 0, kIndexSlot);
    StackOperand(body, MoveUnaligned256Store, operands.Mask, 0, kMaskSlot);
    const std::size_t loaded = gathered > 16 ? 32 : 16;
    for (auto offset = gathered; offset < loaded; offset += 8) {
        auto clear = _stack(0x48, 0xC7, 0, kDestinationSlot + static_cast<std::int32_t>(offset));
        Append32(clear, 0);
        body.Raw(clear);
    }
    for (std::size_t index = 0; index < count; ++index) body.Raw(_element(operands, reg, index));
    StackOperand(body, loaded == 32 ? MoveUnaligned256Load : MoveUnaligned128Load, operands.Destination, 0, kDestinationSlot);
    Register(body, kClear, operands.Mask, operands.Mask, operands.Mask);
    body.Raw(_stack(0x48, 0x8B, reg, kSavedRegister));
    body.Raw(std::vector<std::uint8_t>{0x9D});
    StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
