#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/ClzeroLowering.hpp>
#include <codegen/x86/ClzeroOperands.hpp>
#include <codegen/x86/ReciprocalLowering.hpp>
#include <codegen/x86/ReciprocalOperands.hpp>
#include <codegen/x86/Sha256Lowering.hpp>
#include <codegen/x86/Sha256Operands.hpp>
#include <codegen/x86/Sha1Lowering.hpp>
#include <codegen/x86/Sha1Operands.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

constexpr std::size_t kMaxInstructionLength = 15;

Amd64OnlyMatch _unsupported(const Entry& entry, const std::size_t length) {
    return Amd64OnlyMatch{entry.Name, length, Amd64OnlyLowering::Unsupported, {}, {}, 0};
}

const Entry& _sse4aEntry(const Sse4aOperands& operands) {
    if (operands.RegisterForm)
        return operands.Insertq ? kInsertqRegisterForm : kExtrqRegisterForm;
    return operands.Insertq ? kInsertq : kExtrq;
}

const Entry& _reciprocalEntry(const ReciprocalOperands& operands) {
    return operands.Operation == ReciprocalOperation::ReciprocalSquareRoot ? kVrsqrtps : kVrcpps;
}

const Entry& _sha256Entry(const Sha256Operands& operands) {
    switch (operands.Operation) {
    case Sha256Operation::Rnds2:
        return kSha256rnds2;
    case Sha256Operation::Msg1:
        return kSha256msg1;
    case Sha256Operation::Msg2:
        return kSha256msg2;
    }
    return kSha256rnds2;
}

const Entry& _sha1Entry(const Sha1Operands& operands) {
    switch (operands.Operation) {
    case Sha1Operation::Rnds4:
        return kSha1rnds4;
    case Sha1Operation::Nexte:
        return kSha1nexte;
    case Sha1Operation::Msg1:
        return kSha1msg1;
    case Sha1Operation::Msg2:
        return kSha1msg2;
    }
    return kSha1rnds4;
}

Amd64OnlyMatch _trampoline(std::string name, const std::size_t length, LoweredBody body, const bool optional = false) {
    return Amd64OnlyMatch{std::move(name), length, Amd64OnlyLowering::Trampoline, {}, std::move(body.Bytes), body.ReturnBranchOffset, optional, std::move(body.Relocations)};
}

void _moveTrailing(StubBodyBuilder& body, std::span<const std::uint8_t> trailing) {
    const X64InstructionDecoder decoder;
    while (!trailing.empty()) {
        const auto info = decoder.DecodeInstruction(trailing.data(), trailing.size());
        if (info.Length == 0 || info.Length > trailing.size())
            throw CodegenException("Trailing bytes do not split into whole instructions");
        body.Move(trailing.first(info.Length), info.HasRipRelativeDisp ? std::optional<std::size_t>(info.RipRelativeDispOffset) : std::nullopt);
        trailing = trailing.subspan(info.Length);
    }
}

template<typename TEmit>
LoweredBody _outOfLine(const std::size_t length, const std::span<const std::uint8_t> trailing, const TEmit& emit) {
    StubBodyBuilder body;
    emit(body);
    body.Advance(length);
    _moveTrailing(body, trailing);
    return body.Finish();
}

Amd64OnlyMatch _inPlace(const Entry& entry, const std::size_t length, std::vector<std::uint8_t> replacement) {
    while (replacement.size() < length) {
        const auto& nop = kNops[std::min<std::size_t>(length - replacement.size(), std::size(kNops)) - 1];
        replacement.insert(replacement.end(), nop.Bytes, nop.Bytes + nop.Size);
    }
    return Amd64OnlyMatch{entry.Name, length, Amd64OnlyLowering::InPlace, std::move(replacement), {}, 0};
}

bool _isClzeroOpcode(const DecodedInstruction& instr) {
    const auto pos = instr.OpcodeOffset();
    return pos + 2 < instr.Length && instr.Data[pos] == X64OpcodeConstants::TwoByteOpcodeEscape && instr.Data[pos + 1] == X64OpcodeConstants::TwoByteGrp7 && instr.Data[pos + 2] == 0xFC;
}

bool _validWait(const DecodedInstruction& instr) {
    const auto opcode = instr.Data + instr.OpcodeOffset();
    return instr.Length <= kMaxInstructionLength && std::find(instr.Data, opcode, X64OpcodeConstants::PrefixLock) == opcode;
}

class Amd64OnlyInstructionMatcher : public IAmd64OnlyInstructionMatcher {
public:
    [[nodiscard]] std::optional<Amd64OnlyMatch> Match(
        const std::uint8_t* data,
        std::size_t length,
        std::span<const std::uint8_t> trailing = {}
    ) const override;

    [[nodiscard]] std::optional<Amd64OnlyMatch> MatchSequence(
        std::span<const std::span<const std::uint8_t>> instructions,
        std::span<const std::uint8_t> trailing
    ) const override;

private:
    Sse4aLowering _lowering;
    Sha256Lowering _sha256Lowering;
    Sha1Lowering _sha1Lowering;
    ClzeroLowering _clzeroLowering;
    ReciprocalLowering _reciprocalLowering;

    [[nodiscard]] Amd64OnlyMatch _matchMovnts(const DecodedInstruction& instr, const Entry& entry) const;
    [[nodiscard]] Amd64OnlyMatch _matchSse4a(const DecodedInstruction& instr, const Entry& entry, const Entry& registerFormEntry, std::span<const std::uint8_t> trailing) const;
    [[nodiscard]] Amd64OnlyMatch _matchSha256(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const;
    [[nodiscard]] Amd64OnlyMatch _matchSha1(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const;
    [[nodiscard]] Amd64OnlyMatch _matchClzero(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const;
};

Amd64OnlyMatch Amd64OnlyInstructionMatcher::_matchMovnts(const DecodedInstruction& instr, const Entry& entry) const {
    using namespace X64OpcodeConstants;
    const auto opcodeOffset = instr.OpcodeOffset();
    if (opcodeOffset + 2 >= instr.Length)
        throw CodegenException("MOVNTSS/MOVNTSD truncated before its ModRM byte");
    const auto modrm = instr.Data[opcodeOffset + 2];
    if (((modrm >> ModRmModShift) & ModRmModMask) == ModRmModRegister)
        throw CodegenException("MOVNTSS/MOVNTSD with a register operand");
    std::vector<std::uint8_t> replacement(instr.Data, instr.Data + instr.Length);
    replacement[opcodeOffset + 1] = kMovsStoreOpcode;
    return Amd64OnlyMatch{entry.Name, instr.Length, Amd64OnlyLowering::InPlace, std::move(replacement), {}, 0};
}

Amd64OnlyMatch Amd64OnlyInstructionMatcher::_matchSse4a(const DecodedInstruction& instr, const Entry& entry, const Entry& registerFormEntry, std::span<const std::uint8_t> trailing) const {
    const auto operands = DecodeSse4a(instr.Data, instr.Length);
    if (!operands.RegisterForm && trailing.empty()) {
        if (auto inPlace = _lowering.LowerInPlace(operands, instr.Length))
            return Amd64OnlyMatch{entry.Name, instr.Length, Amd64OnlyLowering::InPlace, std::move(*inPlace), {}, 0};
    }
    const auto& name = operands.RegisterForm ? registerFormEntry.Name : entry.Name;
    return _trampoline(name, instr.Length, _outOfLine(instr.Length, trailing, [&](StubBodyBuilder& body) { _lowering.EmitOutOfLine(body, operands); }));
}

Amd64OnlyMatch Amd64OnlyInstructionMatcher::_matchSha256(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const {
    const auto operands = DecodeSha256(instr.Data, instr.Length);
    return _trampoline(_sha256Entry(operands).Name, instr.Length, _outOfLine(instr.Length, trailing, [&](StubBodyBuilder& body) { _sha256Lowering.EmitOutOfLine(body, operands); }));
}

Amd64OnlyMatch Amd64OnlyInstructionMatcher::_matchSha1(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const {
    const auto operands = DecodeSha1(instr.Data, instr.Length);
    return _trampoline(_sha1Entry(operands).Name, instr.Length, _outOfLine(instr.Length, trailing, [&](StubBodyBuilder& body) { _sha1Lowering.EmitOutOfLine(body, operands); }));
}

Amd64OnlyMatch Amd64OnlyInstructionMatcher::_matchClzero(const DecodedInstruction& instr, std::span<const std::uint8_t> trailing) const {
    const auto operands = DecodeClzero(instr.Data, instr.Length);
    return _trampoline(kClzero.Name, instr.Length, _outOfLine(instr.Length, trailing, [&](StubBodyBuilder& body) { _clzeroLowering.EmitOutOfLine(body, operands); }));
}

std::optional<Amd64OnlyMatch> Amd64OnlyInstructionMatcher::MatchSequence(
    std::span<const std::span<const std::uint8_t>> instructions,
    std::span<const std::uint8_t> trailing
) const {
    if (instructions.empty())
        return std::nullopt;
    StubBodyBuilder body;
    const char* name = nullptr;
    for (const auto& instruction : instructions) {
        const DecodedInstruction instr{instruction.data(), instruction.size()};
        if (instr.IsExtrq() || instr.IsInsertq()) {
            const auto operands = DecodeSse4a(instr.Data, instr.Length);
            if (name == nullptr)
                name = _sse4aEntry(operands).Name;
            _lowering.EmitOutOfLine(body, operands);
        } else if (instr.IsSha256()) {
            const auto operands = DecodeSha256(instr.Data, instr.Length);
            if (name == nullptr)
                name = _sha256Entry(operands).Name;
            _sha256Lowering.EmitOutOfLine(body, operands);
        } else if (instr.IsSha1()) {
            const auto operands = DecodeSha1(instr.Data, instr.Length);
            if (name == nullptr)
                name = _sha1Entry(operands).Name;
            _sha1Lowering.EmitOutOfLine(body, operands);
        } else if (instr.IsClzero()) {
            const auto operands = DecodeClzero(instr.Data, instr.Length);
            if (name == nullptr)
                name = kClzero.Name;
            _clzeroLowering.EmitOutOfLine(body, operands);
        } else if (const auto reciprocal = DecodeVexReciprocal(instr.Data, instr.Length)) {
            if (name == nullptr)
                name = _reciprocalEntry(*reciprocal).Name;
            _reciprocalLowering.EmitOutOfLine(body, *reciprocal);
        } else {
            return std::nullopt;
        }
        body.Advance(instr.Length);
    }
    _moveTrailing(body, trailing);
    const bool optional = DecodeVexReciprocal(instructions.front().data(), instructions.front().size()).has_value();
    return _trampoline(name, instructions.front().size(), body.Finish(), optional);
}

std::optional<Amd64OnlyMatch> Amd64OnlyInstructionMatcher::Match(
    const std::uint8_t* data,
    std::size_t length,
    std::span<const std::uint8_t> trailing
) const {
    const DecodedInstruction instr{data, length};

    if (instr.IsMovntss())
        return _matchMovnts(instr, kMovntss);

    if (instr.IsMovntsd())
        return _matchMovnts(instr, kMovntsd);

    if (instr.IsExtrq())
        return _matchSse4a(instr, kExtrq, kExtrqRegisterForm, trailing);

    if (instr.IsInsertq())
        return _matchSse4a(instr, kInsertq, kInsertqRegisterForm, trailing);

    if (instr.IsSha256())
        return _matchSha256(instr, trailing);

    if (instr.IsSha1())
        return _matchSha1(instr, trailing);

    if (instr.IsMonitorx())
        return _validWait(instr) ? _inPlace(kMonitorx, length, {}) : _unsupported(kMonitorx, length);

    if (instr.IsMwaitx())
        return _validWait(instr) ? _inPlace(kMwaitx, length, std::vector<std::uint8_t>(kPause.Bytes, kPause.Bytes + kPause.Size)) : _unsupported(kMwaitx, length);

    if (const auto reciprocal = DecodeVexReciprocal(data, length)) {
        return _trampoline(_reciprocalEntry(*reciprocal).Name, length, _outOfLine(length, trailing, [&](StubBodyBuilder& body) { _reciprocalLowering.EmitOutOfLine(body, *reciprocal); }), true);
    }

    if (instr.IsClzero())
        return _matchClzero(instr, trailing);

    if (_isClzeroOpcode(instr))
        return _unsupported(kClzero, length);

    if (instr.IsRdpru())
        return _unsupported(kRdpru, length);

    if (instr.IsMcommit())
        return _unsupported(kMcommit, length);

    return std::nullopt;
}

}

std::unique_ptr<IAmd64OnlyInstructionMatcher> MakeAmd64OnlyInstructionMatcher() {
    return std::make_unique<Amd64OnlyInstructionMatcher>();
}

}
