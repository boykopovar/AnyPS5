#ifndef CODEGEN_X86_SHA256OPERANDS_HPP
#define CODEGEN_X86_SHA256OPERANDS_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Codegen {

enum class Sha256Operation : std::uint8_t {
    Rnds2,
    Msg1,
    Msg2
};

struct Sha256Operands {
    Sha256Operation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
    std::optional<MemoryOperand> Memory;
};

[[nodiscard]] Sha256Operands DecodeSha256(const std::uint8_t* data, std::size_t length);

}

#endif
