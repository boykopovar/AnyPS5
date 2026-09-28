#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Sse4aOperands.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <memory>
#include <span>
#include <vector>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

Amd64OnlyMatch _unsupported(const Entry& entry, const std::size_t length) {
    return Amd64OnlyMatch{entry.Name, length, Amd64OnlyLowering::Unsupported, {}, {}, 0};
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

    [[nodiscard]] Amd64OnlyMatch _matchMovnts(const DecodedInstruction& instr, const Entry& entry) const;
    [[nodiscard]] Amd64OnlyMatch _matchSse4a(const DecodedInstruction& instr, const Entry& entry, const Entry& registerFormEntry, std::span<const std::uint8_t> trailing) const;
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
    if (operands.RegisterForm && operands.Insertq)
        return _unsupported(registerFormEntry, instr.Length);
    if (!operands.RegisterForm && trailing.empty()) {
        if (auto inPlace = _lowering.LowerInPlace(operands, instr.Length))
            return Amd64OnlyMatch{entry.Name, instr.Length, Amd64OnlyLowering::InPlace, std::move(*inPlace), {}, 0};
    }
    auto body = _lowering.LowerOutOfLine(operands, trailing);
    const auto& name = operands.RegisterForm ? registerFormEntry.Name : entry.Name;
    return Amd64OnlyMatch{name, instr.Length, Amd64OnlyLowering::Trampoline, {}, std::move(body.Bytes), body.ReturnBranchOffset};
}

std::optional<Amd64OnlyMatch> Amd64OnlyInstructionMatcher::MatchSequence(
    std::span<const std::span<const std::uint8_t>> instructions,
    std::span<const std::uint8_t> trailing
) const {
    if (instructions.empty())
        return std::nullopt;
    std::vector<Sse4aOperands> sequence;
    for (const auto& instruction : instructions) {
        const DecodedInstruction instr{instruction.data(), instruction.size()};
        if (!instr.IsExtrq() && !instr.IsInsertq())
            return std::nullopt;
        const auto operands = DecodeSse4a(instr.Data, instr.Length);
        if (operands.RegisterForm && operands.Insertq)
            return std::nullopt;
        sequence.push_back(operands);
    }
    const auto& first = sequence.front();
    const auto& name = first.RegisterForm ? (first.Insertq ? kInsertqRegisterForm.Name : kExtrqRegisterForm.Name) : (first.Insertq ? kInsertq.Name : kExtrq.Name);
    auto body = _lowering.LowerOutOfLine(std::span<const Sse4aOperands>(sequence), trailing);
    return Amd64OnlyMatch{name, instructions.front().size(), Amd64OnlyLowering::Trampoline, {}, std::move(body.Bytes), body.ReturnBranchOffset};
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

    if (instr.IsMonitorx())
        return _unsupported(kMonitorx, length);

    if (instr.IsMwaitx())
        return _unsupported(kMwaitx, length);

    if (instr.IsClzero())
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
