#ifndef CODEGEN_IINSTRUCTIONSCANNER_HPP
#define CODEGEN_IINSTRUCTIONSCANNER_HPP

#include <codegen/CodegenTypes.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Codegen {

class IInstructionScanner {
public:
    virtual ~IInstructionScanner() = default;

    [[nodiscard]] virtual std::vector<InstructionMatch> ScanCodeSection(const std::vector<std::uint8_t>& codeSection, Domain::FileByteOffset codeSectionOffset, Domain::FileByteOffset codeSectionSize) const = 0;
    virtual void VisitCodeSection(const std::vector<std::uint8_t>& codeSection, Domain::FileByteOffset codeSectionOffset, Domain::FileByteOffset codeSectionSize, const std::function<void(const InstructionMatch&)>& visit) const = 0;
};

std::vector<Domain::CodeRange> MergeCodeRanges(std::vector<Domain::CodeRange> ranges);

std::unique_ptr<IInstructionScanner> MakeInstructionScanner(std::function<void(std::uint64_t, std::uint64_t)> progress = {}, std::vector<Domain::CodeRange> unwindRanges = {});

}

#endif
