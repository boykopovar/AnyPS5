#include <codegen/x86/ParallelBitsLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::int32_t kFlagsSlot = 0;
constexpr std::int32_t kResultSlot = 8;
constexpr std::int32_t kMaskSlot = 16;
constexpr std::int32_t kSourceSlot = 24;
constexpr std::int32_t kBitSlot = 32;
constexpr std::int32_t kCounterSlot = 40;
constexpr std::int32_t kStackResultSlot = 48;
constexpr std::int32_t kSpillBytes = 56;
constexpr std::int32_t kFrame = 192;
constexpr std::uint8_t kPushFlags = 0x9C;
constexpr std::uint8_t kPopFlags = 0x9D;
constexpr std::uint8_t kJumpZero = 0x74;
constexpr std::uint8_t kJump = 0xEB;

struct Scratches {
    std::uint8_t Result;
    std::uint8_t Mask;
    std::uint8_t Source;
    std::uint8_t Bit;
    std::uint8_t Counter;
};

void _append(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& more) {
    bytes.insert(bytes.end(), more.begin(), more.end());
}

std::vector<std::uint8_t> _withImmediate(std::vector<std::uint8_t> bytes, const std::int32_t value) {
    Append32(bytes, value);
    return bytes;
}

std::vector<std::uint8_t> _lowestBit(const bool wide, const Scratches& scratches) {
    auto bytes = RegisterBytes(0x8B, wide, scratches.Bit, scratches.Mask);
    _append(bytes, RegisterBytes(0xF7, wide, 3, scratches.Bit));
    _append(bytes, RegisterBytes(0x23, wide, scratches.Bit, scratches.Mask));
    _append(bytes, RegisterBytes(0x31, wide, scratches.Bit, scratches.Mask));
    return bytes;
}

std::vector<std::uint8_t> _depositStep(const bool wide, const Scratches& scratches) {
    auto bytes = _lowestBit(wide, scratches);
    _append(bytes, _withImmediate(RegisterBytes(0xF7, wide, 0, scratches.Source), 1));
    const auto merge = RegisterBytes(0x09, wide, scratches.Bit, scratches.Result);
    bytes.push_back(kJumpZero);
    bytes.push_back(static_cast<std::uint8_t>(merge.size()));
    _append(bytes, merge);
    _append(bytes, RegisterBytes(0xD1, wide, 5, scratches.Source));
    return bytes;
}

std::vector<std::uint8_t> _extractStep(const bool wide, const Scratches& scratches) {
    auto bytes = _lowestBit(wide, scratches);
    _append(bytes, RegisterBytes(0x85, wide, scratches.Bit, scratches.Source));
    const auto merge = RegisterBytes(0x09, wide, scratches.Counter, scratches.Result);
    bytes.push_back(kJumpZero);
    bytes.push_back(static_cast<std::uint8_t>(merge.size()));
    _append(bytes, merge);
    _append(bytes, RegisterBytes(0xD1, wide, 4, scratches.Counter));
    return bytes;
}

void _loop(StubBodyBuilder& body, const bool wide, const Scratches& scratches, std::vector<std::uint8_t> step) {
    const auto test = RegisterBytes(0x85, wide, scratches.Mask, scratches.Mask);
    const auto length = test.size() + 2 + step.size() + 2;
    if (length > 128) throw CodegenException("parallel bits loop does not fit a short jump");
    step.push_back(kJump);
    step.push_back(static_cast<std::uint8_t>(-static_cast<std::int32_t>(length)));
    body.Raw(test);
    body.Raw(std::array{kJumpZero, static_cast<std::uint8_t>(step.size())});
    body.Raw(step);
}

void _loadRegister(StubBodyBuilder& body, const bool wide, const std::uint8_t scratch, const std::uint8_t reg, const std::int32_t frame) {
    if (reg == Stack)
        StackSlot(body, 0x8D, true, scratch, frame);
    else
        Register(body, 0x8B, wide, scratch, reg);
}

}

const char* ParallelBitsName(const ParallelBitsOperation operation) {
    switch (operation) {
    case ParallelBitsOperation::Deposit: return "PDEP";
    case ParallelBitsOperation::Extract: return "PEXT";
    }
    throw CodegenException("unknown parallel bits operation");
}

std::optional<ParallelBitsOperands> DecodeParallelBits(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF5) return std::nullopt;
    ParallelBitsOperation operation;
    if ((data[vex + 2] & 3) == 3)
        operation = ParallelBitsOperation::Deposit;
    else if ((data[vex + 2] & 3) == 2)
        operation = ParallelBitsOperation::Extract;
    else
        return std::nullopt;
    const auto name = ParallelBitsName(operation);
    if ((data[vex + 2] & 4) != 0) throw CodegenException(std::string(name) + " requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    ParallelBitsOperands operands{
        operation,
        static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((extensions & 0x80) >> 4)),
        static_cast<std::uint8_t>((~data[vex + 2] >> 3) & 15),
        static_cast<std::uint8_t>((modrm & 7) | ((extensions & 0x20) >> 2)),
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

void EmitParallelBits(StubBodyBuilder& body, const ParallelBitsOperands& operands) {
    const auto name = ParallelBitsName(operands.Operation);
    const bool deposit = operands.Operation == ParallelBitsOperation::Deposit;
    auto used = operands.Memory ? UsedRegisters({operands.Destination, operands.Source}, operands.Memory)
                                : UsedRegisters({operands.Destination, operands.Source, operands.Mask}, std::nullopt);
    Scratches scratches{};
    scratches.Result = Scratch(used, name);
    scratches.Mask = Scratch(used, name);
    scratches.Source = Scratch(used, name);
    scratches.Bit = Scratch(used, name);
    if (!deposit) scratches.Counter = Scratch(used, name);
    const auto frame = SelectFrame(operands.Memory, operands.Wide, kFrame, kSpillBytes);
    StackSlot(body, 0x8D, true, Stack, kFlagsSlot + 8 - frame);
    body.Raw(std::array{kPushFlags});
    StackSlot(body, 0x89, true, scratches.Result, kResultSlot);
    StackSlot(body, 0x89, true, scratches.Mask, kMaskSlot);
    StackSlot(body, 0x89, true, scratches.Source, kSourceSlot);
    StackSlot(body, 0x89, true, scratches.Bit, kBitSlot);
    if (!deposit) StackSlot(body, 0x89, true, scratches.Counter, kCounterSlot);
    _loadRegister(body, operands.Wide, scratches.Source, operands.Source, frame);
    if (operands.Memory)
        Memory(body, 0x8B, operands.Wide, *operands.Memory, scratches.Mask, name, frame);
    else
        _loadRegister(body, operands.Wide, scratches.Mask, operands.Mask, frame);
    Register(body, 0x31, operands.Wide, scratches.Result, scratches.Result);
    if (!deposit) body.Raw(_withImmediate(RegisterBytes(0xC7, operands.Wide, 0, scratches.Counter), 1));
    _loop(body, operands.Wide, scratches, deposit ? _depositStep(operands.Wide, scratches) : _extractStep(operands.Wide, scratches));
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, scratches.Result, kStackResultSlot);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, scratches.Result);
    if (!deposit) StackSlot(body, 0x8B, true, scratches.Counter, kCounterSlot);
    StackSlot(body, 0x8B, true, scratches.Bit, kBitSlot);
    StackSlot(body, 0x8B, true, scratches.Source, kSourceSlot);
    StackSlot(body, 0x8B, true, scratches.Mask, kMaskSlot);
    StackSlot(body, 0x8B, true, scratches.Result, kResultSlot);
    body.Raw(std::array{kPopFlags});
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, kStackResultSlot - 8);
    else
        StackSlot(body, 0x8D, true, Stack, frame - 8);
}

}
