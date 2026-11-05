#ifndef CODEGEN_X86_CLZEROOPERANDS_HPP
#define CODEGEN_X86_CLZEROOPERANDS_HPP

#include <cstddef>
#include <cstdint>

namespace Codegen {

struct ClzeroOperands {
    bool AddressSize32;
};

[[nodiscard]] ClzeroOperands DecodeClzero(const std::uint8_t* data, std::size_t length);

}

#endif
