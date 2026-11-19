#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <vector>

namespace Codegen {

namespace {

constexpr std::uint8_t kVexMap0F = 0x01;
constexpr std::uint8_t kVexUnusedRegister = 0x0F;
constexpr std::uint8_t kOpcodeRsqrtps = 0x52;
constexpr std::uint8_t kOpcodeRcpps = 0x53;
constexpr std::uint8_t kScalarPrefix = 0xF3;
constexpr std::uint8_t kVexScalarPrefix = 0x02;

bool _addressPrefix(const std::uint8_t byte) {
    using namespace X64OpcodeConstants;
    return byte == PrefixSegEs || byte == PrefixSegCs || byte == PrefixSegSs || byte == PrefixSegDs || byte == PrefixAddressSize;
}

}

std::optional<ReciprocalOperands> DecodeReciprocal(const std::uint8_t* data, const std::size_t length) {
    using namespace X64OpcodeConstants;
    std::vector<std::uint8_t> prefixes;
    bool legacyScalar = false;
    std::size_t start = 0;
    for (; start < length; ++start) {
        if (data[start] == kScalarPrefix)
            legacyScalar = true;
        else if (_addressPrefix(data[start]))
            prefixes.push_back(data[start]);
        else
            break;
    }
    const auto* const code = data + start;
    const auto remaining = length - start;
    if (remaining < 3)
        return std::nullopt;
    std::uint8_t rex = 0;
    std::uint8_t payload = 0;
    std::size_t opcodeOffset = 0;
    bool vex = true;
    if (!legacyScalar && code[0] == OneByteVex2 && remaining >= 4) {
        rex = (code[1] & 0x80) == 0 ? 0x04 : 0;
        payload = code[1];
        opcodeOffset = 2;
    } else if (!legacyScalar && code[0] == OneByteVex3 && remaining >= 5) {
        if ((code[1] & Vex3MapMask) != kVexMap0F)
            return std::nullopt;
        rex = static_cast<std::uint8_t>((~code[1] >> 5) & 0x07);
        payload = code[2];
        opcodeOffset = 3;
    } else if (code[0] == TwoByteOpcodeEscape) {
        opcodeOffset = 1;
        vex = false;
    } else if (code[0] >= RexMin && code[0] <= RexMax && remaining >= 4 && code[1] == TwoByteOpcodeEscape) {
        rex = static_cast<std::uint8_t>(code[0] & 0x07);
        opcodeOffset = 2;
        vex = false;
    } else {
        return std::nullopt;
    }
    const auto vvvv = static_cast<std::uint8_t>((payload >> 3) & 0x0F);
    const bool wide = (payload & 0x04) != 0;
    const auto pp = static_cast<std::uint8_t>(payload & 0x03);
    const bool vexScalar = vex && pp == kVexScalarPrefix;
    if (vex && !vexScalar && (vvvv != kVexUnusedRegister || pp != 0))
        return std::nullopt;
    const auto opcode = code[opcodeOffset];
    if (opcode != kOpcodeRsqrtps && opcode != kOpcodeRcpps)
        return std::nullopt;
    const auto modRmOffset = start + opcodeOffset + 1;
    const auto modrm = data[modRmOffset];
    const bool registerForm = ((modrm >> ModRmModShift) & ModRmModMask) == ModRmModRegister;
    std::optional<MemoryOperand> memory;
    if (registerForm) {
        if (modRmOffset + 1 != length)
            return std::nullopt;
    } else {
        memory = DecodeMemoryOperand(data, length, modRmOffset, static_cast<std::uint8_t>(rex & 0x03), prefixes);
        if (modRmOffset + memory->EncodedSize != length)
            return std::nullopt;
    }
    ReciprocalOperands operands{};
    operands.Operation = opcode == kOpcodeRsqrtps ? ReciprocalOperation::ReciprocalSquareRoot : ReciprocalOperation::Reciprocal;
    operands.Destination = static_cast<std::uint8_t>(((modrm >> ModRmRegShift) & ModRmRegMask) | ((rex & 0x04) != 0 ? 8 : 0));
    operands.Source = registerForm ? static_cast<std::uint8_t>((modrm & ModRmRmMask) | ((rex & 0x01) != 0 ? 8 : 0)) : operands.Destination;
    operands.Vex = vex;
    operands.Scalar = legacyScalar || vexScalar;
    operands.Upper = vexScalar ? static_cast<std::uint8_t>(~vvvv & 0x0F) : operands.Destination;
    operands.Wide = vex && !vexScalar && wide;
    operands.Memory = std::move(memory);
    return operands;
}

}
