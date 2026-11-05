#ifndef CODEGEN_X86_SSE4AOPERANDS_HPP
#define CODEGEN_X86_SSE4AOPERANDS_HPP

#include <cstddef>
#include <cstdint>

namespace Codegen {

struct Sse4aOperands {
    bool Insertq;
    bool RegisterForm;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::uint8_t Length;
    std::uint8_t Index;
};

[[nodiscard]] Sse4aOperands DecodeSse4a(const std::uint8_t* data, std::size_t length);

}

#endif
