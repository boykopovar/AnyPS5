#ifndef CODEGEN_X86_RECIPROCALOPERANDS_HPP
#define CODEGEN_X86_RECIPROCALOPERANDS_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

namespace Codegen {

enum class ReciprocalOperation : std::uint8_t {
    Reciprocal,
    ReciprocalSquareRoot
};

struct ReciprocalOperands {
    ReciprocalOperation Operation;
    std::uint8_t Destination;
    std::uint8_t Source;
};

[[nodiscard]] std::optional<ReciprocalOperands> DecodeVexReciprocal(const std::uint8_t* data, std::size_t length);

}

#endif
