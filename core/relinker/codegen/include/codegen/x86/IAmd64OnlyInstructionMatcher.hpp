#ifndef CODEGEN_X86_IAMD64ONLYINSTRUCTIONMATCHER_HPP
#define CODEGEN_X86_IAMD64ONLYINSTRUCTIONMATCHER_HPP

#include <codegen/x86/Amd64OnlySubstitutionTypes.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace Codegen {

class IAmd64OnlyInstructionMatcher {
public:
    virtual ~IAmd64OnlyInstructionMatcher() = default;

    [[nodiscard]] virtual std::optional<Amd64OnlyMatch> Match(
        const std::uint8_t* data,
        std::size_t length,
        std::span<const std::uint8_t> trailing = {}
    ) const = 0;

    [[nodiscard]] virtual std::optional<Amd64OnlyMatch> MatchSequence(
        std::span<const std::span<const std::uint8_t>> instructions,
        std::span<const std::uint8_t> trailing
    ) const = 0;
};

std::unique_ptr<IAmd64OnlyInstructionMatcher> MakeAmd64OnlyInstructionMatcher();

}

#endif
