#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>

namespace Codegen {

namespace {

constexpr std::uint8_t kVexMap0F = 0x01;
constexpr std::uint8_t kVexUnusedRegister = 0x0F;
constexpr std::uint8_t kOpcodeRsqrtps = 0x52;
constexpr std::uint8_t kOpcodeRcpps = 0x53;

}

std::optional<ReciprocalOperands> DecodeVexReciprocal(const std::uint8_t* data, const std::size_t length) {
    using namespace X64OpcodeConstants;
    if (length < 4)
        return std::nullopt;
    bool registerExtension = false;
    bool rmExtension = false;
    std::uint8_t payload = 0;
    std::size_t opcodeOffset = 0;
    if (data[0] == OneByteVex2 && length == 4) {
        registerExtension = (data[1] & 0x80) == 0;
        payload = data[1];
        opcodeOffset = 2;
    } else if (data[0] == OneByteVex3 && length == 5) {
        if ((data[1] & Vex3MapMask) != kVexMap0F)
            return std::nullopt;
        registerExtension = (data[1] & 0x80) == 0;
        rmExtension = (data[1] & 0x20) == 0;
        payload = data[2];
        opcodeOffset = 3;
    } else {
        return std::nullopt;
    }
    const auto vvvv = static_cast<std::uint8_t>((payload >> 3) & 0x0F);
    const bool wide = (payload & 0x04) != 0;
    const auto pp = static_cast<std::uint8_t>(payload & 0x03);
    if (vvvv != kVexUnusedRegister || wide || pp != 0)
        return std::nullopt;
    const auto opcode = data[opcodeOffset];
    if (opcode != kOpcodeRsqrtps && opcode != kOpcodeRcpps)
        return std::nullopt;
    const auto modrm = data[opcodeOffset + 1];
    if (((modrm >> ModRmModShift) & ModRmModMask) != ModRmModRegister)
        return std::nullopt;
    ReciprocalOperands operands{};
    operands.Operation = opcode == kOpcodeRsqrtps ? ReciprocalOperation::ReciprocalSquareRoot : ReciprocalOperation::Reciprocal;
    operands.Destination = static_cast<std::uint8_t>(((modrm >> ModRmRegShift) & ModRmRegMask) | (registerExtension ? 8 : 0));
    operands.Source = static_cast<std::uint8_t>((modrm & ModRmRmMask) | (rmExtension ? 8 : 0));
    return operands;
}

}
