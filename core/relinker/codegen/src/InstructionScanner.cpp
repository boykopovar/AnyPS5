#include <codegen/IInstructionScanner.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <algorithm>
#include <memory>
#include <limits>
#include <string>

namespace Codegen {

std::vector<Domain::CodeRange> MergeCodeRanges(std::vector<Domain::CodeRange> ranges) {
    std::erase_if(ranges, [](const Domain::CodeRange& range) { return range.End <= range.Begin; });
    std::sort(ranges.begin(), ranges.end(), [](const Domain::CodeRange& left, const Domain::CodeRange& right) { return left.Begin < right.Begin; });
    std::vector<Domain::CodeRange> merged;
    for (const auto& range : ranges) {
        if (!merged.empty() && range.Begin <= merged.back().End)
            merged.back().End = std::max(merged.back().End, range.End);
        else
            merged.push_back(range);
    }
    return merged;
}

class InstructionScanner : public IInstructionScanner {
public:
    InstructionScanner(std::function<void(std::uint64_t, std::uint64_t)> progress, std::vector<Domain::CodeRange> unwindRanges) : progress(std::move(progress)), unwindRanges(MergeCodeRanges(std::move(unwindRanges))) {}
    [[nodiscard]] std::vector<InstructionMatch> ScanCodeSection(const std::vector<std::uint8_t>& codeSection, Domain::FileByteOffset codeSectionOffset, Domain::FileByteOffset codeSectionSize) const override;
    void VisitCodeSection(const std::vector<std::uint8_t>& codeSection, Domain::FileByteOffset codeSectionOffset, Domain::FileByteOffset codeSectionSize, const std::function<void(const InstructionMatch&)>& visit) const override;

private:
    std::function<void(std::uint64_t, std::uint64_t)> progress;
    std::vector<Domain::CodeRange> unwindRanges;
};

template<typename TVisitor>
void ScanCodeSection(const std::vector<std::uint8_t>& codeSection, const Domain::FileByteOffset codeSectionOffset, const Domain::FileByteOffset codeSectionSize, const std::function<void(std::uint64_t, std::uint64_t)>& progress, const std::vector<Domain::CodeRange>& unwindRanges, TVisitor&& visit) {
    if (codeSectionSize > codeSection.size()) throw CodegenException("Code section exceeds the input buffer", codeSectionOffset);
    if (codeSectionSize > std::numeric_limits<Domain::FileByteOffset>::max() - codeSectionOffset) throw CodegenException("Code section offset overflow", codeSectionOffset);
    const auto limit = static_cast<std::size_t>(codeSectionSize);

    const X64InstructionDecoder decoder;
    std::size_t i = 0;
    std::size_t range = 0;

    if (progress) progress(0, limit);
    std::size_t lastReport = 0;
    while (i < limit) {
        while (range < unwindRanges.size() && unwindRanges[range].End <= i) ++range;
        const bool outsideUnwindRanges = !unwindRanges.empty() && (range == unwindRanges.size() || i < unwindRanges[range].Begin);
        const std::size_t nextUnwindRange = outsideUnwindRanges && range < unwindRanges.size() ? unwindRanges[range].Begin : limit;
        const std::uint8_t* cursor = codeSection.data() + i;
        const std::size_t available = limit - i;

        std::size_t length = 0;
        try {
            length = decoder.Decode(cursor, available);
        } catch (const CodegenException& e) {
            if (std::all_of(cursor, cursor + available, [](const std::uint8_t b) { return b == 0; })) break;
            if (!outsideUnwindRanges) throw CodegenException(std::string("Cannot decode instruction: ") + e.what(), codeSectionOffset + i);
            ++i;
            continue;
        }

        if (length > nextUnwindRange - i) {
            i = nextUnwindRange;
            continue;
        }

        const InstructionMatch match{codeSectionOffset + i, length};
        visit(match);

        i += length;
        if (progress && i - lastReport >= 65536) {
            progress(i, limit);
            lastReport = i;
        }
    }

    if (progress) progress(limit, limit);
}

std::vector<InstructionMatch> InstructionScanner::ScanCodeSection(const std::vector<std::uint8_t>& codeSection, const Domain::FileByteOffset codeSectionOffset, const Domain::FileByteOffset codeSectionSize) const {
    std::vector<InstructionMatch> matches;
    Codegen::ScanCodeSection(codeSection, codeSectionOffset, codeSectionSize, progress, unwindRanges, [&](const InstructionMatch& match) { matches.push_back(match); });
    return matches;
}

void InstructionScanner::VisitCodeSection(const std::vector<std::uint8_t>& codeSection, const Domain::FileByteOffset codeSectionOffset, const Domain::FileByteOffset codeSectionSize, const std::function<void(const InstructionMatch&)>& visit) const {
    Codegen::ScanCodeSection(codeSection, codeSectionOffset, codeSectionSize, progress, unwindRanges, visit);
}

std::unique_ptr<IInstructionScanner> MakeInstructionScanner(std::function<void(std::uint64_t, std::uint64_t)> progress, std::vector<Domain::CodeRange> unwindRanges) {
    return std::make_unique<InstructionScanner>(std::move(progress), std::move(unwindRanges));
}

}
