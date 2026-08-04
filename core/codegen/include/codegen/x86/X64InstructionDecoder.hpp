#ifndef CODEGEN_X86_X64INSTRUCTIONDECODER_HPP
#define CODEGEN_X86_X64INSTRUCTIONDECODER_HPP

#include <codegen/IInstructionDecoder.hpp>

namespace Codegen {

class X64InstructionDecoder : public IInstructionDecoder {
public:
    [[nodiscard]] DecodedInstruction Decode(
        const std::uint8_t* data,
        std::size_t available
    ) const override;
};

}

#endif
