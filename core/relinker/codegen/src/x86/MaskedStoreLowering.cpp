#include <codegen/x86/MaskedStoreLowering.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace Codegen {

namespace {

constexpr std::int32_t kFrameSize = 128 + 3 * 8;
constexpr std::uint8_t kAddress = 0;
constexpr std::uint8_t kVexMap0F38 = 0x02;
constexpr std::uint8_t kVexPp66 = 0x01;

bool _prefix(std::uint8_t value) {
    return value == 0x2E || value == 0x36 || value == 0x3E || value == 0x26 || value == 0x64 || value == 0x65 || value == 0x67;
}

void _append32(std::vector<std::uint8_t>& bytes, std::int32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
}

void _address(StubBodyBuilder& body, const MemoryOperand& memory) {
    auto displacement = static_cast<std::int64_t>(memory.Displacement);
    if (memory.StackBase) displacement += kFrameSize;
    const bool address32 = std::find(memory.Prefixes.begin(), memory.Prefixes.end(), 0x67) != memory.Prefixes.end();
    if (!address32 && displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("Masked store stack displacement does not fit after spilling");
    const auto mod = memory.StackBase ? 2 : memory.Mod;
    auto bytes = memory.Prefixes;
    bytes.push_back(static_cast<std::uint8_t>(0x48 | memory.RexIndexBase));
    bytes.push_back(0x8D);
    bytes.push_back(static_cast<std::uint8_t>((mod << 6) | (kAddress << 3) | memory.Rm));
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

std::optional<MaskedStoreOperands> DecodeMaskedStore(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && _prefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != kVexMap0F38 || (data[vex + 2] & 3) != kVexPp66 || (data[vex + 4] >> 6) == 3)
        return std::nullopt;
    const bool w = (data[vex + 2] & 0x80) != 0;
    const char* name = nullptr;
    std::uint8_t elementSize = 4;
    if (data[vex + 3] == 0x2E && !w) {
        name = "VMASKMOVPS";
    } else if (data[vex + 3] == 0x2F && !w) {
        name = "VMASKMOVPD";
        elementSize = 8;
    } else if (data[vex + 3] == 0x8E) {
        name = w ? "VPMASKMOVQ" : "VPMASKMOVD";
        elementSize = w ? 8 : 4;
    } else {
        return std::nullopt;
    }
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
    auto memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
    if (vex + 4 + memory.EncodedSize != length)
        throw CodegenException("Masked store memory operand has trailing bytes");
    const auto displacementOffset = vex + 5 + (memory.Rm == 4 ? 1 : 0);
    return MaskedStoreOperands{name, elementSize, (data[vex + 2] & 4) != 0, static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        std::move(memory), std::vector<std::uint8_t>(data, data + length), displacementOffset};
}

void EmitMaskedStore(StubBodyBuilder& body, const MaskedStoreOperands& operands) {
    const auto& memory = operands.Memory;
    body.Raw(std::array<std::uint8_t, 8>{0x48, 0x8D, 0x64, 0x24, 0x80, 0x9C, 0x50, 0x51});
    _address(body, memory);
    body.Raw(std::array<std::uint8_t, 5>{0xC4, static_cast<std::uint8_t>(operands.Mask >= 8 ? 0xC1 : 0xE1),
        static_cast<std::uint8_t>(0x78 | (operands.Wide ? 4 : 0) | (operands.ElementSize == 8 ? 1 : 0)), 0x50,
        static_cast<std::uint8_t>(0xC8 | (operands.Mask & 7))});
    const auto elements = (operands.Wide ? 32u : 16u) / operands.ElementSize;
    for (unsigned element = 0; element < elements; ++element) {
        std::vector<std::uint8_t> probes;
        for (const auto offset : {element * operands.ElementSize, element * operands.ElementSize + operands.ElementSize - 1u}) {
            probes.insert(probes.end(), memory.Prefixes.begin(), memory.Prefixes.end());
            probes.insert(probes.end(), {0xF0, 0x80, 0x48, static_cast<std::uint8_t>(offset), 0x00});
        }
        body.Raw(std::array<std::uint8_t, 5>{0xF6, 0xC1, static_cast<std::uint8_t>(1u << element), 0x74, static_cast<std::uint8_t>(probes.size())});
        body.Raw(probes);
    }
    body.Raw(std::array<std::uint8_t, 11>{0x59, 0x58, 0x9D, 0x48, 0x8D, 0xA4, 0x24, 0x80, 0x00, 0x00, 0x00});
    if (memory.RipRelative)
        body.RipRelative(operands.Instruction, operands.DisplacementOffset, static_cast<std::int64_t>(memory.InstructionLength) + memory.Displacement);
    else
        body.Raw(operands.Instruction);
}

}
