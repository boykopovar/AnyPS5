#include <codegen/x86/BlsLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::uint8_t kSetCarry = 0xF9;

void _append(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& instruction) {
    bytes.insert(bytes.end(), instruction.begin(), instruction.end());
}

std::vector<std::uint8_t> _leaMinusOne(std::uint8_t destination, std::uint8_t base) {
    std::vector<std::uint8_t> bytes{Rex(true, destination, base), 0x8D, static_cast<std::uint8_t>(0x40 | ((destination & 7) << 3) | (base & 7))};
    if ((base & 7) == 4) bytes.push_back(0x24);
    bytes.push_back(0xFF);
    return bytes;
}

std::vector<std::uint8_t> _nonzero(const BlsOperands& operands, std::uint8_t result, std::uint8_t temporary) {
    std::vector<std::uint8_t> bytes;
    switch (operands.Operation) {
    case BlsOperation::Reset:
        _append(bytes, _leaMinusOne(temporary, result));
        _append(bytes, RegisterBytes(0x23, operands.Wide, result, temporary));
        break;
    case BlsOperation::Mask:
        _append(bytes, _leaMinusOne(temporary, result));
        _append(bytes, RegisterBytes(0x33, operands.Wide, result, temporary));
        break;
    case BlsOperation::Isolate:
        _append(bytes, RegisterBytes(0x8B, operands.Wide, temporary, result));
        _append(bytes, RegisterBytes(0xF7, operands.Wide, 3, temporary));
        _append(bytes, RegisterBytes(0x23, operands.Wide, result, temporary));
        bytes.push_back(kSetCarry);
        break;
    }
    return bytes;
}

std::vector<std::uint8_t> _zero(const BlsOperands& operands, std::uint8_t result) {
    std::vector<std::uint8_t> bytes;
    switch (operands.Operation) {
    case BlsOperation::Reset:
        _append(bytes, RegisterBytes(0x33, operands.Wide, result, result));
        bytes.push_back(kSetCarry);
        break;
    case BlsOperation::Mask:
        _append(bytes, RegisterBytes(0x83, operands.Wide, 1, result));
        bytes.push_back(0xFF);
        bytes.push_back(kSetCarry);
        break;
    case BlsOperation::Isolate:
        _append(bytes, RegisterBytes(0x33, operands.Wide, result, result));
        break;
    }
    return bytes;
}

}

const char* BlsName(const BlsOperation operation) {
    switch (operation) {
    case BlsOperation::Reset: return "BLSR";
    case BlsOperation::Mask: return "BLSMSK";
    case BlsOperation::Isolate: return "BLSI";
    }
    throw CodegenException("unknown BLS operation");
}

std::optional<BlsOperands> DecodeBls(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF3)
        return std::nullopt;
    const auto modrm = data[vex + 4];
    const auto extension = static_cast<std::uint8_t>((modrm >> 3) & 7);
    if (extension < 1 || extension > 3)
        return std::nullopt;
    if ((data[vex + 2] & 7) != 0)
        throw CodegenException(std::string(BlsName(static_cast<BlsOperation>(extension))) + " requires VEX.L=0 and VEX.pp=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    BlsOperands operands{
        static_cast<BlsOperation>(extension),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    if ((modrm >> 6) == 3) {
        if (vex + 5 != length) throw CodegenException(std::string(BlsName(operands.Operation)) + " register operand has trailing bytes");
    } else {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        if (vex + 4 + operands.Memory->EncodedSize != length)
            throw CodegenException(std::string(BlsName(operands.Operation)) + " memory operand has trailing bytes");
    }
    return operands;
}

void EmitBls(StubBodyBuilder& body, const BlsOperands& operands) {
    const auto name = BlsName(operands.Operation);
    auto used = operands.Memory ? UsedRegisters({operands.Destination}, operands.Memory) : UsedRegisters({operands.Destination, operands.Source}, std::nullopt);
    const auto result = Scratch(used, name);
    const auto temporary = Scratch(used, name);
    StackSlot(body, 0x8D, true, Stack, -FrameSize);
    StackSlot(body, 0x89, true, result, 0);
    StackSlot(body, 0x89, true, temporary, 8);
    if (operands.Memory)
        Memory(body, 0x8B, operands.Wide, *operands.Memory, result, name);
    else if (operands.Source == Stack)
        StackSlot(body, 0x8D, true, result, FrameSize);
    else
        Register(body, 0x8B, operands.Wide, result, operands.Source);
    Register(body, 0x85, operands.Wide, result, result);
    const auto nonzero = _nonzero(operands, result, temporary);
    const auto zero = _zero(operands, result);
    body.Raw(std::array{std::uint8_t{0x74}, static_cast<std::uint8_t>(nonzero.size() + 2)});
    body.Raw(nonzero);
    body.Raw(std::array{std::uint8_t{0xEB}, static_cast<std::uint8_t>(zero.size())});
    body.Raw(zero);
    StackSlot(body, 0x8B, true, temporary, 8);
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, result, 8);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, result);
    StackSlot(body, 0x8B, true, result, 0);
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, 8);
    else
        StackSlot(body, 0x8D, true, Stack, FrameSize);
}

}
