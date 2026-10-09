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

    struct Segment {
        Domain::ProgramHeader Header;
        std::vector<std::uint8_t> Bytes;
        std::vector<InstructionMatch> Instructions;
        std::vector<Pending> Substitutions;
    };

    X64InstructionRewriter _rewriter;
    std::unique_ptr<IAmd64OnlyInstructionMatcher> _matcher = MakeAmd64OnlyInstructionMatcher();
    std::unique_ptr<IInstructionScanner> _scanner = MakeInstructionScanner();

    void _convertSegment(
        std::vector<std::uint8_t>& fileBytes,
        Segment& segment,
        const std::set<std::uint64_t>& branchTargets,
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
    Segment& segment,
    const std::set<std::uint64_t>& branchTargets,
    ConvertResult& result
) const {
    if (segment.Substitutions.empty())
        return;
    const auto& ph = segment.Header;
    const auto segOffset = static_cast<std::size_t>(ph.Offset);
    const auto segSize = static_cast<std::size_t>(ph.FileSize);
    auto& seg = segment.Bytes;
    const auto& matches = segment.Instructions;

    std::set<std::size_t> consumed;
    for (const auto& item : segment.Substitutions) {
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
            std::vector<std::size_t> taken;
            try {
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
                            taken.push_back(next);
                        } else if (amdOnly || info.FlowKind != ControlFlowKind::Sequential || info.HasBranchTarget) {
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
                    stub.ReturnBranchOffset,
                    stub.Relocations
                });
                replacementLength = stub.StubBody.size();
                consumed.insert(taken.begin(), taken.end());
            } catch (const CodegenException&) {
                if (!substitution.Optional)
                    throw;
                ++result.KeptCount;
                result.Reports.push_back({substitution.InstructionName, fileOffset, match.Length, 0, Amd64OnlyLowering::Kept});
                continue;
            }
            break;
        }
        case Amd64OnlyLowering::Kept:
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
    std::vector<const Domain::ProgramHeader*> fileRanges;
    for (const auto& ph : codeSegments) {
        if (ph.Offset > fileBytes.size() || ph.FileSize > fileBytes.size() - ph.Offset)
            throw CodegenException("Code segment exceeds the file", ph.Offset);
        if (ph.FileSize != 0)
            fileRanges.push_back(&ph);
    }
    std::sort(fileRanges.begin(), fileRanges.end(), [](const auto* left, const auto* right) { return left->Offset < right->Offset; });
    for (std::size_t index = 1; index < fileRanges.size(); ++index) {
        const auto& previous = *fileRanges[index - 1];
        const auto& current = *fileRanges[index];
        if (current.Offset - previous.Offset < previous.FileSize)
            throw CodegenException("Overlapping code segment file ranges are not supported", current.Offset);
    }
    std::vector<Segment> segments;
    bool needsBranchTargets = false;
    for (const auto& ph : codeSegments) {
        const auto segOffset = static_cast<std::size_t>(ph.Offset);
        const auto segSize = static_cast<std::size_t>(ph.FileSize);
        std::vector<std::uint8_t> seg(
            fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset),
            fileBytes.begin() + static_cast<std::ptrdiff_t>(segOffset + segSize)
        );
        auto matches = _atFileOffset(ph.Offset, [&] { return _scanner->ScanCodeSection(seg, 0, seg.size()); });
        std::vector<Pending> pending;
        for (std::size_t index = 0; index < matches.size(); ++index) {
            const auto& match = matches[index];
            auto substitution = _atFileOffset(ph.Offset + match.Offset, [&] { return _matcher->Match(seg.data() + match.Offset, match.Length); });
            if (substitution.has_value()) {
                needsBranchTargets |= substitution->Lowering == Amd64OnlyLowering::Trampoline;
                pending.push_back({index, match, std::move(*substitution)});
            }
        }
        segments.push_back({ph, std::move(seg), std::move(matches), std::move(pending)});
    }
    std::set<std::uint64_t> branchTargets;
    if (needsBranchTargets) {
        for (const auto& segment : segments) {
            const auto targets = _collectBranchTargets(segment.Bytes, segment.Instructions, segment.Header);
            branchTargets.insert(targets.begin(), targets.end());
        }
    }
    ConvertResult result{};
    for (auto& segment : segments)
        _convertSegment(fileBytes, segment, branchTargets, result);
    result.Bytes = std::move(fileBytes);
    return result;
}

}

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter() {
    return std::make_unique<Amd64OnlyConverter>();
}

}
