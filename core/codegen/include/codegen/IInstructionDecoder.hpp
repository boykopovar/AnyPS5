#ifndef CODEGEN_IINSTRUCTIONDECODER_HPP
#define CODEGEN_IINSTRUCTIONDECODER_HPP

#include <codegen/CodegenTypes.hpp>
#include <cstdint>

namespace Codegen {

class IInstructionDecoder {
public:
    virtual ~IInstructionDecoder() = default;

    [[nodiscard]] virtual DecodedInstruction Decode(
        const std::uint8_t* data,
        std::size_t available
    ) const = 0;
};

}

#endif
