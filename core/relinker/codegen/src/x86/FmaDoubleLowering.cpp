#include <codegen/x86/FmaLowering.hpp>
#include <codegen/x86/ScalarLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace Codegen {

namespace {

enum Gpr : std::uint8_t { Rax, Rcx, Rdx, Rbx, Rsp, Rbp, Rsi, Rdi, R8, R9, R10, R11, R12, R13, R14, R15 };

enum class Condition : std::uint8_t {
    Below = 0x2,
    AboveOrEqual = 0x3,
    Equal = 0x4,
    NotEqual = 0x5,
    BelowOrEqual = 0x6,
    Above = 0x7,
    Less = 0xC,
    GreaterOrEqual = 0xD,
    Greater = 0xF,
};

enum class Arithmetic : std::uint8_t { Add = 0, Or = 1, Adc = 2, Sbb = 3, And = 4, Sub = 5, Xor = 6, Cmp = 7 };

enum class Shift : std::uint8_t { Left = 4, Right = 5 };

enum class BitTest : std::uint8_t { Set = 5, Complement = 7 };

constexpr std::int32_t kRedZone = 128;
constexpr std::int32_t kSavedGprs = 0;
constexpr std::int32_t kSavedFlags = 120;
constexpr std::int32_t kControl = 128;
constexpr std::int32_t kNewControl = 132;
constexpr std::int32_t kLane = 136;
constexpr std::int32_t kRaised = 144;
constexpr std::int32_t kScratchVector = 160;
constexpr std::int32_t kDestinationSlot = 192;
constexpr std::int32_t kSecondSlot = 224;
constexpr std::int32_t kThirdSlot = 256;
constexpr std::int32_t kResultSlot = 288;
constexpr std::int32_t kFrame = kRedZone + 320;
constexpr std::uint64_t kInfinity = 0x7FF0000000000000ull;
constexpr std::uint64_t kLargest = 0x7FEFFFFFFFFFFFFFull;
constexpr std::uint64_t kDefaultNan = 0xFFF8000000000000ull;
constexpr std::uint64_t kQuiet = 0x0008000000000000ull;
constexpr std::uint64_t kFraction = 0x000FFFFFFFFFFFFFull;
constexpr std::uint64_t kShiftedInfinity = 0xFFE0000000000000ull;
constexpr std::uint64_t kShiftedSmallestNormal = 0x0020000000000000ull;
constexpr std::int32_t kInvalidFlag = 0x01;
constexpr std::int32_t kDenormalFlag = 0x02;
constexpr std::int32_t kOverflowFlags = 0x28;
constexpr std::int32_t kUnderflowFlags = 0x30;
constexpr std::int32_t kPrecisionFlag = 0x20;
constexpr std::int32_t kDenormalsAreZero = 0x40;
constexpr std::int32_t kFlushToZero = 0x8000;

constexpr VexEncoding::Form kLoadScalarDouble{VexEncoding::Map0F, VexEncoding::PrefixF2, false, false, 0x10};
constexpr VexEncoding::Form kLoadDoubles{VexEncoding::Map0F, VexEncoding::Prefix66, false, false, 0x10};
constexpr VexEncoding::Form kLoadDoublesWide{VexEncoding::Map0F, VexEncoding::Prefix66, false, true, 0x10};

std::uint8_t _rex(const bool wide, const std::uint8_t reg, const std::uint8_t index, const std::uint8_t rm) {
    return static_cast<std::uint8_t>(0x40 | (wide ? 8 : 0) | ((reg & 8) >> 1) | ((index & 8) >> 2) | ((rm & 8) >> 3));
}

class Assembler {
public:
    using Label = std::size_t;

    Label NewLabel() {
        _labels.push_back(std::nullopt);
        return _labels.size() - 1;
    }

    void Bind(const Label label) {
        if (_labels.at(label)) throw CodegenException("FMA lowering bound a label twice");
        _labels[label] = _bytes.size();
    }

    void Jump(const Label label) {
        _bytes.push_back(0xE9);
        _fixup(label);
    }

    void JumpIf(const Condition condition, const Label label) {
        _bytes.insert(_bytes.end(), {0x0F, static_cast<std::uint8_t>(0x80 | static_cast<std::uint8_t>(condition))});
        _fixup(label);
    }

    void Move(const Gpr destination, const Gpr source) { _registers(0x8B, destination, source); }

    void MoveImmediate(const Gpr destination, const std::uint64_t value) {
        _bytes.push_back(_rex(true, 0, 0, destination));
        _bytes.push_back(static_cast<std::uint8_t>(0xB8 | (destination & 7)));
        for (unsigned shift = 0; shift < 64; shift += 8) _bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }

    void Load(const Gpr destination, const std::int32_t displacement) { _stack(0x8B, true, destination, displacement); }
    void Load32(const Gpr destination, const std::int32_t displacement) { _stack(0x8B, false, destination, displacement); }
    void Store(const std::int32_t displacement, const Gpr source) { _stack(0x89, true, source, displacement); }
    void Store32(const std::int32_t displacement, const Gpr source) { _stack(0x89, false, source, displacement); }
    void OperateStack(const Arithmetic operation, const Gpr destination, const std::int32_t displacement) {
        _stack(static_cast<std::uint8_t>((static_cast<std::uint8_t>(operation) << 3) | 3), true, destination, displacement);
    }
    void OrIntoStack(const std::int32_t displacement, const Gpr source) { _stack(0x09, true, source, displacement); }
    void LoadLane(const Gpr destination, const Gpr index, const std::int32_t displacement) { _lane(0x8B, destination, index, displacement); }
    void StoreLane(const std::int32_t displacement, const Gpr index, const Gpr source) { _lane(0x89, source, index, displacement); }

    void Operate(const Arithmetic operation, const Gpr destination, const Gpr source) {
        _registers(static_cast<std::uint8_t>((static_cast<std::uint8_t>(operation) << 3) | 3), destination, source);
    }

    void OperateImmediate(const Arithmetic operation, const Gpr destination, const std::int32_t value) {
        _bytes.insert(_bytes.end(), {_rex(true, 0, 0, destination), 0x81, static_cast<std::uint8_t>(0xC0 | (static_cast<std::uint8_t>(operation) << 3) | (destination & 7))});
        _immediate32(value);
    }

    void Test(const Gpr left, const Gpr right) { _registers(0x85, right, left); }

    void TestImmediate(const Gpr reg, const std::int32_t value) {
        _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xF7, static_cast<std::uint8_t>(0xC0 | (reg & 7))});
        _immediate32(value);
    }

    void ShiftImmediate(const Shift kind, const Gpr reg, const std::uint8_t count) {
        _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xC1, static_cast<std::uint8_t>(0xC0 | (static_cast<std::uint8_t>(kind) << 3) | (reg & 7)), count});
    }

    void ShiftByCount(const Shift kind, const Gpr reg) {
        _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xD3, static_cast<std::uint8_t>(0xC0 | (static_cast<std::uint8_t>(kind) << 3) | (reg & 7))});
    }

    void ShiftLeftDouble(const Gpr destination, const Gpr source) { _extended(0xA5, source, destination); }
    void ShiftRightDouble(const Gpr destination, const Gpr source) { _extended(0xAD, source, destination); }

    void ShiftRightDoubleImmediate(const Gpr destination, const Gpr source, const std::uint8_t count) {
        _extended(0xAC, source, destination);
        _bytes.push_back(count);
    }

    void BitScanReverse(const Gpr destination, const Gpr source) { _extended(0xBD, destination, source); }

    void BitTestImmediate(const BitTest kind, const Gpr reg, const std::uint8_t bit) {
        _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0x0F, 0xBA, static_cast<std::uint8_t>(0xC0 | (static_cast<std::uint8_t>(kind) << 3) | (reg & 7)), bit});
    }

    void Multiply(const Gpr reg) { _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xF7, static_cast<std::uint8_t>(0xE0 | (reg & 7))}); }
    void Negate(const Gpr reg) { _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xF7, static_cast<std::uint8_t>(0xD8 | (reg & 7))}); }
    void Not(const Gpr reg) { _bytes.insert(_bytes.end(), {_rex(true, 0, 0, reg), 0xF7, static_cast<std::uint8_t>(0xD0 | (reg & 7))}); }
    void Exchange(const Gpr left, const Gpr right) { _registers(0x87, left, right); }
    void Zero(const Gpr reg) { Operate(Arithmetic::Xor, reg, reg); }

    void PushFlagsInto(const Gpr reg) {
        _bytes.push_back(0x9C);
        if (reg >= R8) _bytes.push_back(0x41);
        _bytes.push_back(static_cast<std::uint8_t>(0x58 | (reg & 7)));
    }

    void PopFlagsFrom(const Gpr reg) {
        if (reg >= R8) _bytes.push_back(0x41);
        _bytes.push_back(static_cast<std::uint8_t>(0x50 | (reg & 7)));
        _bytes.push_back(0x9D);
    }

    void Control(const std::uint8_t extension, const std::int32_t displacement) {
        _bytes.insert(_bytes.end(), {0xC5, 0xF8, 0xAE, static_cast<std::uint8_t>(0x84 | (extension << 3)), 0x24});
        _immediate32(displacement);
    }

    [[nodiscard]] std::vector<std::uint8_t> Finish() const {
        auto bytes = _bytes;
        for (const auto& [offset, label] : _fixups) {
            if (!_labels.at(label)) throw CodegenException("FMA lowering jumps to an unbound label");
            const auto relative = static_cast<std::int64_t>(*_labels[label]) - static_cast<std::int64_t>(offset + 4);
            for (unsigned shift = 0; shift < 32; shift += 8) bytes[offset + shift / 8] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(relative) >> shift);
        }
        return bytes;
    }

private:
    void _fixup(const Label label) {
        _fixups.emplace_back(_bytes.size(), label);
        _bytes.insert(_bytes.end(), 4, 0);
    }

    void _immediate32(const std::int32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8) _bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> shift));
    }

    void _registers(const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t rm) {
        _bytes.insert(_bytes.end(), {_rex(true, reg, 0, rm), opcode, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))});
    }

    void _extended(const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t rm) {
        _bytes.insert(_bytes.end(), {_rex(true, reg, 0, rm), 0x0F, opcode, static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7))});
    }

    void _stack(const std::uint8_t opcode, const bool wide, const std::uint8_t reg, const std::int32_t displacement) {
        _bytes.insert(_bytes.end(), {_rex(wide, reg, 0, Rsp), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), 0x24});
        _immediate32(displacement);
    }

    void _lane(const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t index, const std::int32_t displacement) {
        _bytes.insert(_bytes.end(), {_rex(true, reg, index, Rsp), opcode, static_cast<std::uint8_t>(0x84 | ((reg & 7) << 3)), static_cast<std::uint8_t>(0xC4 | ((index & 7) << 3))});
        _immediate32(displacement);
    }

    std::vector<std::uint8_t> _bytes;
    std::vector<std::optional<std::size_t>> _labels;
    std::vector<std::pair<std::size_t, Label>> _fixups;
};

constexpr std::array<Gpr, 15> kSaved{Rax, Rcx, Rdx, Rbx, Rbp, Rsi, Rdi, R8, R9, R10, R11, R12, R13, R14, R15};

void _shiftLeftWide(Assembler& code, const Gpr high, const Gpr low) {
    const auto far = code.NewLabel();
    const auto end = code.NewLabel();
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 64);
    code.JumpIf(Condition::AboveOrEqual, far);
    code.ShiftLeftDouble(high, low);
    code.ShiftByCount(Shift::Left, low);
    code.Jump(end);
    code.Bind(far);
    code.Move(high, low);
    code.ShiftByCount(Shift::Left, high);
    code.Zero(low);
    code.Bind(end);
}

void _shiftRightSticky(Assembler& code, const Gpr high, const Gpr low, const Gpr lost, const Gpr temporary, const Gpr other) {
    const auto word = code.NewLabel();
    const auto all = code.NewLabel();
    const auto end = code.NewLabel();
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 128);
    code.JumpIf(Condition::AboveOrEqual, all);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 64);
    code.JumpIf(Condition::AboveOrEqual, word);
    code.Move(lost, low);
    code.Move(temporary, low);
    code.ShiftByCount(Shift::Right, temporary);
    code.ShiftByCount(Shift::Left, temporary);
    code.Operate(Arithmetic::Xor, lost, temporary);
    code.ShiftRightDouble(low, high);
    code.ShiftByCount(Shift::Right, high);
    code.Jump(end);
    code.Bind(word);
    code.Move(lost, low);
    code.Move(low, high);
    code.Zero(high);
    code.Move(temporary, low);
    code.ShiftByCount(Shift::Right, low);
    code.Move(other, low);
    code.ShiftByCount(Shift::Left, other);
    code.Operate(Arithmetic::Xor, other, temporary);
    code.Operate(Arithmetic::Or, lost, other);
    code.Jump(end);
    code.Bind(all);
    code.Move(lost, high);
    code.Operate(Arithmetic::Or, lost, low);
    code.Zero(high);
    code.Zero(low);
    code.Bind(end);
}

void _roundingMode(Assembler& code) {
    code.Move(Rcx, R15);
    code.ShiftImmediate(Shift::Right, Rcx, 13);
    code.OperateImmediate(Arithmetic::And, Rcx, 3);
}

void _roundAt(Assembler& code) {
    const auto modeDown = code.NewLabel();
    const auto modeUp = code.NewLabel();
    const auto up = code.NewLabel();
    const auto done = code.NewLabel();
    code.Move(Rsi, Rdx);
    code.Move(Rdi, Rax);
    code.OperateImmediate(Arithmetic::Sub, Rcx, 1);
    _shiftRightSticky(code, Rsi, Rdi, R11, R8, R9);
    code.Move(R9, Rdi);
    code.OperateImmediate(Arithmetic::And, R9, 1);
    code.ShiftRightDoubleImmediate(Rdi, Rsi, 1);
    code.Move(Rbx, Rdi);
    code.Move(R13, R9);
    const auto exact = code.NewLabel();
    code.Test(R11, R11);
    code.JumpIf(Condition::Equal, exact);
    code.OperateImmediate(Arithmetic::Or, R13, 1);
    code.Bind(exact);
    _roundingMode(code);
    code.Test(Rcx, Rcx);
    code.JumpIf(Condition::NotEqual, modeDown);
    code.Test(R9, R9);
    code.JumpIf(Condition::Equal, done);
    code.Test(R11, R11);
    code.JumpIf(Condition::NotEqual, up);
    code.TestImmediate(Rbx, 1);
    code.JumpIf(Condition::NotEqual, up);
    code.Jump(done);
    code.Bind(modeDown);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 1);
    code.JumpIf(Condition::NotEqual, modeUp);
    code.Test(R13, R13);
    code.JumpIf(Condition::Equal, done);
    code.Test(R12, R12);
    code.JumpIf(Condition::NotEqual, up);
    code.Jump(done);
    code.Bind(modeUp);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 2);
    code.JumpIf(Condition::NotEqual, done);
    code.Test(R13, R13);
    code.JumpIf(Condition::Equal, done);
    code.Test(R12, R12);
    code.JumpIf(Condition::NotEqual, done);
    code.Bind(up);
    code.OperateImmediate(Arithmetic::Add, Rbx, 1);
    code.Bind(done);
}

void _parts(Assembler& code, const Gpr value, const Gpr exponent) {
    const auto normal = code.NewLabel();
    const auto done = code.NewLabel();
    code.Move(exponent, value);
    code.ShiftImmediate(Shift::Right, exponent, 52);
    code.OperateImmediate(Arithmetic::And, exponent, 0x7FF);
    code.Operate(Arithmetic::And, value, Rdx);
    code.Test(exponent, exponent);
    code.JumpIf(Condition::NotEqual, normal);
    code.MoveImmediate(exponent, 1);
    code.Jump(done);
    code.Bind(normal);
    code.BitTestImmediate(BitTest::Set, value, 52);
    code.Bind(done);
}

void _checkNan(Assembler& code, const Gpr value, const Assembler::Label found) {
    code.Move(Rax, value);
    code.Operate(Arithmetic::Add, Rax, Rax);
    code.Operate(Arithmetic::Cmp, Rax, R8);
    code.JumpIf(Condition::Above, found);
}

void _signalingNan(Assembler& code, const Gpr value) {
    const auto skip = code.NewLabel();
    code.Move(Rax, value);
    code.Operate(Arithmetic::Add, Rax, Rax);
    code.Operate(Arithmetic::Cmp, Rax, R8);
    code.JumpIf(Condition::BelowOrEqual, skip);
    code.Move(Rax, value);
    code.Operate(Arithmetic::And, Rax, R9);
    code.JumpIf(Condition::NotEqual, skip);
    code.OperateImmediate(Arithmetic::Or, R14, kInvalidFlag);
    code.Bind(skip);
}

void _denormal(Assembler& code, const Gpr value) {
    const auto skip = code.NewLabel();
    const auto report = code.NewLabel();
    code.Move(Rax, value);
    code.Operate(Arithmetic::Add, Rax, Rax);
    code.JumpIf(Condition::Equal, skip);
    code.Operate(Arithmetic::Cmp, Rax, R9);
    code.JumpIf(Condition::AboveOrEqual, skip);
    code.TestImmediate(R15, kDenormalsAreZero);
    code.JumpIf(Condition::Equal, report);
    code.ShiftImmediate(Shift::Right, value, 63);
    code.ShiftImmediate(Shift::Left, value, 63);
    code.Jump(skip);
    code.Bind(report);
    code.OperateImmediate(Arithmetic::Or, R14, kDenormalFlag);
    code.Bind(skip);
}

void _lane(Assembler& code, const FmaOperands& operands, const std::array<std::int32_t, 3>& slots, const Assembler::Label next) {
    const auto nanFirst = code.NewLabel();
    const auto nanSecond = code.NewLabel();
    const auto nanThird = code.NewLabel();
    const auto nan = code.NewLabel();
    const auto ordered = code.NewLabel();
    const auto invalid = code.NewLabel();
    const auto infiniteProduct = code.NewLabel();
    const auto infiniteResult = code.NewLabel();
    const auto finiteProduct = code.NewLabel();
    const auto finite = code.NewLabel();
    const auto zero = code.NewLabel();
    const auto productNonzero = code.NewLabel();
    const auto addendLarger = code.NewLabel();
    const auto align = code.NewLabel();
    const auto alignRight = code.NewLabel();
    const auto aligned = code.NewLabel();
    const auto subtract = code.NewLabel();
    const auto noBorrow = code.NewLabel();
    const auto round = code.NewLabel();
    const auto lowLength = code.NewLabel();
    const auto measured = code.NewLabel();
    const auto inexactFirst = code.NewLabel();
    const auto rounded = code.NewLabel();
    const auto tiny = code.NewLabel();
    const auto noCarry = code.NewLabel();
    const auto overflow = code.NewLabel();
    const auto denormal = code.NewLabel();
    const auto denormalRound = code.NewLabel();
    const auto denormalDone = code.NewLabel();
    const auto done = code.NewLabel();

    code.Load(Rbp, kLane);
    code.LoadLane(Rbx, Rbp, slots[0]);
    code.LoadLane(Rsi, Rbp, slots[1]);
    code.LoadLane(Rdi, Rbp, slots[2]);
    code.Zero(R14);
    code.Load32(R15, kControl);
    code.MoveImmediate(R8, kShiftedInfinity);
    code.MoveImmediate(R9, kQuiet);

    _checkNan(code, Rbx, nanFirst);
    _checkNan(code, Rsi, nanSecond);
    _checkNan(code, Rdi, nanThird);
    code.Jump(ordered);
    code.Bind(nanFirst);
    code.Move(Rdx, Rbx);
    code.Jump(nan);
    code.Bind(nanSecond);
    code.Move(Rdx, Rsi);
    code.Jump(nan);
    code.Bind(nanThird);
    code.Move(Rdx, Rdi);
    code.Bind(nan);
    _signalingNan(code, Rbx);
    _signalingNan(code, Rsi);
    _signalingNan(code, Rdi);
    code.Move(Rax, Rdx);
    code.Operate(Arithmetic::Or, Rax, R9);
    code.Jump(done);

    code.Bind(ordered);
    code.MoveImmediate(R9, kShiftedSmallestNormal);
    _denormal(code, Rbx);
    _denormal(code, Rsi);
    _denormal(code, Rdi);
    if (operands.Operation == FmaOperation::NegatedMultiplyAdd || operands.Operation == FmaOperation::NegatedMultiplySubtract)
        code.BitTestImmediate(BitTest::Complement, Rbx, 63);
    if (operands.Operation == FmaOperation::MultiplySubtract || operands.Operation == FmaOperation::NegatedMultiplySubtract) {
        code.BitTestImmediate(BitTest::Complement, Rdi, 63);
    } else if (operands.Operation == FmaOperation::AddSubtract || operands.Operation == FmaOperation::SubtractAdd) {
        const auto kept = code.NewLabel();
        code.TestImmediate(Rbp, 1);
        code.JumpIf(operands.Operation == FmaOperation::AddSubtract ? Condition::NotEqual : Condition::Equal, kept);
        code.BitTestImmediate(BitTest::Complement, Rdi, 63);
        code.Bind(kept);
    }
    code.Move(R12, Rbx);
    code.Operate(Arithmetic::Xor, R12, Rsi);
    code.ShiftImmediate(Shift::Right, R12, 63);
    code.Move(R13, Rdi);
    code.ShiftImmediate(Shift::Right, R13, 63);

    code.Move(R10, Rbx);
    code.Operate(Arithmetic::Add, R10, R10);
    code.Move(R11, Rsi);
    code.Operate(Arithmetic::Add, R11, R11);
    code.Move(Rcx, Rdi);
    code.Operate(Arithmetic::Add, Rcx, Rcx);
    const auto secondInfinite = code.NewLabel();
    code.Operate(Arithmetic::Cmp, R10, R8);
    code.JumpIf(Condition::NotEqual, secondInfinite);
    code.Test(R11, R11);
    code.JumpIf(Condition::Equal, invalid);
    code.Jump(infiniteProduct);
    code.Bind(secondInfinite);
    code.Operate(Arithmetic::Cmp, R11, R8);
    code.JumpIf(Condition::NotEqual, finiteProduct);
    code.Test(R10, R10);
    code.JumpIf(Condition::Equal, invalid);
    code.Bind(infiniteProduct);
    code.Operate(Arithmetic::Cmp, Rcx, R8);
    code.JumpIf(Condition::NotEqual, infiniteResult);
    code.Operate(Arithmetic::Cmp, R12, R13);
    code.JumpIf(Condition::NotEqual, invalid);
    code.Bind(infiniteResult);
    code.Move(Rax, R12);
    code.ShiftImmediate(Shift::Left, Rax, 63);
    code.MoveImmediate(Rcx, kInfinity);
    code.Operate(Arithmetic::Or, Rax, Rcx);
    code.Jump(done);
    code.Bind(invalid);
    code.MoveImmediate(Rax, kDefaultNan);
    code.MoveImmediate(R14, kInvalidFlag);
    code.Jump(done);
    code.Bind(finiteProduct);
    code.Operate(Arithmetic::Cmp, Rcx, R8);
    code.JumpIf(Condition::NotEqual, finite);
    code.Move(Rax, Rdi);
    code.Jump(done);

    code.Bind(finite);
    code.MoveImmediate(Rdx, kFraction);
    _parts(code, Rbx, R10);
    _parts(code, Rsi, R11);
    _parts(code, Rdi, R9);
    code.Move(Rax, Rbx);
    code.Multiply(Rsi);
    code.Operate(Arithmetic::Add, R10, R11);
    code.OperateImmediate(Arithmetic::Sub, R10, 1075);
    code.Move(R8, Rdx);
    code.Operate(Arithmetic::Or, R8, Rax);
    code.JumpIf(Condition::NotEqual, productNonzero);
    code.Test(Rdi, Rdi);
    code.JumpIf(Condition::Equal, zero);
    code.Zero(Rdx);
    code.Move(Rax, Rdi);
    code.Move(R10, R9);
    code.Move(R12, R13);
    code.Jump(round);

    code.Bind(productNonzero);
    code.Test(Rdi, Rdi);
    code.JumpIf(Condition::Equal, round);
    const auto productLow = code.NewLabel();
    const auto productMeasured = code.NewLabel();
    code.Test(Rdx, Rdx);
    code.JumpIf(Condition::Equal, productLow);
    code.BitScanReverse(R8, Rdx);
    code.OperateImmediate(Arithmetic::Add, R8, 65);
    code.Jump(productMeasured);
    code.Bind(productLow);
    code.BitScanReverse(R8, Rax);
    code.OperateImmediate(Arithmetic::Add, R8, 1);
    code.Bind(productMeasured);
    code.BitScanReverse(R11, Rdi);
    code.OperateImmediate(Arithmetic::Add, R11, 1);
    code.Move(Rbx, R10);
    code.Operate(Arithmetic::Add, Rbx, R8);
    code.Move(Rsi, R9);
    code.Operate(Arithmetic::Add, Rsi, R11);
    code.Operate(Arithmetic::Cmp, Rbx, Rsi);
    code.JumpIf(Condition::Less, addendLarger);
    code.Move(Rbx, Rdx);
    code.Move(Rsi, Rax);
    code.Zero(Rdx);
    code.Move(Rax, Rdi);
    code.Jump(align);
    code.Bind(addendLarger);
    code.Zero(Rbx);
    code.Move(Rsi, Rdi);
    code.Exchange(R10, R9);
    code.Move(R8, R11);
    code.Exchange(R12, R13);

    code.Bind(align);
    code.MoveImmediate(Rcx, 127);
    code.Operate(Arithmetic::Sub, Rcx, R8);
    _shiftLeftWide(code, Rbx, Rsi);
    code.Operate(Arithmetic::Sub, R10, Rcx);
    code.Move(Rcx, R9);
    code.Operate(Arithmetic::Sub, Rcx, R10);
    code.JumpIf(Condition::Less, alignRight);
    _shiftLeftWide(code, Rdx, Rax);
    code.Jump(aligned);
    code.Bind(alignRight);
    code.Negate(Rcx);
    _shiftRightSticky(code, Rdx, Rax, R11, R8, R9);
    const auto exactAlignment = code.NewLabel();
    code.Test(R11, R11);
    code.JumpIf(Condition::Equal, exactAlignment);
    code.OperateImmediate(Arithmetic::Or, Rax, 1);
    code.Bind(exactAlignment);
    code.Bind(aligned);
    code.Operate(Arithmetic::Cmp, R12, R13);
    code.JumpIf(Condition::NotEqual, subtract);
    code.Operate(Arithmetic::Add, Rsi, Rax);
    code.Operate(Arithmetic::Adc, Rbx, Rdx);
    code.Move(Rdx, Rbx);
    code.Move(Rax, Rsi);
    code.Jump(round);
    code.Bind(subtract);
    code.Operate(Arithmetic::Sub, Rsi, Rax);
    code.Operate(Arithmetic::Sbb, Rbx, Rdx);
    code.JumpIf(Condition::AboveOrEqual, noBorrow);
    code.Not(Rsi);
    code.Not(Rbx);
    code.OperateImmediate(Arithmetic::Add, Rsi, 1);
    code.OperateImmediate(Arithmetic::Adc, Rbx, 0);
    code.Move(R12, R13);
    code.Bind(noBorrow);
    code.Move(Rdx, Rbx);
    code.Move(Rax, Rsi);
    code.Move(Rcx, Rdx);
    code.Operate(Arithmetic::Or, Rcx, Rax);
    code.JumpIf(Condition::NotEqual, round);

    code.Bind(zero);
    const auto zeroBoth = code.NewLabel();
    const auto zeroSigned = code.NewLabel();
    _roundingMode(code);
    code.Move(Rax, R12);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 1);
    code.JumpIf(Condition::NotEqual, zeroBoth);
    code.Operate(Arithmetic::Or, Rax, R13);
    code.Jump(zeroSigned);
    code.Bind(zeroBoth);
    code.Operate(Arithmetic::And, Rax, R13);
    code.Bind(zeroSigned);
    code.ShiftImmediate(Shift::Left, Rax, 63);
    code.Jump(done);

    code.Bind(round);
    code.Test(Rdx, Rdx);
    code.JumpIf(Condition::Equal, lowLength);
    code.BitScanReverse(R8, Rdx);
    code.OperateImmediate(Arithmetic::Add, R8, 65);
    code.Jump(measured);
    code.Bind(lowLength);
    code.BitScanReverse(R8, Rax);
    code.OperateImmediate(Arithmetic::Add, R8, 1);
    code.Bind(measured);
    code.Move(Rbp, R10);
    code.Operate(Arithmetic::Add, Rbp, R8);
    code.OperateImmediate(Arithmetic::Sub, Rbp, 53);
    code.OperateImmediate(Arithmetic::Cmp, R8, 53);
    code.JumpIf(Condition::Above, inexactFirst);
    code.MoveImmediate(Rcx, 53);
    code.Operate(Arithmetic::Sub, Rcx, R8);
    code.Move(Rbx, Rax);
    code.ShiftByCount(Shift::Left, Rbx);
    code.Zero(R13);
    code.Jump(rounded);
    code.Bind(inexactFirst);
    code.Move(Rcx, R8);
    code.OperateImmediate(Arithmetic::Sub, Rcx, 53);
    _roundAt(code);
    code.Bind(rounded);
    code.Move(Rcx, Rbx);
    code.ShiftImmediate(Shift::Right, Rcx, 53);
    code.Operate(Arithmetic::Add, Rcx, Rbp);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 1);
    code.JumpIf(Condition::Less, tiny);
    code.Move(Rcx, Rbx);
    code.ShiftImmediate(Shift::Right, Rcx, 53);
    code.JumpIf(Condition::Equal, noCarry);
    code.ShiftImmediate(Shift::Right, Rbx, 1);
    code.OperateImmediate(Arithmetic::Add, Rbp, 1);
    code.Bind(noCarry);
    code.OperateImmediate(Arithmetic::Cmp, Rbp, 2047);
    code.JumpIf(Condition::GreaterOrEqual, overflow);
    code.Move(Rax, Rbp);
    code.ShiftImmediate(Shift::Left, Rax, 52);
    code.MoveImmediate(Rcx, kFraction);
    code.Operate(Arithmetic::And, Rbx, Rcx);
    code.Operate(Arithmetic::Or, Rax, Rbx);
    code.Move(Rcx, R12);
    code.ShiftImmediate(Shift::Left, Rcx, 63);
    code.Operate(Arithmetic::Or, Rax, Rcx);
    code.Test(R13, R13);
    code.JumpIf(Condition::Equal, done);
    code.OperateImmediate(Arithmetic::Or, R14, kPrecisionFlag);
    code.Jump(done);

    code.Bind(overflow);
    const auto infinite = code.NewLabel();
    const auto largest = code.NewLabel();
    const auto overflowUp = code.NewLabel();
    const auto overflowSigned = code.NewLabel();
    code.OperateImmediate(Arithmetic::Or, R14, kOverflowFlags);
    _roundingMode(code);
    code.Test(Rcx, Rcx);
    code.JumpIf(Condition::Equal, infinite);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 1);
    code.JumpIf(Condition::NotEqual, overflowUp);
    code.Test(R12, R12);
    code.JumpIf(Condition::NotEqual, infinite);
    code.Jump(largest);
    code.Bind(overflowUp);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 2);
    code.JumpIf(Condition::NotEqual, largest);
    code.Test(R12, R12);
    code.JumpIf(Condition::Equal, infinite);
    code.Bind(largest);
    code.MoveImmediate(Rax, kLargest);
    code.Jump(overflowSigned);
    code.Bind(infinite);
    code.MoveImmediate(Rax, kInfinity);
    code.Bind(overflowSigned);
    code.Move(Rcx, R12);
    code.ShiftImmediate(Shift::Left, Rcx, 63);
    code.Operate(Arithmetic::Or, Rax, Rcx);
    code.Jump(done);

    code.Bind(tiny);
    code.TestImmediate(R15, kFlushToZero);
    code.JumpIf(Condition::Equal, denormal);
    code.Move(Rax, R12);
    code.ShiftImmediate(Shift::Left, Rax, 63);
    code.OperateImmediate(Arithmetic::Or, R14, kUnderflowFlags);
    code.Jump(done);
    code.Bind(denormal);
    code.MoveImmediate(Rcx, 1);
    code.Operate(Arithmetic::Sub, Rcx, R10);
    code.OperateImmediate(Arithmetic::Cmp, Rcx, 0);
    code.JumpIf(Condition::Greater, denormalRound);
    code.Negate(Rcx);
    code.Move(Rbx, Rax);
    code.ShiftByCount(Shift::Left, Rbx);
    code.Zero(R13);
    code.Jump(denormalDone);
    code.Bind(denormalRound);
    _roundAt(code);
    code.Bind(denormalDone);
    const auto denormalExact = code.NewLabel();
    code.Test(R13, R13);
    code.JumpIf(Condition::Equal, denormalExact);
    code.OperateImmediate(Arithmetic::Or, R14, kUnderflowFlags);
    code.Bind(denormalExact);
    code.Move(Rax, R12);
    code.ShiftImmediate(Shift::Left, Rax, 63);
    code.Operate(Arithmetic::Or, Rax, Rbx);

    code.Bind(done);
    code.Load(Rbp, kLane);
    code.StoreLane(kResultSlot, Rbp, Rax);
    code.OrIntoStack(kRaised, R14);
    code.OperateImmediate(Arithmetic::Add, Rbp, 1);
    code.Store(kLane, Rbp);
    code.OperateImmediate(Arithmetic::Cmp, Rbp, operands.Scalar ? 1 : operands.Wide ? 4 : 2);
    code.JumpIf(Condition::Below, next);
}

}

void EmitFmaDouble(StubBodyBuilder& body, const FmaOperands& operands) {
    std::array<bool, 16> used{};
    used[operands.Destination] = true;
    used[operands.Second] = true;
    if (!operands.Memory) used[operands.Third] = true;
    std::uint8_t scratch = 15;
    while (used[scratch]) --scratch;

    ScalarLowering::StackSlot(body, 0x8D, true, ScalarLowering::Stack, -kFrame);
    Assembler save;
    for (std::size_t index = 0; index < kSaved.size(); ++index) save.Store(kSavedGprs + static_cast<std::int32_t>(index * 8), kSaved[index]);
    body.Raw(save.Finish());

    if (operands.Memory) {
        VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Store, scratch, 0, kScratchVector);
        VexEncoding::Memory(body, operands.Scalar ? kLoadScalarDouble : operands.Wide ? kLoadDoublesWide : kLoadDoubles, scratch, 0, *operands.Memory, kFrame, operands.Name);
        VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Store, scratch, 0, kThirdSlot);
    } else {
        VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Store, operands.Third, 0, kThirdSlot);
    }
    VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Store, operands.Destination, 0, kDestinationSlot);
    VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Store, operands.Second, 0, kSecondSlot);

    const auto slots = operands.Order == FmaOrder::Order132 ? std::array<std::int32_t, 3>{kDestinationSlot, kThirdSlot, kSecondSlot}
        : operands.Order == FmaOrder::Order213 ? std::array<std::int32_t, 3>{kSecondSlot, kDestinationSlot, kThirdSlot}
        : std::array<std::int32_t, 3>{kSecondSlot, kThirdSlot, kDestinationSlot};

    Assembler code;
    code.PushFlagsInto(Rax);
    code.Store(kSavedFlags, Rax);
    code.Control(3, kControl);
    code.Zero(Rax);
    code.Store(kLane, Rax);
    code.Store(kRaised, Rax);
    const auto next = code.NewLabel();
    code.Bind(next);
    _lane(code, operands, slots, next);
    if (operands.Scalar) {
        code.Load(Rax, kDestinationSlot + 8);
        code.Store(kResultSlot + 8, Rax);
    }
    code.Load32(Rax, kControl);
    code.OperateStack(Arithmetic::Or, Rax, kRaised);
    code.Store32(kNewControl, Rax);
    code.Control(2, kNewControl);
    code.Load(Rax, kSavedFlags);
    code.PopFlagsFrom(Rax);
    body.Raw(code.Finish());

    VexEncoding::StackOperand(body, operands.Wide ? VexEncoding::MoveUnaligned256Load : VexEncoding::MoveUnaligned128Load, operands.Destination, 0, kResultSlot);
    if (operands.Memory) VexEncoding::StackOperand(body, VexEncoding::MoveUnaligned256Load, scratch, 0, kScratchVector);

    Assembler restore;
    for (std::size_t index = 0; index < kSaved.size(); ++index) restore.Load(kSaved[index], kSavedGprs + static_cast<std::int32_t>(index * 8));
    body.Raw(restore.Finish());
    ScalarLowering::StackSlot(body, 0x8D, true, ScalarLowering::Stack, kFrame);
}

}
