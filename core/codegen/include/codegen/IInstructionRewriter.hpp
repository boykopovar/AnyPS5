#ifndef CODEGEN_IINSTRUCTIONREWRITER_HPP
#define CODEGEN_IINSTRUCTIONREWRITER_HPP

#include <codegen/CodegenTypes.hpp>
#include <vector>

namespace Codegen {

class IInstructionRewriter {
public:
    virtual ~IInstructionRewriter() = default;

    [[nodiscard]] virtual RewriteResult Rewrite(
        const std::vector<std::uint8_t>& codeSection,
        const RewriteRequest& request
    ) const = 0;
};

}

#endif
