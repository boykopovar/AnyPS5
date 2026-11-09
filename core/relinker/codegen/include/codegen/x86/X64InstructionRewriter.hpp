#ifndef CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP
#define CODEGEN_X86_X64INSTRUCTIONREWRITER_HPP

#include <codegen/IInstructionRewriter.hpp>
#include <cstdint>

namespace Codegen {

class X64InstructionRewriter : public IInstructionRewriter {
public:
    [[nodiscard]] RewriteResult Rewrite(
        const std::vector<std::uint8_t>& codeSection,
        const RewriteRequest& request
    ) const override;

private:
    enum class ReferenceKind {
        None,
        Rel8Branch,
        Rel32Branch,
        RipRelativeDisp32
    };

    struct ReferenceSite {
        ReferenceKind Kind;
        std::size_t FieldOffset;
    };

    [[nodiscard]] static ReferenceSite ClassifyInstruction(
        const std::uint8_t* data,
        std::size_t length
    );

    [[nodiscard]] static std::int32_t ReadInt32(const std::uint8_t* data);

    static void WriteInt32(std::uint8_t* data, std::int32_t value);

    [[nodiscard]] static std::int64_t AdjustedDelta(
        std::int64_t instrPosition,
        std::int64_t targetPosition,
        std::int64_t cutStart,
        std::int64_t cutEnd,
        std::int64_t sliceSize,
        std::int64_t delta
    );
};

}

#endif
