#include <codegen/IAmd64OnlyConverter.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/x86/IAmd64OnlyInstructionMatcher.hpp>
#include <memory>

namespace Codegen {

namespace {

class Amd64OnlyConverter : public IAmd64OnlyConverter {
public:
    [[nodiscard]] ConvertResult Convert(
        std::vector<std::uint8_t> fileBytes,
        const std::vector<Domain::ProgramHeader>& codeSegments
    ) const override;

private:
    X64InstructionRewriter _rewriter;
    std::unique_ptr<IAmd64OnlyInstructionMatcher> _matcher = MakeAmd64OnlyInstructionMatcher();
    std::unique_ptr<IInstructionScanner> _scanner = MakeInstructionScanner();

    std::size_t _convertSegment(
        std::vector<std::uint8_t>& fileBytes,
        const Domain::ProgramHeader& ph
    ) const;
};

std::size_t Amd64OnlyConverter::_convertSegment(
    std::vector<std::uint8_t>& fileBytes,
    const Domain::ProgramHeader& ph
) const {
    const auto segOffset = static_cast<std::size_t>(ph.Offset);
    const auto segSize = static_cast<std::size_t>(ph.FileSize);

    std::vector<std::uint8_t> seg(
        fileBytes.begin() + segOffset,
        fileBytes.begin() + segOffset + segSize
    );

    const auto matches = _scanner->ScanCodeSection(seg, 0, seg.size());
    std::int64_t shift = 0;
    std::size_t count = 0;

    for (const auto& match : matches) {
        const auto shiftedOffset = static_cast<std::int64_t>(match.Offset) + shift;
        const auto offset = static_cast<std::size_t>(shiftedOffset);
        const auto result = _matcher->Match(seg.data() + offset, match.Length);
        if (!result.has_value()) {
            continue;
        }
        const RewriteRequest request{
            static_cast<Domain::FileByteOffset>(offset),
            result->ReplacementBytes
        };
        auto rewriteResult = _rewriter.Rewrite(seg, request);
        shift += static_cast<std::int64_t>(result->ReplacementBytes.size()) -
                 static_cast<std::int64_t>(match.Length);
        seg = std::move(rewriteResult.Bytes);
        ++count;
    }

    const auto copySize = std::min(seg.size(), segSize);
    std::copy(seg.begin(), seg.begin() + copySize, fileBytes.begin() + segOffset);
    return count;
}

ConvertResult Amd64OnlyConverter::Convert(
    std::vector<std::uint8_t> fileBytes,
    const std::vector<Domain::ProgramHeader>& codeSegments
) const {
    std::size_t total = 0;
    for (const auto& ph : codeSegments) {
        total += _convertSegment(fileBytes, ph);
    }
    return {std::move(fileBytes), total};
}

}

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter() {
    return std::make_unique<Amd64OnlyConverter>();
}

}
