#include <codegen/x86/DepositExtractLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>
#include <utility>

namespace Codegen {

using namespace ScalarLowering;

namespace {

constexpr std::int32_t kFlagsBytes = 8;
constexpr std::int32_t kSourceSlot = 8;
constexpr std::int32_t kMaskSlot = 16;
constexpr std::int32_t kResultSlot = 24;
constexpr std::int32_t kLowestSlot = 32;
constexpr std::int32_t kBitSlot = 40;
constexpr std::int32_t kStackResultSlot = 48;
constexpr std::int32_t kSpillBytes = 64;
constexpr std::int32_t kFrame = FrameSize - 16 + kSpillBytes;
constexpr std::uint8_t kPushFlags = 0x9C;
constexpr std::uint8_t kPopFlags = 0x9D;
constexpr std::uint8_t kJumpNoCarry = 0x73;
constexpr std::uint8_t kJumpZero = 0x74;
constexpr std::uint8_t kJumpNotZero = 0x75;

void _append(std::vector<std::uint8_t>& bytes, const std::vector<std::uint8_t>& more) {
    bytes.insert(bytes.end(), more.begin(), more.end());
}

void _load(StubBodyBuilder& body, const bool wide, const std::uint8_t destination, const std::uint8_t source, const std::int32_t frame) {
    if (source == Stack)
        StackSlot(body, 0x8D, true, destination, frame);
    else
        Register(body, 0x8B, wide, destination, source);
}

std::vector<std::uint8_t> _lowestMaskBit(const bool wide, const std::uint8_t lowest, const std::uint8_t mask) {
    auto bytes = RegisterBytes(0x8B, wide, lowest, mask);
    _append(bytes, RegisterBytes(0xF7, wide, 3, lowest));
    _append(bytes, RegisterBytes(0x21, wide, mask, lowest));
    return bytes;
}

std::vector<std::uint8_t> _loop(std::vector<std::uint8_t> bytes, const bool wide, const std::uint8_t lowest, const std::uint8_t mask) {
    _append(bytes, RegisterBytes(0x31, wide, lowest, mask));
    bytes.push_back(kJumpNotZero);
    bytes.push_back(static_cast<std::uint8_t>(0x100 - (bytes.size() + 1)));
    return bytes;
}

std::vector<std::uint8_t> _deposit(const bool wide, const std::uint8_t source, const std::uint8_t mask, const std::uint8_t result, const std::uint8_t lowest) {
    auto bytes = _lowestMaskBit(wide, lowest, mask);
    const auto set = RegisterBytes(0x09, wide, lowest, result);
    _append(bytes, RegisterBytes(0xD1, wide, 5, source));
    bytes.push_back(kJumpNoCarry);
    bytes.push_back(static_cast<std::uint8_t>(set.size()));
    _append(bytes, set);
    return _loop(std::move(bytes), wide, lowest, mask);
}

std::vector<std::uint8_t> _extract(const bool wide, const std::uint8_t source, const std::uint8_t mask, const std::uint8_t result, const std::uint8_t lowest, const std::uint8_t bit) {
    auto bytes = _lowestMaskBit(wide, lowest, mask);
    const auto set = RegisterBytes(0x09, wide, bit, result);
    _append(bytes, RegisterBytes(0x85, wide, lowest, source));
    bytes.push_back(kJumpZero);
    bytes.push_back(static_cast<std::uint8_t>(set.size()));
    _append(bytes, set);
    _append(bytes, RegisterBytes(0x01, wide, bit, bit));
    return _loop(std::move(bytes), wide, lowest, mask);
}

}

const char* DepositExtractName(const DepositExtractOperation operation) {
    switch (operation) {
    case DepositExtractOperation::Deposit: return "PDEP";
    case DepositExtractOperation::Extract: return "PEXT";
    }
    throw CodegenException("unknown deposit or extract operation");
}

std::optional<DepositExtractOperands> DecodeDepositExtract(const std::uint8_t* data, const std::size_t length) {
    std::size_t vex = 0;
    while (vex < length && LegacyPrefix(data[vex])) ++vex;
    if (vex + 5 > length || data[vex] != 0xC4 || (data[vex + 1] & 0x1F) != 2 || data[vex + 3] != 0xF5) return std::nullopt;
    const auto pp = static_cast<std::uint8_t>(data[vex + 2] & 3);
    if (pp < 2) return std::nullopt;
    const auto operation = pp == 3 ? DepositExtractOperation::Deposit : DepositExtractOperation::Extract;
    const auto name = DepositExtractName(operation);
    if ((data[vex + 2] & 4) != 0) throw CodegenException(std::string(name) + " requires VEX.L=0");
    const auto extensions = static_cast<std::uint8_t>(~data[vex + 1]);
    const auto modrm = data[vex + 4];
    DepositExtractOperands operands{
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

void EmitDepositExtract(StubBodyBuilder& body, const DepositExtractOperands& operands) {
    const auto name = DepositExtractName(operands.Operation);
    const bool extract = operands.Operation == DepositExtractOperation::Extract;
    auto used = operands.Memory ? UsedRegisters({operands.Destination, operands.Source}, operands.Memory)
                                : UsedRegisters({operands.Destination, operands.Source, operands.Mask}, std::nullopt);
    const auto source = Scratch(used, name);
    const auto mask = Scratch(used, name);
    const auto result = Scratch(used, name);
    const auto lowest = Scratch(used, name);
    const auto bit = extract ? Scratch(used, name) : lowest;
    const auto frame = SelectFrame(operands.Memory, operands.Wide, kFrame, kSpillBytes);
    StackSlot(body, 0x8D, true, Stack, kFlagsBytes - frame);
    body.Raw(std::array{kPushFlags});
    StackSlot(body, 0x89, true, source, kSourceSlot);
    StackSlot(body, 0x89, true, mask, kMaskSlot);
    StackSlot(body, 0x89, true, result, kResultSlot);
    StackSlot(body, 0x89, true, lowest, kLowestSlot);
    if (extract) StackSlot(body, 0x89, true, bit, kBitSlot);
    _load(body, operands.Wide, source, operands.Source, frame);
    if (operands.Memory)
        Memory(body, 0x8B, operands.Wide, *operands.Memory, mask, name, frame);
    else
        _load(body, operands.Wide, mask, operands.Mask, frame);
    Register(body, 0x31, false, result, result);
    if (extract) {
        auto one = RegisterBytes(0xC7, false, 0, bit);
        Append32(one, 1);
        body.Raw(one);
    }
    const auto loop = extract ? _extract(operands.Wide, source, mask, result, lowest, bit) : _deposit(operands.Wide, source, mask, result, lowest);
    Register(body, 0x85, operands.Wide, mask, mask);
    body.Raw(std::array{kJumpZero, static_cast<std::uint8_t>(loop.size())});
    body.Raw(loop);
    if (operands.Destination == Stack)
        StackSlot(body, 0x89, operands.Wide, result, kStackResultSlot);
    else
        Register(body, 0x8B, operands.Wide, operands.Destination, result);
    if (extract) StackSlot(body, 0x8B, true, bit, kBitSlot);
    StackSlot(body, 0x8B, true, lowest, kLowestSlot);
    StackSlot(body, 0x8B, true, result, kResultSlot);
    StackSlot(body, 0x8B, true, mask, kMaskSlot);
    StackSlot(body, 0x8B, true, source, kSourceSlot);
    body.Raw(std::array{kPopFlags});
    if (operands.Destination == Stack)
        StackSlot(body, 0x8B, operands.Wide, Stack, kStackResultSlot - kFlagsBytes);
    else
        StackSlot(body, 0x8D, true, Stack, frame - kFlagsBytes);
}

}
