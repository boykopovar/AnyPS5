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
constexpr std::uint8_t kMap0F = 0x01;
constexpr std::uint8_t kMap0F3A = 0x03;
constexpr std::uint8_t kPp66 = 0x01;
constexpr std::uint8_t kPpF3 = 0x02;
constexpr std::uint8_t kModRmRegister = 0xC0;
constexpr std::uint8_t kModRmRip = 0x05;

void _vex(StubBodyBuilder& body, const std::uint8_t map, const bool wide, const std::uint8_t pp, const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint8_t rm, const std::uint8_t modrm) {
    body.Raw(std::array<std::uint8_t, 5>{
        0xC4,
        static_cast<std::uint8_t>(((reg & 8) != 0 ? 0x00 : 0x80) | 0x40 | ((rm & 8) != 0 ? 0x00 : 0x20) | map),
        static_cast<std::uint8_t>(((~vvvv & 0x0F) << 3) | (wide ? 0x04 : 0x00) | pp),
        opcode,
        modrm
    });
}

void _vexRegister(StubBodyBuilder& body, const std::uint8_t map, const bool wide, const std::uint8_t pp, const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t vvvv, const std::uint8_t rm) {
    _vex(body, map, wide, pp, opcode, reg, vvvv, rm, static_cast<std::uint8_t>(kModRmRegister | ((reg & 7) << 3) | (rm & 7)));
}

class Encoder {
public:
    Encoder(StubBodyBuilder& body, const bool vex) : body(body), vex(vex) {}

    void Op(const std::uint8_t opcode, const std::uint8_t dst, const std::uint8_t src) const {
        if (vex)
            _vexRegister(body, kMap0F, false, kPp66, opcode, dst, opcode == kMovdqa ? 0 : dst, src);
        else
            body.Sse(kPacked, {0x0F, opcode}, dst, src);
    }

    void Shift(const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) const {
        if (vex) {
            _vex(body, kMap0F, false, kPp66, 0x72, 0, reg, reg, static_cast<std::uint8_t>(kModRmRegister | (extension << 3) | (reg & 7)));
            body.Raw(std::array<std::uint8_t, 1>{imm});
        } else {
            body.ShiftDwordImm(extension, reg, imm);
        }
    }

    void Constant(const std::uint8_t opcode, const std::uint8_t reg, const SharedConstant constant) const {
        const auto offset = SharedConstantOffset(constant);
        if (vex) {
            _vex(body, kMap0F, false, kPp66, opcode, reg, opcode == kMovdqa ? 0 : reg, 0, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRip));
            body.SharedDisplacement(offset);
        } else {
            body.SharedOperand({0x0F, opcode}, reg, offset);
        }
    }

    void Lane(const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t lane) const {
        if (vex) {
            _vexRegister(body, kMap0F3A, false, kPp66, opcode, reg, opcode == 0x22 ? reg : 0, kRax);
            body.Raw(std::array<std::uint8_t, 1>{lane});
        } else {
            body.SseImm(kPacked, {0x0F, 0x3A, opcode}, reg, kRax, lane);
        }
    }

    StubBodyBuilder& Body() const {
        return body;
    }

private:
    StubBodyBuilder& body;
    bool vex;
};

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

void _blend(const Encoder& e, const std::uint8_t result, const std::uint8_t value, const std::uint8_t mask) {
    e.Op(kPxor, value, result);
    e.Op(kPand, value, mask);
    e.Op(kPxor, result, value);
}

void _lookup(const Encoder& e, const std::uint8_t reg) {
    for (std::uint8_t lane = 0; lane < 4; ++lane) {
        e.Lane(0x16, reg, lane);
        e.Body().Raw(std::array<std::uint8_t, 4>{0x0F, 0xB7, 0x04, 0x42});
        e.Lane(0x22, reg, lane);
    }
}

void _mantissaAndExponent(const Encoder& e, const ReciprocalOperation operation, const std::uint8_t input, const std::uint8_t result, const std::uint8_t temporary, const std::uint8_t mask) {
    const bool squareRoot = operation == ReciprocalOperation::ReciprocalSquareRoot;
    e.Op(kMovdqa, result, input);
    e.Shift(kShiftRightLogical, result, 11);
    e.Constant(kPand, result, squareRoot ? SharedConstant::Index13 : SharedConstant::Index12);
    if (squareRoot)
        e.Constant(kPxor, result, SharedConstant::Bit12);
    _lookup(e, result);
    e.Shift(kShiftLeft, result, 11);
    e.Op(kMovdqa, temporary, input);
    e.Shift(kShiftRightLogical, temporary, 23);
    e.Constant(kPand, temporary, SharedConstant::ExponentMask);
    e.Constant(kMovdqa, mask, squareRoot ? SharedConstant::SquareRootBias : SharedConstant::ReciprocalBias);
    e.Op(kPsubd, mask, temporary);
    if (squareRoot)
        e.Shift(kShiftRightLogical, mask, 1);
    e.Shift(kShiftLeft, mask, 23);
    e.Op(kPor, result, mask);
}

void _reciprocal(const Encoder& e, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    _mantissaAndExponent(e, ReciprocalOperation::Reciprocal, input, result, s.Temporary, s.Mask);
    e.Op(kMovdqa, s.Mask, input);
    e.Constant(kPand, s.Mask, SharedConstant::Sign);
    e.Op(kPor, result, s.Mask);
    e.Op(kMovdqa, s.Temporary, input);
    e.Constant(kPand, s.Temporary, SharedConstant::Magnitude);
    e.Op(kMovdqa, s.Mask, s.Temporary);
    e.Constant(kPcmpgtd, s.Mask, SharedConstant::ReciprocalOverflow);
    e.Op(kMovdqa, s.Value, input);
    e.Constant(kPand, s.Value, SharedConstant::Sign);
    _blend(e, result, s.Value, s.Mask);
    e.Constant(kMovdqa, s.Mask, SharedConstant::SmallestNormal);
    e.Op(kPcmpgtd, s.Mask, s.Temporary);
    e.Op(kMovdqa, s.Value, input);
    e.Constant(kPand, s.Value, SharedConstant::Sign);
    e.Constant(kPor, s.Value, SharedConstant::Infinity);
    _blend(e, result, s.Value, s.Mask);
    e.Op(kMovdqa, s.Mask, s.Temporary);
    e.Constant(kPcmpgtd, s.Mask, SharedConstant::Infinity);
    e.Op(kMovdqa, s.Value, input);
    e.Constant(kPor, s.Value, SharedConstant::QuietBit);
    _blend(e, result, s.Value, s.Mask);
}

void _reciprocalSquareRoot(const Encoder& e, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    _mantissaAndExponent(e, ReciprocalOperation::ReciprocalSquareRoot, input, result, s.Temporary, s.Mask);
    e.Op(kMovdqa, s.Mask, input);
    e.Constant(kPcmpeqd, s.Mask, SharedConstant::Infinity);
    e.Op(kPandn, s.Mask, result);
    e.Op(kMovdqa, result, s.Mask);
    e.Constant(kMovdqa, s.Mask, SharedConstant::SmallestNormal);
    e.Op(kPcmpgtd, s.Mask, input);
    e.Constant(kMovdqa, s.Value, SharedConstant::Infinity);
    _blend(e, result, s.Value, s.Mask);
    e.Op(kPxor, s.Mask, s.Mask);
    e.Op(kPcmpgtd, s.Mask, input);
    e.Constant(kMovdqa, s.Value, SharedConstant::DefaultNan);
    _blend(e, result, s.Value, s.Mask);
    e.Constant(kMovdqa, s.Mask, SharedConstant::NegativeSmallestNormal);
    e.Op(kPcmpgtd, s.Mask, input);
    e.Constant(kMovdqa, s.Value, SharedConstant::NegativeInfinity);
    _blend(e, result, s.Value, s.Mask);
    e.Op(kMovdqa, s.Mask, input);
    e.Constant(kPand, s.Mask, SharedConstant::Magnitude);
    e.Constant(kPcmpgtd, s.Mask, SharedConstant::Infinity);
    e.Op(kMovdqa, s.Value, input);
    e.Constant(kPor, s.Value, SharedConstant::QuietBit);
    _blend(e, result, s.Value, s.Mask);
}

void _compute(const Encoder& e, const ReciprocalOperation operation, const std::uint8_t input, const std::uint8_t result, const Scratch& s) {
    if (operation == ReciprocalOperation::ReciprocalSquareRoot)
        _reciprocalSquareRoot(e, input, result, s);
    else
        _reciprocal(e, input, result, s);
}

void _loadInputs(StubBodyBuilder& body, const ReciprocalOperands& operands, const Scratch& s) {
    const auto& memory = *operands.Memory;
    if (operands.Scalar)
        body.Load(s.Input, memory, kPacked, kMovd, 0, operands.Vex);
    else if (!operands.Vex)
        body.Load(s.Input, memory, kPacked, kMovdqa);
    else
        body.Load(s.Input, memory, kScalar, kMovdqa, 0, true);
    if (operands.Wide)
        body.Load(s.High, memory, kScalar, kMovdqa, 16, true);
}

void _store(StubBodyBuilder& body, const ReciprocalOperands& operands, const Scratch& s) {
    const auto dst = operands.Destination;
    if (operands.Wide) {
        _vexRegister(body, kMap0F3A, true, kPp66, 0x18, dst, s.Result, s.HighResult);
        body.Raw(std::array<std::uint8_t, 1>{0x01});
    } else if (operands.Scalar && operands.Vex) {
        _vexRegister(body, kMap0F, false, kPpF3, kMovss, dst, operands.Upper, s.Result);
    } else if (operands.Scalar) {
        body.Sse(kScalar, {0x0F, kMovss}, dst, s.Result);
    } else if (operands.Vex) {
        _vexRegister(body, kMap0F, false, 0x00, kMovaps, dst, 0, s.Result);
    } else {
        body.Sse(kPacked, {0x0F, kMovdqa}, dst, s.Result);
    }
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
        body.Spill(reg, operands.Vex);
    if (loaded)
        _loadInputs(body, operands, s);
    else if (operands.Wide) {
        _vexRegister(body, kMap0F3A, true, kPp66, 0x19, operands.Source, 0, s.High);
        body.Raw(std::array<std::uint8_t, 1>{0x01});
    }
    body.Push(kRax);
    body.Push(kRdx);
    body.SharedAddress(kRdx, operands.Operation == ReciprocalOperation::ReciprocalSquareRoot ? kReciprocalSquareRootTableOffset : kReciprocalTableOffset);
    const Encoder e(body, operands.Vex);
    _compute(e, operands.Operation, s.Input, s.Result, s);
    if (operands.Wide)
        _compute(e, operands.Operation, s.High, s.HighResult, s);
    body.Pop(kRdx);
    body.Pop(kRax);
    _store(body, operands, s);
    for (auto reg = registers.rbegin(); reg != registers.rend(); ++reg)
        body.Restore(*reg, operands.Vex);
}

LoweredBody ReciprocalLowering::LowerOutOfLine(const ReciprocalOperands& operands) const {
    StubBodyBuilder body;
    EmitOutOfLine(body, operands);
    return body.Finish();
}

}
