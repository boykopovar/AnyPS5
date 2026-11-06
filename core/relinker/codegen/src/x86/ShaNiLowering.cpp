#include <codegen/x86/ShaNiLowering.hpp>
#include <codegen/CodegenException.hpp>
#include <array>
#include <span>

namespace Codegen {

namespace {

constexpr std::uint8_t kRexFirst = 0x40;
constexpr std::uint8_t kRexLast = 0x4F;
constexpr std::uint8_t kRexR = 0x04;
constexpr std::uint8_t kRexB = 0x01;
constexpr std::uint8_t kRexX = 0x02;
constexpr std::uint8_t kModRmRipBase = 5;
constexpr std::uint8_t kModRmSibBase = 4;
constexpr std::int32_t kMemoryFrame = 0x90;
constexpr std::uint8_t kEscape = 0x0F;
constexpr std::uint8_t kEscape38 = 0x38;
constexpr std::uint8_t kEscape3A = 0x3A;
constexpr std::uint8_t kModRmModShift = 6;
constexpr std::uint8_t kModRmRegisterMod = 3;
constexpr std::uint8_t kMovdqa[] = {0x0F, 0x6F};
constexpr std::uint8_t kPor[] = {0x0F, 0xEB};
constexpr std::uint8_t kPand[] = {0x0F, 0xDB};
constexpr std::uint8_t kPaddd[] = {0x0F, 0xFE};
constexpr std::uint8_t kShiftLeftDword = 6;
constexpr std::uint8_t kShiftRightDword = 2;
constexpr std::uint8_t kRotateLeft = 30;
constexpr std::uint8_t kRotateRight = 2;
constexpr std::size_t kHighLaneOffset = 12;

constexpr std::uint8_t kPxor[] = {0x0F, 0xEF};
constexpr std::uint8_t kShiftLeftBytes = 7;
constexpr std::uint8_t kShiftRightBytes = 3;
constexpr std::uint8_t kDwordBits = 32;

template <std::size_t TCount>
std::array<std::uint8_t, TCount> _scratchRegisters(const ShaNiOperands& operands, const bool keepXmm0) {
    std::array<std::uint8_t, TCount> scratch{};
    std::uint8_t found = 0;
    for (std::uint8_t reg = 0; found < TCount; ++reg) {
        if (reg == operands.Destination || reg == operands.Source) continue;
        if (keepXmm0 && reg == 0) continue;
        scratch[found++] = reg;
    }
    return scratch;
}

void _move(StubBodyBuilder& body, const std::uint8_t dst, const std::uint8_t src) {
    body.Sse(kStubPrefixPacked, {kMovdqa[0], kMovdqa[1]}, dst, src);
}

void _rotateLeft(StubBodyBuilder& body, const std::uint8_t reg, const std::uint8_t temp, const std::uint8_t count) {
    _move(body, temp, reg);
    body.ShiftImm(kStubShiftDword, kShiftLeftDword, reg, count);
    body.ShiftImm(kStubShiftDword, kShiftRightDword, temp, static_cast<std::uint8_t>(kDwordBits - count));
    body.Sse(kStubPrefixPacked, {kPor[0], kPor[1]}, reg, temp);
}

void _sigma(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t in, const std::uint8_t tempA, const std::uint8_t tempB, const std::uint8_t rotateA, const std::uint8_t rotateB, const std::uint8_t shift) {
    _move(body, out, in);
    _rotateLeft(body, out, tempA, rotateA);
    _move(body, tempB, in);
    _rotateLeft(body, tempB, tempA, rotateB);
    body.Sse(kStubPrefixPacked, {kPxor[0], kPxor[1]}, out, tempB);
    _move(body, tempB, in);
    body.ShiftImm(kStubShiftDword, kShiftRightDword, tempB, shift);
    body.Sse(kStubPrefixPacked, {kPxor[0], kPxor[1]}, out, tempB);
}

// Keep the scratch slot above all eleven saved GPRs and below the guest red zone.
constexpr std::int32_t kFrameSize = 0x110;
constexpr std::int32_t kDestinationSlot = 0x08;
constexpr std::int32_t kSourceSlot = 0x18;
constexpr std::int32_t kImplicitSlot = 0x28;
constexpr std::int32_t kSaveSlot = 0x38;
constexpr std::int32_t kTempSlot = 0x90;
constexpr std::uint8_t kRax = 0;
constexpr std::uint8_t kRcx = 1;
constexpr std::uint8_t kRdx = 2;
constexpr std::uint8_t kRbx = 3;
constexpr std::uint8_t kRbp = 5;
constexpr std::uint8_t kRsi = 6;
constexpr std::uint8_t kRdi = 7;
constexpr std::uint8_t kR8 = 8;
constexpr std::uint8_t kR9 = 9;
constexpr std::uint8_t kR10 = 10;
constexpr std::uint8_t kR11 = 11;
constexpr std::uint32_t kSha1Constants[] = {0x5A827999u, 0x6ED9EBA1u, 0x8F1BBCDCu, 0xCA62C1D6u};

void _saveGeneralRegisters(StubBodyBuilder& body, const std::span<const std::uint8_t> registers) {
    for (std::size_t index = 0; index < registers.size(); ++index)
        body.StoreQword(registers[index], kSaveSlot + static_cast<std::int32_t>(index) * 8);
}

void _restoreGeneralRegisters(StubBodyBuilder& body, const std::span<const std::uint8_t> registers) {
    for (std::size_t index = 0; index < registers.size(); ++index)
        body.LoadQword(registers[index], kSaveSlot + static_cast<std::int32_t>(index) * 8);
}

void _rotateCopy(StubBodyBuilder& body, const std::uint8_t out, const std::uint8_t in, const std::uint8_t count) {
    body.GprBinary(kGprMov, out, in);
    body.GprRotate(false, out, count);
}

void _emitSha1Rnds4(StubBodyBuilder& body, const ShaNiOperands& operands) {
    constexpr std::uint8_t saved[] = {kRax, kRcx, kRdx, kRbx, kRsi, kRdi, kR8};
    const std::uint8_t a = kRax, b = kRcx, c = kRdx, d = kRbx, e = kRsi, t = kRdi, tmp = kR8;
    const auto function = operands.Immediate & 3u;
    body.AdjustStack(-kFrameSize);
    body.PushFlags();
    body.StoreXmm(operands.Destination, kDestinationSlot);
    body.StoreXmm(operands.Source, kSourceSlot);
    _saveGeneralRegisters(body, saved);
    body.LoadDword(a, kDestinationSlot + 12);
    body.LoadDword(b, kDestinationSlot + 8);
    body.LoadDword(c, kDestinationSlot + 4);
    body.LoadDword(d, kDestinationSlot);
    body.GprBinary(kGprXor, e, e);
    for (int round = 0; round < 4; ++round) {
        if (function == 0) {
            body.GprBinary(kGprMov, tmp, b);
            body.GprNot(tmp);
            body.GprBinary(kGprAnd, tmp, d);
            body.GprBinary(kGprMov, t, b);
            body.GprBinary(kGprAnd, t, c);
            body.GprBinary(kGprOr, t, tmp);
        } else if (function == 2) {
            body.GprBinary(kGprMov, t, b);
            body.GprBinary(kGprAnd, t, c);
            body.GprBinary(kGprMov, tmp, b);
            body.GprBinary(kGprAnd, tmp, d);
            body.GprBinary(kGprOr, t, tmp);
            body.GprBinary(kGprMov, tmp, c);
            body.GprBinary(kGprAnd, tmp, d);
            body.GprBinary(kGprOr, t, tmp);
        } else {
            body.GprBinary(kGprMov, t, b);
            body.GprBinary(kGprXor, t, c);
            body.GprBinary(kGprXor, t, d);
        }
        body.GprBinary(kGprMov, tmp, a);
        body.GprRotate(true, tmp, 5);
        body.GprBinary(kGprAdd, t, tmp);
        body.AddDwordFromStack(t, kSourceSlot + 12 - round * 4);
        body.GprBinary(kGprAdd, t, e);
        body.GprAddImmediate(t, kSha1Constants[function]);
        body.GprBinary(kGprMov, e, d);
        body.GprBinary(kGprMov, d, c);
        body.GprBinary(kGprMov, c, b);
        body.GprRotate(true, c, 30);
        body.GprBinary(kGprMov, b, a);
        body.GprBinary(kGprMov, a, t);
    }
    body.StoreDword(a, kDestinationSlot + 12);
    body.StoreDword(b, kDestinationSlot + 8);
    body.StoreDword(c, kDestinationSlot + 4);
    body.StoreDword(d, kDestinationSlot);
    body.LoadXmm(operands.Destination, kDestinationSlot);
    _restoreGeneralRegisters(body, saved);
    body.PopFlags();
    body.AdjustStack(kFrameSize);
}

void _emitSha256Rnds2(StubBodyBuilder& body, const ShaNiOperands& operands) {
    constexpr std::uint8_t saved[] = {kRax, kRcx, kRdx, kRbx, kRsi, kRdi, kRbp, kR8, kR9, kR10, kR11};
    const std::uint8_t a = kRax, b = kRcx, c = kRdx, d = kRbx, e = kRsi, f = kRdi, g = kR8, h = kR9;
    const std::uint8_t t1 = kR10, tmp = kR11, tmp2 = kRbp;
    body.AdjustStack(-kFrameSize);
    body.PushFlags();
    body.StoreXmm(operands.Destination, kDestinationSlot);
    body.StoreXmm(operands.Source, kSourceSlot);
    body.StoreXmm(0, kImplicitSlot);
    _saveGeneralRegisters(body, saved);
    body.LoadDword(a, kSourceSlot + 12);
    body.LoadDword(b, kSourceSlot + 8);
    body.LoadDword(e, kSourceSlot + 4);
    body.LoadDword(f, kSourceSlot);
    body.LoadDword(c, kDestinationSlot + 12);
    body.LoadDword(d, kDestinationSlot + 8);
    body.LoadDword(g, kDestinationSlot + 4);
    body.LoadDword(h, kDestinationSlot);
    for (int round = 0; round < 2; ++round) {
        _rotateCopy(body, t1, e, 6);
        _rotateCopy(body, tmp, e, 11);
        body.GprBinary(kGprXor, t1, tmp);
        _rotateCopy(body, tmp, e, 25);
        body.GprBinary(kGprXor, t1, tmp);
        body.GprBinary(kGprMov, tmp, e);
        body.GprNot(tmp);
        body.GprBinary(kGprAnd, tmp, g);
        body.GprBinary(kGprMov, tmp2, e);
        body.GprBinary(kGprAnd, tmp2, f);
        body.GprBinary(kGprXor, tmp, tmp2);
        body.GprBinary(kGprAdd, t1, tmp);
        body.GprBinary(kGprAdd, t1, h);
        body.AddDwordFromStack(t1, kImplicitSlot + round * 4);
        _rotateCopy(body, tmp, a, 2);
        _rotateCopy(body, tmp2, a, 13);
        body.GprBinary(kGprXor, tmp, tmp2);
        _rotateCopy(body, tmp2, a, 22);
        body.GprBinary(kGprXor, tmp, tmp2);
        body.StoreDword(tmp, kTempSlot);
        body.GprBinary(kGprMov, tmp, a);
        body.GprBinary(kGprAnd, tmp, b);
        body.GprBinary(kGprMov, tmp2, a);
        body.GprBinary(kGprAnd, tmp2, c);
        body.GprBinary(kGprXor, tmp, tmp2);
        body.GprBinary(kGprMov, tmp2, b);
        body.GprBinary(kGprAnd, tmp2, c);
        body.GprBinary(kGprXor, tmp, tmp2);
        body.AddDwordFromStack(tmp, kTempSlot);
        body.GprBinary(kGprMov, h, g);
        body.GprBinary(kGprMov, g, f);
        body.GprBinary(kGprMov, f, e);
        body.GprBinary(kGprMov, e, d);
        body.GprBinary(kGprAdd, e, t1);
        body.GprBinary(kGprMov, d, c);
        body.GprBinary(kGprMov, c, b);
        body.GprBinary(kGprMov, b, a);
        body.GprBinary(kGprMov, a, t1);
        body.GprBinary(kGprAdd, a, tmp);
    }
    body.StoreDword(a, kDestinationSlot + 12);
    body.StoreDword(b, kDestinationSlot + 8);
    body.StoreDword(e, kDestinationSlot + 4);
    body.StoreDword(f, kDestinationSlot);
    body.LoadXmm(operands.Destination, kDestinationSlot);
    _restoreGeneralRegisters(body, saved);
    body.PopFlags();
    body.AdjustStack(kFrameSize);
}
void _emitSha1Nexte(StubBodyBuilder& body, const ShaNiOperands& operands) {
    const auto scratch = _scratchRegisters<2>(operands, false);
    StubConstant highLane{};
    for (std::size_t index = kHighLaneOffset; index < highLane.size(); ++index)
        highLane[index] = 0xFF;
    body.Spill(scratch[0]);
    body.Spill(scratch[1]);
    _move(body, scratch[0], operands.Destination);
    body.ShiftImm(kStubShiftDword, kShiftLeftDword, scratch[0], kRotateLeft);
    _move(body, scratch[1], operands.Destination);
    body.ShiftImm(kStubShiftDword, kShiftRightDword, scratch[1], kRotateRight);
    body.Sse(kStubPrefixPacked, {kPor[0], kPor[1]}, scratch[0], scratch[1]);
    body.RipOperand({kPand[0], kPand[1]}, scratch[0], highLane);
    _move(body, operands.Destination, operands.Source);
    body.Sse(kStubPrefixPacked, {kPaddd[0], kPaddd[1]}, operands.Destination, scratch[0]);
    body.Restore(scratch[1]);
    body.Restore(scratch[0]);
}

void _emitSha1Msg1(StubBodyBuilder& body, const ShaNiOperands& operands) {
    const auto scratch = _scratchRegisters<2>(operands, false);
    body.Spill(scratch[0]);
    body.Spill(scratch[1]);
    _move(body, scratch[0], operands.Destination);
    body.ShiftImm(kStubShiftQword, kShiftLeftBytes, scratch[0], 8);
    _move(body, scratch[1], operands.Source);
    body.ShiftImm(kStubShiftQword, kShiftRightBytes, scratch[1], 8);
    body.Sse(kStubPrefixPacked, {kPor[0], kPor[1]}, scratch[0], scratch[1]);
    body.Sse(kStubPrefixPacked, {kPxor[0], kPxor[1]}, operands.Destination, scratch[0]);
    body.Restore(scratch[1]);
    body.Restore(scratch[0]);
}

void _emitSha1Msg2(StubBodyBuilder& body, const ShaNiOperands& operands) {
    const auto scratch = _scratchRegisters<3>(operands, false);
    for (const auto reg : scratch) body.Spill(reg);
    _move(body, scratch[0], operands.Source);
    body.ShiftImm(kStubShiftQword, kShiftLeftBytes, scratch[0], 4);
    body.Sse(kStubPrefixPacked, {kPxor[0], kPxor[1]}, scratch[0], operands.Destination);
    _rotateLeft(body, scratch[0], scratch[1], 1);
    _move(body, scratch[1], scratch[0]);
    body.ShiftImm(kStubShiftQword, kShiftRightBytes, scratch[1], 12);
    _rotateLeft(body, scratch[1], scratch[2], 1);
    body.Sse(kStubPrefixPacked, {kPxor[0], kPxor[1]}, scratch[0], scratch[1]);
    _move(body, operands.Destination, scratch[0]);
    for (auto index = scratch.size(); index > 0; --index) body.Restore(scratch[index - 1]);
}

void _emitSha256Msg1(StubBodyBuilder& body, const ShaNiOperands& operands) {
    const auto scratch = _scratchRegisters<4>(operands, false);
    for (const auto reg : scratch) body.Spill(reg);
    _move(body, scratch[0], operands.Destination);
    body.ShiftImm(kStubShiftQword, kShiftRightBytes, scratch[0], 4);
    _move(body, scratch[1], operands.Source);
    body.ShiftImm(kStubShiftQword, kShiftLeftBytes, scratch[1], 12);
    body.Sse(kStubPrefixPacked, {kPor[0], kPor[1]}, scratch[0], scratch[1]);
    _sigma(body, scratch[1], scratch[0], scratch[2], scratch[3], 25, 14, 3);
    body.Sse(kStubPrefixPacked, {kPaddd[0], kPaddd[1]}, operands.Destination, scratch[1]);
    for (auto index = scratch.size(); index > 0; --index) body.Restore(scratch[index - 1]);
}

void _emitSha256Msg2(StubBodyBuilder& body, const ShaNiOperands& operands) {
    const auto scratch = _scratchRegisters<4>(operands, false);
    for (const auto reg : scratch) body.Spill(reg);
    _move(body, scratch[0], operands.Source);
    body.ShiftImm(kStubShiftQword, kShiftRightBytes, scratch[0], 8);
    _sigma(body, scratch[1], scratch[0], scratch[2], scratch[3], 15, 13, 10);
    body.Sse(kStubPrefixPacked, {kPaddd[0], kPaddd[1]}, operands.Destination, scratch[1]);
    _move(body, scratch[0], operands.Destination);
    body.ShiftImm(kStubShiftQword, kShiftLeftBytes, scratch[0], 8);
    _sigma(body, scratch[1], scratch[0], scratch[2], scratch[3], 15, 13, 10);
    body.Sse(kStubPrefixPacked, {kPaddd[0], kPaddd[1]}, operands.Destination, scratch[1]);
    for (auto index = scratch.size(); index > 0; --index) body.Restore(scratch[index - 1]);
}

}

ShaNiOperands DecodeShaNi(const std::uint8_t* data, const std::size_t length) {
    std::size_t position = 0;
    std::uint8_t rex = 0;
    while (position < length && data[position] >= kRexFirst && data[position] <= kRexLast)
        rex = data[position++];
    if (position + 3 > length)
        throw CodegenException("SHA-NI instruction truncated before its ModRM byte");
    if (data[position] != kEscape || (data[position + 1] != kEscape38 && data[position + 1] != kEscape3A))
        throw CodegenException("Instruction is not a SHA-NI opcode");
    ShaNiOperands operands;
    operands.ThreeByte3A = data[position + 1] == kEscape3A;
    operands.Opcode = data[position + 2];
    if (position + 3 >= length)
        throw CodegenException("SHA-NI instruction truncated before its ModRM byte");
    const auto modrm = data[position + 3];
    const auto mod = static_cast<std::uint8_t>(modrm >> kModRmModShift);
    operands.Destination = static_cast<std::uint8_t>(((modrm >> 3) & 7) | ((rex & kRexR) != 0 ? 8 : 0));
    operands.RexExtension = static_cast<std::uint8_t>(rex & (kRexX | kRexB));
    if (mod == kModRmRegisterMod) {
        operands.Source = static_cast<std::uint8_t>((modrm & 7) | ((rex & kRexB) != 0 ? 8 : 0));
    } else {
        operands.MemoryForm = true;
        const auto rm = static_cast<std::uint8_t>(modrm & 7);
        operands.RipRelative = mod == 0 && rm == kModRmRipBase;
        std::size_t cursor = position + 4;
        std::uint8_t sib = 0;
        const bool hasSib = rm == kModRmSibBase;
        if (hasSib) {
            if (cursor >= length)
                throw CodegenException("SHA-NI instruction truncated before its SIB byte");
            sib = data[cursor++];
        }
        std::size_t displacementSize = 0;
        if (mod == 1) displacementSize = 1;
        else if (mod == 2) displacementSize = 4;
        else if (hasSib && (sib & 7) == kModRmRipBase) displacementSize = 4;
        else if (!hasSib && rm == kModRmRipBase) displacementSize = 4;
        if (cursor + displacementSize > length)
            throw CodegenException("SHA-NI instruction truncated before its displacement");
        std::int32_t displacement = 0;
        if (displacementSize == 1) {
            displacement = static_cast<std::int8_t>(data[cursor]);
        } else if (displacementSize == 4) {
            std::uint32_t raw = 0;
            for (std::size_t index = 0; index < 4; ++index)
                raw |= static_cast<std::uint32_t>(data[cursor + index]) << (index * 8);
            displacement = static_cast<std::int32_t>(raw);
        }
        operands.StackRelative = hasSib && (sib & 7) == kModRmSibBase && (rex & kRexB) == 0 && mod != 0;
        if (hasSib && (sib & 7) == kModRmSibBase && (rex & kRexB) == 0 && mod == 0)
            operands.StackRelative = true;
        operands.Displacement = displacement;
        operands.Address.assign(data + position + 3, data + cursor + displacementSize);
        operands.Length = cursor + displacementSize - 0;
        if (hasSib)
            operands.Source = sib;
    }
    if (!operands.MemoryForm)
        operands.Length = position + 4;
    if (operands.ThreeByte3A) {
        const auto immediateOffset = position + 3 + (operands.MemoryForm ? operands.Address.size() : 1);
        if (immediateOffset >= length)
            throw CodegenException("SHA1RNDS4 truncated before its immediate");
        operands.Immediate = data[immediateOffset];
        ++operands.Length;
    }
    return operands;
}

bool ShaNiLowering::CanLower(const ShaNiOperands& operands) const {
    if (operands.ThreeByte3A)
        return operands.Opcode == kShaOpSha1Rnds4;
    switch (operands.Opcode) {
    case kShaOpSha256Rnds2:
    case kShaOpSha1Nexte:
    case kShaOpSha1Msg1:
    case kShaOpSha1Msg2:
    case kShaOpSha256Msg1:
    case kShaOpSha256Msg2:
        return true;
    default:
        return false;
    }
}

void _emitOperation(StubBodyBuilder& body, const ShaNiOperands& operands) {
    if (operands.ThreeByte3A) {
        _emitSha1Rnds4(body, operands);
        return;
    }
    switch (operands.Opcode) {
    case kShaOpSha256Rnds2:
        _emitSha256Rnds2(body, operands);
        return;
    case kShaOpSha1Nexte:
        _emitSha1Nexte(body, operands);
        return;
    case kShaOpSha1Msg1:
        _emitSha1Msg1(body, operands);
        return;
    case kShaOpSha1Msg2:
        _emitSha1Msg2(body, operands);
        return;
    case kShaOpSha256Msg1:
        _emitSha256Msg1(body, operands);
        return;
    default:
        _emitSha256Msg2(body, operands);
        return;
    }
}

void _emitMemoryForm(StubBodyBuilder& body, const ShaNiOperands& operands, const std::size_t instructionEnd) {
    std::uint8_t loaded = 0;
    while (loaded == operands.Destination || (operands.Opcode == kShaOpSha256Rnds2 && !operands.ThreeByte3A && loaded == 0))
        ++loaded;
    auto registerForm = operands;
    registerForm.MemoryForm = false;
    registerForm.Source = loaded;
    registerForm.Address.clear();
    body.AdjustStack(-kMemoryFrame);
    body.StoreXmm(loaded, 0);
    if (operands.StackRelative) {
        body.LoadXmmStackRelative(loaded, operands.RexExtension, operands.Source, operands.Displacement + kMemoryFrame);
    } else {
        body.LoadXmmIndirect(loaded, operands.RexExtension, operands.Address);
        if (operands.RipRelative)
            body.AddRipFixup(body.Size() - 4, operands.Displacement, instructionEnd);
    }
    _emitOperation(body, registerForm);
    body.LoadXmm(loaded, 0);
    body.AdjustStack(kMemoryFrame);
}

LoweredBody ShaNiLowering::LowerOutOfLine(const std::span<const ShaNiOperands> sequence, const std::span<const std::uint8_t> trailing) const {
    StubBodyBuilder body;
    std::size_t consumed = 0;
    for (const auto& operands : sequence) {
        if (!CanLower(operands))
            throw CodegenException("SHA-NI opcode has no Intel lowering");
        consumed += operands.Length;
        if (operands.MemoryForm)
            _emitMemoryForm(body, operands, consumed);
        else
            _emitOperation(body, operands);
    }
    const auto trailingOffset = body.Size();
    body.Raw(trailing);
    auto result = body.Finish();
    result.TrailingOffset = trailingOffset;
    return result;
}

}
