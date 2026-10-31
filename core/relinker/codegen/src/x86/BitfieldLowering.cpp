#include <codegen/x86/BitfieldLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::uint8_t kControlRegister = 1;
constexpr std::int32_t kResultSlot = 0;
constexpr std::int32_t kControlSlot = 8;
constexpr std::int32_t kMaskSlot = 16;
constexpr std::int32_t kStackResultSlot = 24;
constexpr std::int32_t kFrame = FrameSize + 16;
constexpr std::uint8_t kJumpBelow = 0x72;
constexpr std::uint8_t kJumpAboveOrEqual = 0x73;
constexpr std::uint8_t kJump = 0xEB;
constexpr std::uint8_t kSetCarry = 0xF9;

std::uint8_t _bits(const BitfieldOperands& operands) {
    return operands.Wide ? 64 : 32;
}

std::vector<std::uint8_t> _compareControlByte(const std::uint8_t rm, const std::uint8_t bits) {
    return {0x80, static_cast<std::uint8_t>(0xF8 | rm), bits};
}

std::vector<std::uint8_t> _lowMask(const BitfieldOperands& operands, const std::uint8_t mask) {
    auto bytes = RegisterBytes(0xD3, operands.Wide, 4, mask);
    const auto invert = RegisterBytes(0xF7, operands.Wide, 2, mask);
    bytes.insert(bytes.end(), invert.begin(), invert.end());
    return bytes;
}

void _append(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& more) {
    bytes.insert(bytes.end(), more.begin(), more.end());
}

void _loadSource(StubBodyBuilder& body, const BitfieldOperands& operands, const std::uint8_t result, const char* name) {
    if (operands.Memory)
        Memory(body, 0x8B, operands.Wide, *operands.Memory, result, name, kFrame);
    else if (operands.Source == Stack)
        StackSlot(body, 0x8D, true, result, kFrame);
    else
        Register(body, 0x8B, operands.Wide, result, operands.Source);
}

void _zeroHigh(StubBodyBuilder& body, const BitfieldOperands& operands, const std::uint8_t result, const std::uint8_t mask) {
    auto high = operands.Wide ? std::vector<std::uint8_t>{} : RegisterBytes(0x8B, false, result, result);
    _append(high, RegisterBytes(0x85, operands.Wide, result, result));
    high.push_back(kSetCarry);
    auto low = _lowMask(operands, mask);
    _append(low, RegisterBytes(0x21, operands.Wide, mask, result));
    body.Raw(_compareControlByte(kControlRegister, _bits(operands)));
    body.Raw(std::array{kJumpBelow, static_cast<std::uint8_t>(high.size() + 2)});
    body.Raw(high);
    body.Raw(std::array{kJump, static_cast<std::uint8_t>(low.size())});
    body.Raw(low);
}

void _extract(StubBodyBuilder& body, const BitfieldOperands& operands, const std::uint8_t result, const std::uint8_t mask) {
    const auto bits = _bits(operands);
    std::vector<std::uint8_t> length{0x88, 0xE9};
    _append(length, _lowMask(operands, mask));
    const auto zero = RegisterBytes(0x31, operands.Wide, result, result);
    auto shifted = RegisterBytes(0xD3, operands.Wide, 5, result);
    _append(shifted, _compareControlByte(5, bits));
    shifted.push_back(kJumpAboveOrEqual);
    shifted.push_back(static_cast<std::uint8_t>(length.size() + 2 + zero.size()));
    _append(shifted, length);
    shifted.push_back(kJump);
    shifted.push_back(static_cast<std::uint8_t>(zero.size()));
    body.Raw(_compareControlByte(kControlRegister, bits));
    body.Raw(std::array{kJumpAboveOrEqual, static_cast<std::uint8_t>(shifted.size())});
    body.Raw(shifted);
    body.Raw(zero);
    Register(body, 0x21, operands.Wide, mask, result);
}

}

const char* BitfieldName(const BitfieldOperation operation) {
    switch (operation) {
    case BitfieldOperation::ZeroHigh: return "BZHI";
    case BitfieldOperation::Extract: return "BEXTR";
    }
    throw CodegenException("unknown bitfield operation");
}

std::optional<BitfieldOperands> DecodeBitfield(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || (data[vex + 2] & 3) != 0) return std::nullopt;
    BitfieldOperation operation;
    if (data[vex + 3] == 0xF5)
        operation = BitfieldOperation::ZeroHigh;
    else if (data[vex + 3] == 0xF7)
        operation = BitfieldOperation::Extract;
    else
        return std::nullopt;
    const auto name = BitfieldName(operation);
    if ((data[vex + 2] & 4) != 0) throw CodegenException(std::string(name) + " requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    BitfieldOperands operands{
        operation,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        (data[vex + 2] & 0x80) != 0, std::nullopt};
    std::size_t end = vex + 5;
    if ((modrm >> 6) != 3) {
        const auto rex = static_cast<std::uint8_t>(((extensions & 0x20) >> 5) | ((extensions & 0x40) >> 5));
        operands.Memory = DecodeMemoryOperand(data, length, vex + 4, rex, {data, data + vex});
        end = vex + 4 + operands.Memory->EncodedSize;
    }
    if (end != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return operands;
}

void EmitBitfield(StubBodyBuilder& body, const BitfieldOperands& operands) {
    const auto name = BitfieldName(operands.Operation);
    auto used = operands.Memory ? UsedRegisters({operands.Destination, operands.Control, kControlRegister}, operands.Memory)
                                : UsedRegisters({operands.Destination, operands.Source, operands.Control, kControlRegister}, std::nullopt);
    const auto result = Scratch(used, name);
    const auto mask = Scratch(used, name);
    StackSlot(body, 0x8D, true, Stack, -kFrame);
    StackSlot(body, 0x89, true, result, kResultSlot);
    StackSlot(body, 0x89, true, kControlRegister, kControlSlot);
    StackSlot(body, 0x89, true, mask, kMaskSlot);
    _loadSource(body, operands, result, name);
    if (operands.Control == Stack)
        StackSlot(body, 0x8D, true, kControlRegister, kFrame);
    else if (operands.Control != kControlRegister)
        Register(body, 0x8B, true, kControlRegister, operands.Control);
    auto allOnes = RegisterBytes(0xC7, operands.Wide, 0, mask);
    Append32(allOnes, -1);
    body.Raw(allOnes);
    if (operands.Operation == BitfieldOperation::ZeroHigh)
        _zeroHigh(body, operands, result, mask);
    else
        _extract(body, operands, result, mask);
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, result, kStackResultSlot);
    else if (operands.Destination == kControlRegister)
        StackSlot(body, 0x89, true, result, kControlSlot);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, result);
    StackSlot(body, 0x8B, true, mask, kMaskSlot);
    StackSlot(body, 0x8B, true, kControlRegister, kControlSlot);
    StackSlot(body, 0x8B, true, result, kResultSlot);
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, kStackResultSlot);
    else
        StackSlot(body, 0x8D, true, Stack, kFrame);
}

}
