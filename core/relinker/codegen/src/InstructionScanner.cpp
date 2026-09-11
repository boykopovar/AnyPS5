#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <memory>

namespace Codegen {

class InstructionScanner : public IInstructionScanner {
public:
    [[nodiscard]] std::vector<InstructionMatch> ScanCodeSection(
        const std::vector<std::uint8_t>& codeSection,
        Domain::FileByteOffset codeSectionOffset,
        Domain::FileByteOffset codeSectionSize
    ) const override;
};

std::vector<InstructionMatch> InstructionScanner::ScanCodeSection(
    const std::vector<std::uint8_t>& codeSection,
    const Domain::FileByteOffset codeSectionOffset,
    const Domain::FileByteOffset codeSectionSize
) const {
    const std::size_t limit = std::min(codeSection.size(), static_cast<std::size_t>(codeSectionSize));

    const X64InstructionDecoder decoder;
    std::vector<InstructionMatch> matches;
    std::size_t i = 0;

    while (i < limit) {
        const std::uint8_t* cursor = codeSection.data() + i;
        const std::size_t available = limit - i;

        const std::size_t length = decoder.Decode(cursor, available);

        matches.push_back({codeSectionOffset + i, length});

        i += length;
    }

    return matches;
}

std::unique_ptr<IInstructionScanner> MakeInstructionScanner() {
    return std::make_unique<InstructionScanner>();
}

}
