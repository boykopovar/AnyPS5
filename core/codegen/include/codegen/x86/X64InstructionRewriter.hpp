#ifndef CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP
#define CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP

#include <codegen/IInstructionRewriter.hpp>

namespace Codegen {

class X64InstructionRewriter : public IInstructionRewriter {
public:
    [[nodiscard]] RewriteResult Rewrite(
        const std::vector<std::uint8_t>& codeSection,
        const RewriteRequest& request
    ) const override;
};

}

#endif
