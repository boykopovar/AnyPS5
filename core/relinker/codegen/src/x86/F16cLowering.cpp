#include <codegen/x86/F16cLowering.hpp>
#include <codegen/x86/VexEncoding.hpp>
#include <array>
#include <limits>
#include <string>
#include <vector>

namespace Codegen {

using namespace ScalarLowering;
using namespace VexEncoding;

namespace {

constexpr std::uint8_t kPushFlags = 0x9C;
constexpr std::uint8_t kPopFlags = 0x9D;
constexpr std::array<std::uint8_t, 7> kSavedGprs{0, 1, 2, 6, 7, 8, 9};
constexpr std::int32_t kGprBaseSlot = 8;
constexpr std::int32_t kMxcsrSlot = 64;
constexpr std::int32_t kInputSlot = 80;
constexpr std::int32_t kOutputSlot = 112;
constexpr std::int32_t kYmmSlot = 144;
constexpr std::int32_t kSpillBytes = 176;
constexpr std::int32_t kDefaultFrame = 128 + kSpillBytes;

constexpr Form kLoad64{Map0F, PrefixF3, false, false, 0x7E};
constexpr Form kStore64{Map0F, Prefix66, false, false, 0xD6};
constexpr Form kStore128{Map0F, PrefixF3, false, false, 0x7F};

constexpr std::uint8_t kRegEax = 0;
constexpr std::uint8_t kRegEcx = 1;
constexpr std::uint8_t kRegEdx = 2;
constexpr std::uint8_t kRegEsi = 6;
constexpr std::uint8_t kRegEdi = 7;
constexpr std::uint8_t kRegR8 = 8;
constexpr std::uint8_t kRegR9 = 9;

constexpr std::uint8_t kCcB = 0x72;
constexpr std::uint8_t kCcE = 0x74;
constexpr std::uint8_t kCcNe = 0x75;
constexpr std::uint8_t kCcA = 0x77;

std::int32_t _selectFrame(const std::optional<MemoryOperand>& memory, const std::int64_t width) {
    if (!memory || !UnindexedStack(*memory)) return kDefaultFrame;
    const auto displacement = static_cast<std::int64_t>(memory->Displacement);
    if (displacement >= kSpillBytes - kDefaultFrame || displacement + width <= -kDefaultFrame) return kDefaultFrame;
    const auto needed = (-displacement + kSpillBytes + 15) & ~std::int64_t{15};
    if (needed > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("stack spill frame exceeds 32-bit displacement");
    return static_cast<std::int32_t>(needed);
}

std::uint8_t _scratchYmm(const std::uint8_t avoid) {
    return avoid == 15 ? 14 : 15;
}

class CodeBuffer {
public:
    struct Fixup {
        std::size_t Offset;
        std::size_t TargetLabel;
    };

    std::size_t NewLabel() {
        _labels.push_back(std::numeric_limits<std::size_t>::max());
        return _labels.size() - 1;
    }

    void Bind(const std::size_t label) {
        _labels[label] = _bytes.size();
    }

    void Jmp(const std::size_t label) {
        _bytes.push_back(0xE9);
        _fixups.push_back({_bytes.size(), label});
        Append32(_bytes, 0);
    }

    void Jcc(const std::uint8_t shortCc, const std::size_t label) {
        _bytes.push_back(0x0F);
        _bytes.push_back(static_cast<std::uint8_t>(shortCc + 0x10));
        _fixups.push_back({_bytes.size(), label});
        Append32(_bytes, 0);
    }

    void Raw(std::initializer_list<std::uint8_t> bytes) {
        _bytes.insert(_bytes.end(), bytes);
    }

    void EmitReg32(const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t rm) {
        _rex(reg, rm);
        _bytes.push_back(opcode);
        _bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((reg & 7) << 3) | (rm & 7)));
    }

    void Mov32(const std::uint8_t dst, const std::uint8_t src) {
        EmitReg32(0x89, src, dst);
    }

    void MovImm32(const std::uint8_t dst, const std::uint32_t imm) {
        if ((dst & 8) != 0) _bytes.push_back(0x41);
        _bytes.push_back(static_cast<std::uint8_t>(0xB8 | (dst & 7)));
        Append32(_bytes, static_cast<std::int32_t>(imm));
    }

    void Xor32(const std::uint8_t dst, const std::uint8_t src) {
        EmitReg32(0x31, src, dst);
    }

    void Or32(const std::uint8_t dst, const std::uint8_t src) {
        EmitReg32(0x09, src, dst);
    }

    void Sub32(const std::uint8_t dst, const std::uint8_t src) {
        EmitReg32(0x29, src, dst);
    }

    void Test32(const std::uint8_t a, const std::uint8_t b) {
        EmitReg32(0x85, b, a);
    }

    void Cmp32(const std::uint8_t a, const std::uint8_t b) {
        EmitReg32(0x39, b, a);
    }

    void AluImm32(const std::uint8_t ext, const std::uint8_t dst, const std::uint32_t imm) {
        _rex(0, dst);
        if (static_cast<std::int32_t>(imm) >= -128 && static_cast<std::int32_t>(imm) <= 127) {
            _bytes.push_back(0x83);
            _bytes.push_back(static_cast<std::uint8_t>(0xC0 | (ext << 3) | (dst & 7)));
            _bytes.push_back(static_cast<std::uint8_t>(imm));
        } else {
            _bytes.push_back(0x81);
            _bytes.push_back(static_cast<std::uint8_t>(0xC0 | (ext << 3) | (dst & 7)));
            Append32(_bytes, static_cast<std::int32_t>(imm));
        }
    }

    void AddImm32(const std::uint8_t dst, const std::uint32_t imm) { AluImm32(0, dst, imm); }
    void OrImm32(const std::uint8_t dst, const std::uint32_t imm) { AluImm32(1, dst, imm); }
    void AndImm32(const std::uint8_t dst, const std::uint32_t imm) { AluImm32(4, dst, imm); }
    void SubImm32(const std::uint8_t dst, const std::uint32_t imm) { AluImm32(5, dst, imm); }
    void CmpImm32(const std::uint8_t dst, const std::uint32_t imm) { AluImm32(7, dst, imm); }

    void TestImm32(const std::uint8_t dst, const std::uint32_t imm) {
        _rex(0, dst);
        _bytes.push_back(0xF7);
        _bytes.push_back(static_cast<std::uint8_t>(0xC0 | (dst & 7)));
        Append32(_bytes, static_cast<std::int32_t>(imm));
    }

    void ShiftImm32(const std::uint8_t ext, const std::uint8_t dst, const std::uint8_t count) {
        _rex(0, dst);
        _bytes.push_back(0xC1);
        _bytes.push_back(static_cast<std::uint8_t>(0xC0 | (ext << 3) | (dst & 7)));
        _bytes.push_back(count);
    }

    void ShlImm32(const std::uint8_t dst, const std::uint8_t count) { ShiftImm32(4, dst, count); }
    void ShrImm32(const std::uint8_t dst, const std::uint8_t count) { ShiftImm32(5, dst, count); }

    void ShlCl32(const std::uint8_t dst) {
        _rex(0, dst);
        _bytes.push_back(0xD3);
        _bytes.push_back(static_cast<std::uint8_t>(0xE0 | (dst & 7)));
    }

    void ShrCl32(const std::uint8_t dst) {
        _rex(0, dst);
        _bytes.push_back(0xD3);
        _bytes.push_back(static_cast<std::uint8_t>(0xE8 | (dst & 7)));
    }

    void Neg32(const std::uint8_t dst) {
        _rex(0, dst);
        _bytes.push_back(0xF7);
        _bytes.push_back(static_cast<std::uint8_t>(0xD8 | (dst & 7)));
    }

    void Inc32(const std::uint8_t dst) {
        _rex(0, dst);
        _bytes.push_back(0xFF);
        _bytes.push_back(static_cast<std::uint8_t>(0xC0 | (dst & 7)));
    }

    void Dec32(const std::uint8_t dst) {
        _rex(0, dst);
        _bytes.push_back(0xFF);
        _bytes.push_back(static_cast<std::uint8_t>(0xC8 | (dst & 7)));
    }

    void Bsr32(const std::uint8_t dst, const std::uint8_t src) {
        _rex(dst, src);
        _bytes.push_back(0x0F);
        _bytes.push_back(0xBD);
        _bytes.push_back(static_cast<std::uint8_t>(0xC0 | ((dst & 7) << 3) | (src & 7)));
    }

    std::vector<std::uint8_t> Finish() {
        for (const auto& fixup : _fixups) {
            const auto target = _labels[fixup.TargetLabel];
            const auto displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(target) - static_cast<std::int64_t>(fixup.Offset + 4));
            for (unsigned shift = 0; shift < 32; shift += 8)
                _bytes[fixup.Offset + shift / 8] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(displacement) >> shift);
        }
        return std::move(_bytes);
    }

private:
    std::vector<std::uint8_t> _bytes;
    std::vector<std::size_t> _labels;
    std::vector<Fixup> _fixups;

    void _rex(const std::uint8_t reg, const std::uint8_t rm) {
        const auto rex = static_cast<std::uint8_t>(0x40 | ((reg & 8) >> 1) | ((rm & 8) >> 3));
        if (rex != 0x40) _bytes.push_back(rex);
    }
};

void _saveGprs(StubBodyBuilder& body) {
    for (std::size_t index = 0; index < kSavedGprs.size(); ++index)
        StackSlot(body, 0x89, true, kSavedGprs[index], kGprBaseSlot + static_cast<std::int32_t>(index * 8));
}

void _restoreGprs(StubBodyBuilder& body) {
    for (std::size_t index = 0; index < kSavedGprs.size(); ++index)
        StackSlot(body, 0x8B, true, kSavedGprs[index], kGprBaseSlot + static_cast<std::int32_t>(index * 8));
}

void _emitPh2PsLoop(StubBodyBuilder& body, const std::uint32_t count) {
    CodeBuffer code;
    const auto loopTop = code.NewLabel();
    const auto zeroOrSubnormal = code.NewLabel();
    const auto isZero = code.NewLabel();
    const auto infOrNan = code.NewLabel();
    const auto isInf = code.NewLabel();
    const auto store = code.NewLabel();

    code.Xor32(kRegEdi, kRegEdi);
    code.Bind(loopTop);
    code.Raw({0x0F, 0xB7, 0x44, 0x7C, static_cast<std::uint8_t>(kInputSlot)});
    code.Mov32(kRegEdx, kRegEax);
    code.AndImm32(kRegEdx, 0x8000u);
    code.ShlImm32(kRegEdx, 16);
    code.Mov32(kRegEsi, kRegEax);
    code.AndImm32(kRegEsi, 0x3FFu);
    code.Mov32(kRegEcx, kRegEax);
    code.ShrImm32(kRegEcx, 10);
    code.AndImm32(kRegEcx, 0x1Fu);
    code.Test32(kRegEcx, kRegEcx);
    code.Jcc(kCcE, zeroOrSubnormal);
    code.CmpImm32(kRegEcx, 31u);
    code.Jcc(kCcE, infOrNan);

    code.AddImm32(kRegEcx, 112u);
    code.ShlImm32(kRegEcx, 23);
    code.ShlImm32(kRegEsi, 13);
    code.Mov32(kRegEax, kRegEdx);
    code.Or32(kRegEax, kRegEcx);
    code.Or32(kRegEax, kRegEsi);
    code.Jmp(store);

    code.Bind(zeroOrSubnormal);
    code.Test32(kRegEsi, kRegEsi);
    code.Jcc(kCcE, isZero);
    code.Bsr32(kRegEax, kRegEsi);
    code.MovImm32(kRegEcx, 23u);
    code.Sub32(kRegEcx, kRegEax);
    code.ShlCl32(kRegEsi);
    code.AndImm32(kRegEsi, 0x7FFFFFu);
    code.AddImm32(kRegEax, 103u);
    code.ShlImm32(kRegEax, 23);
    code.Or32(kRegEax, kRegEdx);
    code.Or32(kRegEax, kRegEsi);
    code.Jmp(store);

    code.Bind(isZero);
    code.Mov32(kRegEax, kRegEdx);
    code.Jmp(store);

    code.Bind(infOrNan);
    code.Test32(kRegEsi, kRegEsi);
    code.Jcc(kCcE, isInf);
    code.ShlImm32(kRegEsi, 13);
    code.MovImm32(kRegEax, 0x7FC00000u);
    code.Or32(kRegEax, kRegEdx);
    code.Or32(kRegEax, kRegEsi);
    code.Jmp(store);

    code.Bind(isInf);
    code.MovImm32(kRegEax, 0x7F800000u);
    code.Or32(kRegEax, kRegEdx);

    code.Bind(store);
    code.Raw({0x89, 0x44, 0xBC, static_cast<std::uint8_t>(kOutputSlot)});
    code.Inc32(kRegEdi);
    code.CmpImm32(kRegEdi, count);
    code.Jcc(kCcB, loopTop);

    body.Raw(code.Finish());
}

void _emitPs2PhLoop(StubBodyBuilder& body, const std::uint32_t count, const std::uint8_t immediate) {
    CodeBuffer code;
    const auto loopTop = code.NewLabel();
    const auto isZero = code.NewLabel();
    const auto isNan = code.NewLabel();
    const auto isInf = code.NewLabel();
    const auto isOverflow = code.NewLabel();
    const auto overflowInf = code.NewLabel();
    const auto subnormalOrUnderflow = code.NewLabel();
    const auto isTiny = code.NewLabel();
    const auto tinyNonZero = code.NewLabel();
    const auto doRound = code.NewLabel();
    const auto roundRne = code.NewLabel();
    const auto roundUp = code.NewLabel();
    const auto attachSign = code.NewLabel();

    code.Raw({0x0F, 0xAE, 0x5C, 0x24, static_cast<std::uint8_t>(kMxcsrSlot)});
    if ((immediate & 4u) != 0) {
        code.Raw({0x44, 0x8B, 0x44, 0x24, static_cast<std::uint8_t>(kMxcsrSlot)});
        code.ShrImm32(kRegR8, 13);
        code.AndImm32(kRegR8, 3u);
    } else {
        code.MovImm32(kRegR8, immediate & 3u);
    }

    code.Raw({0x48, 0xC7, 0x44, 0x24, static_cast<std::uint8_t>(kOutputSlot), 0, 0, 0, 0});
    code.Raw({0x48, 0xC7, 0x44, 0x24, static_cast<std::uint8_t>(kOutputSlot + 8), 0, 0, 0, 0});

    code.Xor32(kRegEdi, kRegEdi);
    code.Bind(loopTop);
    code.Raw({0x8B, 0x44, 0xBC, static_cast<std::uint8_t>(kInputSlot)});
    code.Mov32(kRegEdx, kRegEax);
    code.ShrImm32(kRegEdx, 31);
    code.AndImm32(kRegEax, 0x7FFFFFFFu);
    code.Test32(kRegEax, kRegEax);
    code.Jcc(kCcE, isZero);
    code.CmpImm32(kRegEax, 0x7F800000u);
    code.Jcc(kCcA, isNan);
    code.Jcc(kCcE, isInf);
    code.Mov32(kRegEcx, kRegEax);
    code.ShrImm32(kRegEcx, 23);
    code.Mov32(kRegEsi, kRegEax);
    code.AndImm32(kRegEsi, 0x7FFFFFu);
    code.CmpImm32(kRegEcx, 142u);
    code.Jcc(kCcA, isOverflow);
    code.CmpImm32(kRegEcx, 113u);
    code.Jcc(kCcB, subnormalOrUnderflow);

    code.SubImm32(kRegEcx, 112u);
    code.ShlImm32(kRegEcx, 10);
    code.Mov32(kRegEax, kRegEsi);
    code.ShrImm32(kRegEax, 13);
    code.Or32(kRegEax, kRegEcx);
    code.Mov32(kRegEcx, kRegEsi);
    code.AndImm32(kRegEcx, 0xFFFu);
    code.ShrImm32(kRegEsi, 12);
    code.AndImm32(kRegEsi, 1u);
    code.Jmp(doRound);

    code.Bind(subnormalOrUnderflow);
    code.CmpImm32(kRegEcx, 102u);
    code.Jcc(kCcB, isTiny);
    code.OrImm32(kRegEsi, 0x00800000u);
    code.Neg32(kRegEcx);
    code.AddImm32(kRegEcx, 126u);
    code.Mov32(kRegEax, kRegEsi);
    code.ShrCl32(kRegEax);
    code.Dec32(kRegEcx);
    code.Mov32(kRegR9, kRegEsi);
    code.ShrCl32(kRegR9);
    code.AndImm32(kRegR9, 1u);
    code.Neg32(kRegEcx);
    code.AddImm32(kRegEcx, 32u);
    code.ShlCl32(kRegEsi);
    code.Mov32(kRegEcx, kRegEsi);
    code.Mov32(kRegEsi, kRegR9);
    code.Jmp(doRound);

    code.Bind(isTiny);
    code.Test32(kRegEcx, kRegEcx);
    code.Jcc(kCcNe, tinyNonZero);
    code.Raw({0xF7, 0x44, 0x24, static_cast<std::uint8_t>(kMxcsrSlot), 0x40, 0x00, 0x00, 0x00});
    code.Jcc(kCcNe, isZero);
    code.Bind(tinyNonZero);
    code.Xor32(kRegEax, kRegEax);
    code.Xor32(kRegEsi, kRegEsi);
    code.MovImm32(kRegEcx, 1u);
    code.Jmp(doRound);

    code.Bind(isOverflow);
    code.MovImm32(kRegEax, 0x7BFFu);
    code.CmpImm32(kRegR8, 0u);
    code.Jcc(kCcE, overflowInf);
    code.CmpImm32(kRegR8, 3u);
    code.Jcc(kCcE, attachSign);
    code.Mov32(kRegEcx, kRegR8);
    code.AndImm32(kRegEcx, 1u);
    code.Cmp32(kRegEcx, kRegEdx);
    code.Jcc(kCcNe, attachSign);
    code.Bind(overflowInf);
    code.MovImm32(kRegEax, 0x7C00u);
    code.Jmp(attachSign);

    code.Bind(isZero);
    code.Xor32(kRegEax, kRegEax);
    code.Jmp(attachSign);

    code.Bind(isInf);
    code.MovImm32(kRegEax, 0x7C00u);
    code.Jmp(attachSign);

    code.Bind(isNan);
    code.AndImm32(kRegEax, 0x7FFFFFu);
    code.ShrImm32(kRegEax, 13);
    code.OrImm32(kRegEax, 0x7E00u);
    code.Jmp(attachSign);

    code.Bind(doRound);
    code.CmpImm32(kRegR8, 0u);
    code.Jcc(kCcE, roundRne);
    code.CmpImm32(kRegR8, 3u);
    code.Jcc(kCcE, attachSign);
    code.Or32(kRegEsi, kRegEcx);
    code.Jcc(kCcE, attachSign);
    code.Mov32(kRegEcx, kRegR8);
    code.AndImm32(kRegEcx, 1u);
    code.Cmp32(kRegEcx, kRegEdx);
    code.Jcc(kCcNe, attachSign);
    code.Inc32(kRegEax);
    code.Jmp(attachSign);

    code.Bind(roundRne);
    code.Test32(kRegEsi, kRegEsi);
    code.Jcc(kCcE, attachSign);
    code.Test32(kRegEcx, kRegEcx);
    code.Jcc(kCcNe, roundUp);
    code.TestImm32(kRegEax, 1u);
    code.Jcc(kCcE, attachSign);
    code.Bind(roundUp);
    code.Inc32(kRegEax);

    code.Bind(attachSign);
    code.ShlImm32(kRegEdx, 15);
    code.Or32(kRegEax, kRegEdx);
    code.Raw({0x66, 0x89, 0x44, 0x7C, static_cast<std::uint8_t>(kOutputSlot)});
    code.Inc32(kRegEdi);
    code.CmpImm32(kRegEdi, count);
    code.Jcc(kCcB, loopTop);

    body.Raw(code.Finish());
}

void _emitHalfToSingle(StubBodyBuilder& body, const F16cOperands& operands) {
    const auto frame = _selectFrame(operands.Memory, operands.Vector256 ? 16 : 8);
    StackSlot(body, 0x8D, true, Stack, 8 - frame);
    body.Raw(std::array{kPushFlags});
    const auto scratch = _scratchYmm(operands.Destination);
    if (operands.Memory) {
        StackOperand(body, MoveUnaligned256Store, scratch, 0, kYmmSlot);
        Memory(body, operands.Vector256 ? MoveUnaligned128Load : kLoad64, scratch, 0, *operands.Memory, frame, operands.Name);
        StackOperand(body, kStore128, scratch, 0, kInputSlot);
    } else {
        StackOperand(body, kStore128, operands.Source, 0, kInputSlot);
    }
    _saveGprs(body);
    _emitPh2PsLoop(body, operands.Vector256 ? 8u : 4u);
    _restoreGprs(body);
    if (operands.Memory)
        StackOperand(body, MoveUnaligned256Load, scratch, 0, kYmmSlot);
    StackOperand(body, operands.Vector256 ? MoveUnaligned256Load : MoveUnaligned128Load, operands.Destination, 0, kOutputSlot);
    body.Raw(std::array{kPopFlags});
    StackSlot(body, 0x8D, true, Stack, frame - 8);
}

void _emitSingleToHalf(StubBodyBuilder& body, const F16cOperands& operands) {
    const auto frame = _selectFrame(operands.Memory, operands.Vector256 ? 16 : 8);
    StackSlot(body, 0x8D, true, Stack, 8 - frame);
    body.Raw(std::array{kPushFlags});
    const auto scratch = _scratchYmm(operands.Source);
    if (operands.Memory)
        StackOperand(body, MoveUnaligned256Store, scratch, 0, kYmmSlot);
    StackOperand(body, operands.Vector256 ? MoveUnaligned256Store : kStore128, operands.Source, 0, kInputSlot);
    _saveGprs(body);
    _emitPs2PhLoop(body, operands.Vector256 ? 8u : 4u, operands.Immediate);
    if (operands.Memory) {
        StackOperand(body, MoveUnaligned128Load, scratch, 0, kOutputSlot);
        _restoreGprs(body);
        Memory(body, operands.Vector256 ? kStore128 : kStore64, scratch, 0, *operands.Memory, frame, operands.Name);
        StackOperand(body, MoveUnaligned256Load, scratch, 0, kYmmSlot);
    } else {
        _restoreGprs(body);
        StackOperand(body, MoveUnaligned128Load, operands.Destination, 0, kOutputSlot);
    }
    body.Raw(std::array{kPopFlags});
    StackSlot(body, 0x8D, true, Stack, frame - 8);
}

}

std::optional<F16cOperands> DecodeF16c(const std::uint8_t* data, const std::size_t length) {
    const auto instruction = Decode(data, length);
    if (!instruction || instruction->Prefix != Prefix66 || instruction->Wide || instruction->Vvvv != 0)
        return std::nullopt;
    if (instruction->Map == Map0F38 && instruction->Opcode == 0x13) {
        if (instruction->End != length) throw CodegenException("VCVTPH2PS has trailing bytes");
        return F16cOperands{"VCVTPH2PS", F16cOperation::HalfToSingle, instruction->Vector256, instruction->Reg, instruction->Rm, 0, instruction->Memory};
    }
    if (instruction->Map == Map0F3A && instruction->Opcode == 0x1D) {
        if (instruction->End >= length) throw CodegenException("VCVTPS2PH is missing its immediate");
        if (instruction->End + 1 != length) throw CodegenException("VCVTPS2PH has trailing bytes");
        return F16cOperands{"VCVTPS2PH", F16cOperation::SingleToHalf, instruction->Vector256, instruction->Rm, instruction->Reg, data[instruction->End], instruction->Memory};
    }
    return std::nullopt;
}

void EmitF16c(StubBodyBuilder& body, const F16cOperands& operands) {
    if (operands.Operation == F16cOperation::HalfToSingle)
        _emitHalfToSingle(body, operands);
    else
        _emitSingleToHalf(body, operands);
}

}
