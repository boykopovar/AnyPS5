#include <codegen/x86/FmaLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <codegen/x86/VexStub.hpp>
#include <array>
#include <string>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;
using namespace VexStub;

namespace {

constexpr std::int32_t kRedZone = 128;
constexpr std::int32_t kSlotBytes = 128;
constexpr std::size_t kScratch = 13;
constexpr std::uint8_t kNoPrefix = 0;
constexpr Form kAnd{Map0F, Prefix66, false, false, 0xDB};
constexpr Form kAndNot{Map0F, Prefix66, false, false, 0xDF};
constexpr Form kOr{Map0F, Prefix66, false, false, 0xEB};
constexpr Form kExclusiveOr{Map0F, Prefix66, false, false, 0xEF};
constexpr Form kGreaterDwords{Map0F, Prefix66, false, false, 0x66};
constexpr Form kEqualDwords{Map0F, Prefix66, false, false, 0x76};
constexpr Form kShuffleDwords{Map0F, Prefix66, false, false, 0x70};
constexpr Form kBlendBytes{Map0F3A, Prefix66, false, false, 0x4C};
constexpr Form kSubtractQwords{Map0F, Prefix66, false, false, 0xFB};
constexpr Form kShuffleFloats{Map0F, kNoPrefix, false, false, 0xC6};
constexpr Form kBlendFloats{Map0F3A, Prefix66, false, false, 0x0C};
constexpr Form kLoadVector{Map0F, PrefixF3, false, false, 0x6F};
constexpr Form kStoreVector{Map0F, PrefixF3, false, false, 0x7F};
constexpr Form kLoadDword{Map0F, Prefix66, false, false, 0x6E};
constexpr Form kStoreDword{Map0F, Prefix66, false, false, 0x7E};
constexpr Form kControl{Map0F, kNoPrefix, false, false, 0xAE};
constexpr Form kLoadScalar{Map0F, PrefixF3, false, false, 0x10};
constexpr Form kLoadFloats{Map0F, kNoPrefix, false, false, 0x10};
constexpr Form kLoadFloatsWide{Map0F, kNoPrefix, false, true, 0x10};
constexpr Form kWiden{Map0F, kNoPrefix, false, true, 0x5A};
constexpr Form kNarrow{Map0F, Prefix66, false, true, 0x5A};
constexpr Form kMultiplyDoubles{Map0F, Prefix66, false, true, 0x59};
constexpr Form kAddDoubles{Map0F, Prefix66, false, true, 0x58};
constexpr Form kSubtractDoubles{Map0F, Prefix66, false, true, 0x5C};
constexpr Form kCompareDoubles{Map0F, Prefix66, false, true, 0xC2};
constexpr Form kAndDoubles{Map0F, Prefix66, false, true, 0x54};
constexpr Form kOrDoubles{Map0F, Prefix66, false, true, 0x56};
constexpr Form kExclusiveOrDoubles{Map0F, Prefix66, false, true, 0x57};
constexpr Form kBlendDoubles{Map0F3A, Prefix66, false, true, 0x4B};
constexpr Form kBroadcastDouble{Map0F38, Prefix66, false, true, 0x19};
constexpr std::uint8_t kEqual = 0x00;
constexpr std::uint8_t kUnordered = 0x03;
constexpr std::uint8_t kNotEqual = 0x0C;
constexpr std::uint8_t kLoadControl = 2;
constexpr std::uint8_t kStoreControl = 3;
constexpr std::uint32_t kInvalidFlag = 1;
constexpr std::uint32_t kDenormalFlag = 2;
constexpr std::uint32_t kRoundingFlags = 0x38;
constexpr std::uint32_t kSign = 0x80000000;
constexpr std::array<std::uint32_t, 4> kQwordOne{1, 0, 1, 0};

struct Slots {
    std::int32_t Saved;
    std::int32_t Double;
    std::int32_t Narrow;
    std::int32_t Capture;
    std::int32_t RoundDown;
    std::int32_t Flush;
    std::int32_t NanValue;
    std::int32_t NanMask;
    std::int32_t ZeroSign;
    std::int32_t Upper;
    std::int32_t Denormal;
};

const char* _operationName(const FmaOperation operation) {
    switch (operation) {
    case FmaOperation::MultiplyAdd: return "VFMADD";
    case FmaOperation::MultiplySubtract: return "VFMSUB";
    case FmaOperation::NegatedMultiplyAdd: return "VFNMADD";
    case FmaOperation::NegatedMultiplySubtract: return "VFNMSUB";
    case FmaOperation::AddSubtract: return "VFMADDSUB";
    case FmaOperation::SubtractAdd: return "VFMSUBADD";
    }
    throw CodegenException("unknown FMA operation");
}

const char* _name(const FmaOperation operation, const FmaOrder order, const bool scalar, const bool precise) {
    static const auto names = [] {
        std::array<std::string, 72> table{};
        for (std::size_t operationIndex = 0; operationIndex < 6; ++operationIndex)
            for (std::size_t orderIndex = 0; orderIndex < 3; ++orderIndex)
                for (std::size_t scalarIndex = 0; scalarIndex < 2; ++scalarIndex)
                    for (std::size_t preciseIndex = 0; preciseIndex < 2; ++preciseIndex)
                        table[operationIndex * 12 + orderIndex * 4 + scalarIndex * 2 + preciseIndex] = std::string(_operationName(static_cast<FmaOperation>(operationIndex))) +
                            (orderIndex == 0 ? "132" : orderIndex == 1 ? "213" : "231") + (scalarIndex != 0 ? "S" : "P") + (preciseIndex != 0 ? "D" : "S");
        return table;
    }();
    return names[static_cast<std::size_t>(operation) * 12 + static_cast<std::size_t>(order) * 4 + (scalar ? 2 : 0) + (precise ? 1 : 0)].c_str();
}

std::optional<std::array<std::uint32_t, 4>> _addendSigns(const FmaOperation operation) {
    switch (operation) {
    case FmaOperation::MultiplySubtract:
    case FmaOperation::NegatedMultiplySubtract: return std::array<std::uint32_t, 4>{kSign, kSign, kSign, kSign};
    case FmaOperation::AddSubtract: return std::array<std::uint32_t, 4>{kSign, 0, kSign, 0};
    case FmaOperation::SubtractAdd: return std::array<std::uint32_t, 4>{0, kSign, 0, kSign};
    case FmaOperation::MultiplyAdd:
    case FmaOperation::NegatedMultiplyAdd: return std::nullopt;
    }
    throw CodegenException("unknown FMA operation");
}

void _narrowMask(StubBodyBuilder& body, const std::uint8_t mask, const std::uint8_t temporary) {
    ExtractUpper(body, temporary, mask);
    Register(body, kShuffleFloats, mask, mask, temporary, std::uint8_t{0x88});
}

std::uint8_t _quarter(StubBodyBuilder& body, ScratchPool& pool, const FmaOperands& operands, const Slots& slots,
    const std::array<std::uint8_t, 3>& inputs, const bool upper, const std::uint8_t flags) {
    std::array<std::uint8_t, 3> x{};
    for (std::size_t index = 0; index < x.size(); ++index) {
        x[index] = pool.Take();
        if (upper) ExtractUpper(body, x[index], inputs[index]);
        else if (operands.Scalar) ConstantLanes(body, kAnd, x[index], inputs[index], {0xFFFFFFFF, 0, 0, 0});
        else Register(body, kOr, x[index], inputs[index], inputs[index]);
    }

    const auto bits = pool.Take();
    const auto nanMask = pool.Take();
    const auto denormal = pool.Take();
    const auto value = pool.Take();
    const auto magnitude = pool.Take();
    const auto lane = pool.Take();
    const auto temporary = pool.Take();
    Register(body, kExclusiveOr, bits, bits, bits);
    Register(body, kExclusiveOr, nanMask, nanMask, nanMask);
    Register(body, kExclusiveOr, denormal, denormal, denormal);
    for (std::size_t step = 0; step < x.size(); ++step) {
        const auto operand = x[x.size() - 1 - step];
        Constant(body, kAnd, magnitude, operand, 0x7FFFFFFF);
        Constant(body, kGreaterDwords, lane, magnitude, 0x7F800000);
        if (step == 0) Register(body, kOr, value, operand, operand);
        else Register(body, kBlendBytes, value, value, operand, static_cast<std::uint8_t>(lane << 4));
        Register(body, kOr, nanMask, nanMask, lane);
        Constant(body, kAnd, temporary, operand, 0x00400000);
        Constant(body, kEqualDwords, temporary, temporary, 0);
        Register(body, kAnd, temporary, temporary, lane);
        Constant(body, kAnd, temporary, temporary, kInvalidFlag);
        Register(body, kOr, bits, bits, temporary);
        Constant(body, kGreaterDwords, lane, magnitude, 0x007FFFFF);
        Constant(body, kGreaterDwords, temporary, magnitude, 0);
        Register(body, kAndNot, temporary, lane, temporary);
        Register(body, kOr, denormal, denormal, temporary);
        VexEncoding::StackOperand(body, kAnd, temporary, temporary, slots.Flush);
        Constant(body, kAnd, temporary, temporary, 0x7FFFFFFF);
        Register(body, kAndNot, operand, temporary, operand);
    }
    Constant(body, kOr, value, value, 0x00400000);
    VexEncoding::StackOperand(body, kStoreVector, value, 0, slots.NanValue);
    VexEncoding::StackOperand(body, kStoreVector, nanMask, 0, slots.NanMask);
    Register(body, kAndNot, denormal, nanMask, denormal);
    VexEncoding::StackOperand(body, kLoadVector, temporary, 0, slots.Flush);
    Register(body, kAndNot, denormal, temporary, denormal);
    VexEncoding::StackOperand(body, kStoreVector, denormal, 0, slots.Denormal);
    pool.Give(denormal);
    pool.Give(nanMask);

    if (operands.Operation == FmaOperation::NegatedMultiplyAdd || operands.Operation == FmaOperation::NegatedMultiplySubtract)
        Constant(body, kExclusiveOr, x[0], x[0], kSign);
    if (const auto signs = _addendSigns(operands.Operation))
        ConstantLanes(body, kExclusiveOr, x[2], x[2], *signs);

    Register(body, kExclusiveOr, value, x[0], x[1]);
    Constant(body, kAnd, value, value, kSign);
    Constant(body, kAnd, magnitude, x[2], kSign);
    Register(body, kOr, lane, value, magnitude);
    Register(body, kAnd, value, value, magnitude);
    VexEncoding::StackOperand(body, kLoadVector, temporary, 0, slots.RoundDown);
    Register(body, kBlendBytes, value, value, lane, static_cast<std::uint8_t>(temporary << 4));
    VexEncoding::StackOperand(body, kStoreVector, value, 0, slots.ZeroSign);
    pool.Give(value);
    pool.Give(magnitude);
    pool.Give(lane);
    pool.Give(temporary);

    VexEncoding::StackOperand(body, kControl, kLoadControl, 0, slots.Double);
    std::array<std::uint8_t, 3> wide{};
    for (std::size_t index = 0; index < wide.size(); ++index) {
        wide[index] = pool.Take();
        Register(body, kWiden, wide[index], 0, x[index]);
        pool.Give(x[index]);
    }
    const auto product = wide[0];
    const auto factor = wide[1];
    const auto addend = wide[2];
    Register(body, kMultiplyDoubles, product, product, factor);
    const auto sum = pool.Take();
    Register(body, kAddDoubles, sum, product, addend);
    Register(body, kSubtractDoubles, factor, sum, product);
    const auto error = pool.Take();
    Register(body, kSubtractDoubles, error, sum, factor);
    Register(body, kSubtractDoubles, error, product, error);
    Register(body, kSubtractDoubles, factor, addend, factor);
    Register(body, kAddDoubles, error, error, factor);
    pool.Give(product);
    pool.Give(factor);
    pool.Give(addend);

    const auto zero = pool.Take();
    const auto inexact = pool.Take();
    const auto exactZero = pool.Take();
    const auto scratch = pool.Take();
    Register(body, kExclusiveOrDoubles, zero, zero, zero);
    Register(body, kCompareDoubles, inexact, error, zero, kNotEqual);
    Register(body, kCompareDoubles, exactZero, sum, zero, kEqual);
    Register(body, kCompareDoubles, scratch, error, zero, kEqual);
    Register(body, kAndDoubles, exactZero, exactZero, scratch);
    _narrowMask(body, exactZero, scratch);
    const auto invalid = zero;
    Register(body, kCompareDoubles, invalid, sum, sum, kUnordered);
    _narrowMask(body, invalid, scratch);
    VexEncoding::StackOperand(body, kLoadVector, scratch, 0, slots.NanMask);
    Register(body, kAndNot, invalid, scratch, invalid);
    VexEncoding::StackOperand(body, kLoadVector, scratch, 0, slots.Denormal);
    Register(body, kAndNot, scratch, invalid, scratch);
    Constant(body, kAnd, scratch, scratch, kDenormalFlag);
    Register(body, kOr, bits, bits, scratch);
    Constant(body, kAnd, invalid, invalid, kInvalidFlag);
    Register(body, kOr, bits, bits, invalid);
    pool.Give(invalid);

    Register(body, kExclusiveOrDoubles, error, error, sum);
    Register(body, kAndDoubles, error, error, inexact);
    const auto toward = pool.Take();
    ConstantLanes(body, kSubtractQwords, toward, sum, kQwordOne);
    ExtractUpper(body, scratch, sum);
    ConstantLanes(body, kSubtractQwords, scratch, scratch, kQwordOne);
    InsertUpper(body, toward, toward, scratch);
    Register(body, kBlendDoubles, sum, sum, toward, static_cast<std::uint8_t>(error << 4));
    ConstantLanes(body, kBroadcastDouble, toward, 0, kQwordOne);
    Register(body, kAndDoubles, toward, toward, inexact);
    Register(body, kOrDoubles, sum, sum, toward);
    pool.Give(toward);
    pool.Give(error);
    pool.Give(inexact);

    VexEncoding::StackOperand(body, kControl, kLoadControl, 0, slots.Narrow);
    const auto result = pool.Take();
    Register(body, kNarrow, result, 0, sum);
    pool.Give(sum);
    VexEncoding::StackOperand(body, kControl, kStoreControl, 0, slots.Capture);
    VexEncoding::StackOperand(body, kLoadDword, scratch, 0, slots.Capture);
    Constant(body, kAnd, scratch, scratch, kRoundingFlags);
    Register(body, kOr, flags, flags, scratch);

    VexEncoding::StackOperand(body, kLoadVector, scratch, 0, slots.ZeroSign);
    Register(body, kBlendBytes, result, result, scratch, static_cast<std::uint8_t>(exactZero << 4));
    VexEncoding::StackOperand(body, kLoadVector, scratch, 0, slots.NanValue);
    VexEncoding::StackOperand(body, kLoadVector, exactZero, 0, slots.NanMask);
    Register(body, kBlendBytes, result, result, scratch, static_cast<std::uint8_t>(exactZero << 4));
    pool.Give(exactZero);
    pool.Give(scratch);
    Register(body, kOr, flags, flags, bits);
    pool.Give(bits);
    return result;
}

}

std::optional<FmaOperands> DecodeFma(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Map != Map0F38 || instruction->Prefix != Prefix66)
        return std::nullopt;
    const auto high = static_cast<std::uint8_t>(instruction->Opcode & 0xF0);
    const auto low = static_cast<std::uint8_t>(instruction->Opcode & 0x0F);
    if (low < 6 || (high != 0x90 && high != 0xA0 && high != 0xB0)) return std::nullopt;
    const auto order = high == 0x90 ? FmaOrder::Order132 : high == 0xA0 ? FmaOrder::Order213 : FmaOrder::Order231;
    FmaOperation operation = FmaOperation::MultiplyAdd;
    bool scalar = false;
    switch (low) {
    case 0x6: operation = FmaOperation::AddSubtract; break;
    case 0x7: operation = FmaOperation::SubtractAdd; break;
    case 0x8: operation = FmaOperation::MultiplyAdd; break;
    case 0x9: operation = FmaOperation::MultiplyAdd; scalar = true; break;
    case 0xA: operation = FmaOperation::MultiplySubtract; break;
    case 0xB: operation = FmaOperation::MultiplySubtract; scalar = true; break;
    case 0xC: operation = FmaOperation::NegatedMultiplyAdd; break;
    case 0xD: operation = FmaOperation::NegatedMultiplyAdd; scalar = true; break;
    case 0xE: operation = FmaOperation::NegatedMultiplySubtract; break;
    default: operation = FmaOperation::NegatedMultiplySubtract; scalar = true; break;
    }
    const auto* name = _name(operation, order, scalar, instruction->Wide);
    if (instruction->End != length) throw CodegenException(std::string(name) + " has trailing bytes");
    return FmaOperands{name, operation, order, scalar, !scalar && instruction->Vector256, instruction->Wide, instruction->Reg, instruction->Vvvv, instruction->Rm, instruction->Memory};
}

void EmitFma(StubBodyBuilder& body, const FmaOperands& operands) {
    if (operands.Double) {
        EmitFmaDouble(body, operands);
        return;
    }
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Second] = true;
    if (!operands.Memory) used[operands.Third] = true;
    std::array<std::uint8_t, 16> scratch{};
    std::size_t taken = 0;
    for (std::uint8_t reg = 15; taken < kScratch; --reg)
        if (!used[reg]) scratch[taken++] = reg;
    const auto base = static_cast<std::int32_t>(32 * kScratch);
    const Slots slots{base, base + 4, base + 8, base + 12, base + 16, base + 32, base + 48, base + 64, base + 80, base + 96, base + 112};
    const auto frame = kRedZone + base + kSlotBytes;
    StackSlot(body, 0x8D, true, Stack, -frame);
    for (std::size_t index = 0; index < kScratch; ++index)
        VexEncoding::StackOperand(body, MoveUnaligned256Store, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    ScratchPool pool(scratch, kScratch, operands.Name);

    VexEncoding::StackOperand(body, kControl, kStoreControl, 0, slots.Saved);
    const auto control = pool.Take();
    const auto setting = pool.Take();
    VexEncoding::StackOperand(body, kLoadDword, control, 0, slots.Saved);
    Constant(body, kAnd, setting, control, 0xE040);
    Constant(body, kOr, setting, setting, 0x1F80);
    VexEncoding::StackOperand(body, kStoreDword, setting, 0, slots.Narrow);
    Constant(body, kLoadVector, setting, 0, 0x1F80);
    VexEncoding::StackOperand(body, kStoreDword, setting, 0, slots.Double);
    Constant(body, kAnd, setting, control, 0x6000);
    Constant(body, kEqualDwords, setting, setting, 0x2000);
    Register(body, kShuffleDwords, setting, 0, setting, std::uint8_t{0});
    VexEncoding::StackOperand(body, kStoreVector, setting, 0, slots.RoundDown);
    Constant(body, kAnd, setting, control, 0x40);
    Constant(body, kEqualDwords, setting, setting, 0x40);
    Register(body, kShuffleDwords, setting, 0, setting, std::uint8_t{0});
    VexEncoding::StackOperand(body, kStoreVector, setting, 0, slots.Flush);
    pool.Give(setting);
    pool.Give(control);

    const auto flags = pool.Take();
    Register(body, kExclusiveOr, flags, flags, flags);
    auto third = operands.Third;
    if (operands.Memory) {
        third = pool.Take();
        VexEncoding::Memory(body, operands.Scalar ? kLoadScalar : operands.Wide ? kLoadFloatsWide : kLoadFloats, third, 0, *operands.Memory, frame, operands.Name);
    }
    const auto inputs = operands.Order == FmaOrder::Order132 ? std::array<std::uint8_t, 3>{operands.Destination, third, operands.Second}
        : operands.Order == FmaOrder::Order213 ? std::array<std::uint8_t, 3>{operands.Second, operands.Destination, third}
        : std::array<std::uint8_t, 3>{operands.Second, third, operands.Destination};
    if (operands.Wide) {
        const auto upper = _quarter(body, pool, operands, slots, inputs, true, flags);
        VexEncoding::StackOperand(body, kStoreVector, upper, 0, slots.Upper);
        pool.Give(upper);
    }
    const auto lower = _quarter(body, pool, operands, slots, inputs, false, flags);
    if (operands.Memory) pool.Give(third);
    if (operands.Scalar) {
        Register(body, kBlendFloats, operands.Destination, operands.Destination, lower, std::uint8_t{0x01});
    } else if (operands.Wide) {
        const auto upper = pool.Take();
        VexEncoding::StackOperand(body, kLoadVector, upper, 0, slots.Upper);
        InsertUpper(body, operands.Destination, lower, upper);
        pool.Give(upper);
    } else {
        Register(body, kOr, operands.Destination, lower, lower);
    }
    pool.Give(lower);
    const auto temporary = pool.Take();
    OrReduce(body, flags, temporary);
    RaiseFlags(body, flags, temporary, slots.Saved, slots.Capture);
    pool.Give(temporary);
    pool.Give(flags);

    for (std::size_t index = 0; index < kScratch; ++index)
        VexEncoding::StackOperand(body, MoveUnaligned256Load, scratch[index], 0, static_cast<std::int32_t>(index * 32));
    StackSlot(body, 0x8D, true, Stack, frame);
}

}
