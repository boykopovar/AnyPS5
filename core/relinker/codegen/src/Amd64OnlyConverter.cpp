#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <algorithm>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace Codegen {

namespace {

template<typename TOperation>
auto _atFileOffset(const Domain::FileByteOffset base, const TOperation& operation) {
    try {
        return operation();
    } catch (const CodegenException& e) {
        throw CodegenException(e.what(), base + e.FailureOffset);
    }
}

class Amd64OnlyConverter : public IAmd64OnlyConverter {
public:
    [[nodiscard]] ConvertResult Convert(
        std::vector<std::uint8_t> fileBytes,
        const std::vector<Domain::ProgramHeader>& codeSegments
    ) const override;

private:
    struct Pending {
        std::size_t Index;
        InstructionMatch Instruction;
        Amd64OnlyMatch Substitution;
    };

    X64InstructionRewriter _rewriter;
    std::unique_ptr<IAmd64OnlyInstructionMatcher> _matcher = MakeAmd64OnlyInstructionMatcher();
    std::unique_ptr<IInstructionScanner> _scanner = MakeInstructionScanner();

    void _convertSegment(
        std::vector<std::uint8_t>& fileBytes,
        const Domain::ProgramHeader& ph,
        ConvertResult& result
    ) const;

    [[nodiscard]] static std::set<std::uint64_t> _collectBranchTargets(
        const std::vector<std::uint8_t>& seg,
        const std::vector<InstructionMatch>& matches,
        const Domain::ProgramHeader& ph
    );
};

std::set<std::uint64_t> Amd64OnlyConverter::_collectBranchTargets(
    const std::vector<std::uint8_t>& seg,
    const std::vector<InstructionMatch>& matches,
    const Domain::ProgramHeader& ph
) {
    const X64InstructionDecoder decoder;
    std::set<std::uint64_t> targets;
    for (const auto& match : matches) {
        const auto info = decoder.DecodeInstruction(seg.data() + match.Offset, match.Length);
        if (!info.HasBranchTarget || info.HasRipRelativeDisp)
            continue;
        const auto target = static_cast<std::int64_t>(ph.MappedAddress + match.Offset + info.Length) + info.BranchDisp;
        if (target >= 0)
            targets.insert(static_cast<std::uint64_t>(target));
    }
    return targets;
}

void Amd64OnlyConverter::_convertSegment(
    std::vector<std::uint8_t>& fileBytes,
    const Domain::ProgramHeader& ph,
    ConvertResult& result
) const {
    const auto segOffset = static_cast<std::size_t>(ph.Offset);
    const auto segSize = static_cast<std::size_t>(ph.FileSize);
    if (segOffset > fileBytes.size() || segSize > fileBytes.size() - segOffset)
        throw CodegenException("Code segment exceeds the file", ph.Offset);

    std::vector<std::uint8_t> seg(
        fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset),
        fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset + segSize)
    );

    const auto matches = _atFileOffset(ph.Offset, [&] { return _scanner->ScanCodeSection(seg, 0, seg.size()); });

    std::vector<Pending> pending;
    for (std::size_t index = 0; index < matches.size(); ++index) {
        const auto& match = matches[index];
        auto substitution = _atFileOffset(ph.Offset + match.Offset, [&] { return _matcher->Match(seg.data() + match.Offset, match.Length); });
        if (substitution.has_value())
            pending.push_back({index, match, std::move(*substitution)});
    }
    if (pending.empty())
        return;

    const bool needsBranchTargets = std::any_of(pending.begin(), pending.end(), [](const Pending& item) {
        return item.Substitution.Lowering == Amd64OnlyLowering::Trampoline;
    });
    const auto branchTargets = needsBranchTargets ? _collectBranchTargets(seg, matches, ph) : std::set<std::uint64_t>{};

    std::set<std::size_t> consumed;
    for (const auto& item : pending) {
        if (consumed.contains(item.Index))
            continue;
        const auto& match = item.Instruction;
        const auto& substitution = item.Substitution;
        const auto fileOffset = static_cast<Domain::FileByteOffset>(ph.Offset + match.Offset);
        const auto address = static_cast<Domain::VirtualAddress>(ph.MappedAddress + match.Offset);
        std::size_t replacementLength = 0;

        switch (substitution.Lowering) {
        case Amd64OnlyLowering::InPlace: {
            if (substitution.ReplacementBytes.size() != match.Length)
                throw CodegenException("Intel substitution changes the instruction length", fileOffset);
            seg = _atFileOffset(ph.Offset, [&] { return _rewriter.Rewrite(seg, {match.Offset, substitution.ReplacementBytes}).Bytes; });
            replacementLength = substitution.ReplacementBytes.size();
            ++result.ReplacedCount;
            break;
        }
        case Amd64OnlyLowering::Trampoline: {
            std::size_t siteLength = match.Length;
            auto stub = substitution;
            if (siteLength < Amd64OnlySubstitutionTable::kJmpRel32.Size) {
                const X64InstructionDecoder decoder;
                std::vector<std::span<const std::uint8_t>> sequence{std::span<const std::uint8_t>(seg.data() + match.Offset, match.Length)};
                std::size_t trailingBytes = 0;
                for (auto next = item.Index + 1; siteLength < Amd64OnlySubstitutionTable::kJmpRel32.Size; ++next) {
                    if (next >= matches.size() || matches[next].Offset != match.Offset + siteLength)
                        throw CodegenException("AMD-only instruction too short for a jump and not followed by an instruction", fileOffset);
                    const auto& following = matches[next];
                    const std::span<const std::uint8_t> bytes(seg.data() + following.Offset, following.Length);
                    const auto info = decoder.DecodeInstruction(bytes.data(), bytes.size());
                    const bool amdOnly = _matcher->Match(bytes.data(), bytes.size()).has_value();
                    if (amdOnly && trailingBytes == 0) {
                        sequence.push_back(bytes);
                        consumed.insert(next);
                    } else if (amdOnly || info.FlowKind != ControlFlowKind::Sequential || info.HasRipRelativeDisp || info.HasBranchTarget) {
                        throw CodegenException("AMD-only instruction too short for a jump is followed by an instruction that cannot move", ph.Offset + following.Offset);
                    } else {
                        trailingBytes += following.Length;
                    }
                    siteLength += following.Length;
                }
                const std::span<const std::uint8_t> trailing(seg.data() + match.Offset + siteLength - trailingBytes, trailingBytes);
                auto relocated = _atFileOffset(fileOffset, [&] { return _matcher->MatchSequence(sequence, trailing); });
                if (!relocated.has_value() || relocated->Lowering != Amd64OnlyLowering::Trampoline)
                    throw CodegenException("AMD-only instruction sequence has no out-of-line lowering", fileOffset);
                stub = std::move(*relocated);
            }
            const auto hit = branchTargets.upper_bound(address);
            if (hit != branchTargets.end() && *hit < address + siteLength)
                throw CodegenException("Branch enters an AMD-only instruction", ph.Offset + (*hit - ph.MappedAddress));
            const auto begin = seg.begin() + static_cast<std::ptrdiff_t>(match.Offset);
            result.Trampolines.push_back({
                fileOffset,
                address,
                siteLength,
                std::vector<std::uint8_t>(begin, begin + static_cast<std::ptrdiff_t>(siteLength)),
                stub.StubBody,
                stub.ReturnBranchOffset
            });
            replacementLength = stub.StubBody.size();
            break;
        }
        case Amd64OnlyLowering::Unsupported:
            throw CodegenException("AMD-only instruction without Intel lowering: " + substitution.InstructionName, fileOffset);
        }

        result.Reports.push_back({substitution.InstructionName, fileOffset, match.Length, replacementLength, substitution.Lowering});
    }

    if (seg.size() != segSize)
        throw CodegenException("Code segment size changed during Intel conversion", ph.Offset);
    std::copy(seg.begin(), seg.end(), fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset));
}

ConvertResult Amd64OnlyConverter::Convert(
    std::vector<std::uint8_t> fileBytes,
    const std::vector<Domain::ProgramHeader>& codeSegments
) const {
    ConvertResult result{{}, 0, {}, {}};
    for (const auto& ph : codeSegments)
        _convertSegment(fileBytes, ph, result);
    result.Bytes = std::move(fileBytes);
    return result;
}

}

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter() {
    return std::make_unique<Amd64OnlyConverter>();
}

}
