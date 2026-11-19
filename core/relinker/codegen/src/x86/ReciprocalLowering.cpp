#include <codegen/x86/ReciprocalLowering.hpp>
#include <codegen/x86/SharedStubData.hpp>
#include <array>
#include <vector>

namespace Codegen {

namespace {

constexpr std::uint8_t kPacked = 0x66;
constexpr std::uint8_t kScalar = 0xF3;
constexpr std::uint8_t kMovdqa = 0x6F;
constexpr std::uint8_t kMovd = 0x6E;
constexpr std::uint8_t kMovss = 0x10;
constexpr std::uint8_t kMovaps = 0x28;
constexpr std::uint8_t kPand = 0xDB;
constexpr std::uint8_t kPandn = 0xDF;
constexpr std::uint8_t kPor = 0xEB;
constexpr std::uint8_t kPxor = 0xEF;
constexpr std::uint8_t kPsubd = 0xFA;
constexpr std::uint8_t kPcmpgtd = 0x66;
constexpr std::uint8_t kPcmpeqd = 0x76;
constexpr std::uint8_t kShiftRightLogical = 2;
constexpr std::uint8_t kShiftLeft = 6;
constexpr std::uint8_t kRax = 0;
constexpr std::uint8_t kRdx = 2;
constexpr std::uint8_t kHighHalfSlot = 16;

struct Scratch {
    std::uint8_t Input;
    std::uint8_t High;
    std::uint8_t Result;
    std::uint8_t HighResult;
    std::uint8_t Temporary;
    std::uint8_t Mask;
    std::uint8_t Value;
};

std::vector<std::uint8_t> _allocate(const ReciprocalOperands& operands, const std::size_t count) {
    std::vector<std::uint8_t> registers;
    for (std::uint8_t reg = 0; reg < 16 && registers.size() < count; ++reg)
        if (reg != operands.Destination && reg != operands.Upper && (operands.Memory || reg != operands.Source))
            registers.push_back(reg);
    return registers;
}

void _op(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t dst, const std::uint8_t src) {
    body.Sse(kPacked, {0x0F, opcode}, dst, src);
}

void _constant(StubBodyBuilder& body, const std::uint8_t opcode, const std::uint8_t reg, const SharedConstant constant) {
    body.SharedOperand({0x0F, opcode}, reg, SharedConstantOffset(constant));
}

void _blend(StubBodyBuilder& body, const std::uint8_t result, const std::uint8_t value, const std::uint8_t mask) {
    _op(body, kPxor, value, result);
    _op(body, kPand, value, mask);
    _op(body, kPxor, result, value);
}

void _lookup(StubBodyBuilder& body, const std::uint8_t reg) {
    for (std::uint8_t lane = 0; lane < 4; ++lane) {
        body.SseImm(kPacked, {0x0F, 0x3A, 0x16}, reg, kRax, lane);
        body.Raw(std::array<std::uint8_t, 4>{0x0F, 0xB7, 0x04, 0x42});
        body.SseImm(kPacked, {0x0F, 0x3A, 0x22}, reg, kRax, lane);
    }
}

void _mantissaAndExponent(StubBodyBuilder& body, const ReciprocalOperation operation, const std::uint8_t input, const std::uint8_t result, const std::uint8_t temporary, const std::uint8_t mask) {
    const bool squareRoot = operation == ReciprocalOperation::ReciprocalSquareRoot;
    _op(body, kMovdqa, result, input);
    body.ShiftDwordImm(kShiftRightLogical, result, 11);
    _constant(body, kPand, result, squareRoot ? SharedConstant::Index13 : SharedConstant::Index12);
    if (squareRoot)
        _constant(body, kPxor, result, SharedConstant::Bit12);
    _lookup(body, result);
    body.ShiftDwordImm(kShiftLeft, result, 11);
    _op(body, kMovdqa, temporary, input);
    body.ShiftDwordImm(kShiftRightLogical, temporary, 23);
    _constant(body, kPand, temporary, SharedConstant::ExponentMask);
    _constant(body, kMovdqa, mask, squareRoot ? SharedConstant::SquareRootBias : SharedConstant::ReciprocalBias);
    _op(body, kPsubd, mask, temporary);
    if (squareRoot)
        body.ShiftDwordImm(kShiftRightLogical, mask, 1);
    body.ShiftDwordImm(kShiftLeft, mask, 23);
    _op(body, kPor, result, mask);
}

void _reciprocal(StubBodyBuilder& body, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    _mantissaAndExponent(body, ReciprocalOperation::Reciprocal, input, result, s.Temporary, s.Mask);
    _op(body, kMovdqa, s.Mask, input);
    _constant(body, kPand, s.Mask, SharedConstant::Sign);
    _op(body, kPor, result, s.Mask);
    _op(body, kMovdqa, s.Temporary, input);
    _constant(body, kPand, s.Temporary, SharedConstant::Magnitude);
    _op(body, kMovdqa, s.Mask, s.Temporary);
    _constant(body, kPcmpgtd, s.Mask, SharedConstant::ReciprocalOverflow);
    _op(body, kMovdqa, s.Value, input);
    _constant(body, kPand, s.Value, SharedConstant::Sign);
    _blend(body, result, s.Value, s.Mask);
    _constant(body, kMovdqa, s.Mask, SharedConstant::SmallestNormal);
    _op(body, kPcmpgtd, s.Mask, s.Temporary);
    _op(body, kMovdqa, s.Value, input);
    _constant(body, kPand, s.Value, SharedConstant::Sign);
    _constant(body, kPor, s.Value, SharedConstant::Infinity);
    _blend(body, result, s.Value, s.Mask);
    _op(body, kMovdqa, s.Mask, s.Temporary);
    _constant(body, kPcmpgtd, s.Mask, SharedConstant::Infinity);
    _op(body, kMovdqa, s.Value, input);
    _constant(body, kPor, s.Value, SharedConstant::QuietBit);
    _blend(body, result, s.Value, s.Mask);
}

void _reciprocalSquareRoot(StubBodyBuilder& body, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    _mantissaAndExponent(body, ReciprocalOperation::ReciprocalSquareRoot, input, result, s.Temporary, s.Mask);
    _op(body, kMovdqa, s.Mask, input);
    _constant(body, kPcmpeqd, s.Mask, SharedConstant::Infinity);
    _op(body, kPandn, s.Mask, result);
    _op(body, kMovdqa, result, s.Mask);
    _constant(body, kMovdqa, s.Mask, SharedConstant::SmallestNormal);
    _op(body, kPcmpgtd, s.Mask, input);
    _constant(body, kMovdqa, s.Value, SharedConstant::Infinity);
    _blend(body, result, s.Value, s.Mask);
    _op(body, kPxor, s.Mask, s.Mask);
    _op(body, kPcmpgtd, s.Mask, input);
    _constant(body, kMovdqa, s.Value, SharedConstant::DefaultNan);
    _blend(body, result, s.Value, s.Mask);
    _constant(body, kMovdqa, s.Mask, SharedConstant::NegativeSmallestNormal);
    _op(body, kPcmpgtd, s.Mask, input);
    _constant(body, kMovdqa, s.Value, SharedConstant::NegativeInfinity);
    _blend(body, result, s.Value, s.Mask);
    _op(body, kMovdqa, s.Mask, input);
    _constant(body, kPand, s.Mask, SharedConstant::Magnitude);
    _constant(body, kPcmpgtd, s.Mask, SharedConstant::Infinity);
    _op(body, kMovdqa, s.Value, input);
    _constant(body, kPor, s.Value, SharedConstant::QuietBit);
    _blend(body, result, s.Value, s.Mask);
}

void _compute(StubBodyBuilder& body, const ReciprocalOperation operation, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    if (operation == ReciprocalOperation::ReciprocalSquareRoot)
        _reciprocalSquareRoot(body, input, result, s);
    else
        _reciprocal(body, input, result, s);
}

void _vex(StubBodyBuilder& body, const std::uint8_t map, const bool wide, const std::uint8_t pp, const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint8_t rm) {
    body.Raw(std::array<std::uint8_t, 5>{
        0xC4,
        static_cast<std::uint8_t>(((reg & 8) != 0 ? 0x00 : 0x80) | 0x40 | ((rm & 8) != 0 ? 0x00 : 0x20) | map),
        static_cast<std::uint8_t>(((~vvvv & 0x0F) << 3) | (wide ? 0x04 : 0x00) | pp),
        opcode,
        static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))
    });
}

void _extractHighHalf(StubBodyBuilder& body, const std::uint8_t source, const std::uint8_t high) {
    body.Raw(std::array<std::uint8_t, 8>{
        0xC4,
        static_cast<std::uint8_t>(((source & 8) != 0 ? 0x60 : 0xE0) | 0x03),
        0x7D,
        0x19,
        static_cast<std::uint8_t>(0x44 | ((source & 7) << 3)),
        0x24,
        kHighHalfSlot,
        0x01
    });
    body.Raw(std::array<std::uint8_t, 1>{kScalar});
    if ((high & 8) != 0)
        body.Raw(std::array<std::uint8_t, 1>{0x44});
    body.Raw(std::array<std::uint8_t, 5>{0x0F, kMovdqa, static_cast<std::uint8_t>(0x44 | ((high & 7) << 3)), 0x24, kHighHalfSlot});
}

void _loadInputs(StubBodyBuilder& body, const ReciprocalOperands& operands, const Scratch& s) {
    const auto& memory = *operands.Memory;
    if (operands.Scalar)
        body.Load(s.Input, memory, kPacked, kMovd);
    else if (!operands.Vex)
        body.Load(s.Input, memory, kPacked, kMovdqa);
    else
        body.Load(s.Input, memory);
    if (operands.Wide)
        body.Load(s.High, memory, kScalar, kMovdqa, 16);
}

void _store(StubBodyBuilder& body, const ReciprocalOperands& operands, const Scratch& s) {
    const auto dst = operands.Destination;
    if (operands.Wide) {
        _vex(body, 0x03, true, 0x01, 0x18, dst, s.Result, s.HighResult);
        body.Raw(std::array<std::uint8_t, 1>{0x01});
    } else if (operands.Scalar && operands.Vex)
        _vex(body, 0x01, false, 0x02, kMovss, dst, operands.Upper, s.Result);
    else if (operands.Scalar)
        body.Sse(kScalar, {0x0F, kMovss}, dst, s.Result);
    else if (operands.Vex)
        _vex(body, 0x01, false, 0x00, kMovaps, dst, 0, s.Result);
    else
        _op(body, kMovdqa, dst, s.Result);
}

}

void ReciprocalLowering::EmitOutOfLine(StubBodyBuilder& body, const ReciprocalOperands& operands) const {
    const bool loaded = operands.Memory.has_value();
    const auto registers = _allocate(operands, 4 + (loaded ? 1 : 0) + (operands.Wide ? 2 : 0));
    std::size_t next = 0;
    Scratch s{};
    s.Input = loaded ? registers[next++] : operands.Source;
    s.High = operands.Wide ? registers[next++] : s.Input;
    s.Result = registers[next++];
    s.HighResult = operands.Wide ? registers[next++] : s.Result;
    s.Temporary = registers[next++];
    s.Mask = registers[next++];
    s.Value = registers[next++];
    for (const auto reg : registers)
        body.Spill(reg);
    if (loaded)
        _loadInputs(body, operands, s);
    else if (operands.Wide)
        _extractHighHalf(body, operands.Source, s.High);
    body.Push(kRax);
    body.Push(kRdx);
    body.SharedAddress(kRdx, operands.Operation == ReciprocalOperation::ReciprocalSquareRoot ? kReciprocalSquareRootTableOffset : kReciprocalTableOffset);
    _compute(body, operands.Operation, s.Input, s.Result, s);
    if (operands.Wide)
        _compute(body, operands.Operation, s.High, s.HighResult, s);
    body.Pop(kRdx);
    body.Pop(kRax);
    _store(body, operands, s);
    for (auto reg = registers.rbegin(); reg != registers.rend(); ++reg)
        body.Restore(*reg);
}

LoweredBody ReciprocalLowering::LowerOutOfLine(const ReciprocalOperands& operands) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    return body.Finish();
}

}
