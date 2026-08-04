#ifndef CODEGEN_IINSTRUCTIONSCANNER_HPP
#define CODEGEN_IINSTRUCTIONSCANNER_HPP

#include <codegen/CodegenTypes.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace Codegen {

class IInstructionScanner {
public:
    virtual ~IInstructionScanner() = default;

    [[nodiscard]] virtual std::vector<InstructionMatch> ScanCodeSection(
        const std::vector<std::uint8_t>& codeSection,
        Domain::FileByteOffset codeSectionOffset,
        Domain::FileByteOffset codeSectionSize
    ) const = 0;
};

std::unique_ptr<IInstructionScanner> MakeInstructionScanner();

}

#endif
