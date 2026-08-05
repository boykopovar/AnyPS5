#ifndef CODEGEN_IAMD64ONLYCONVERTER_HPP
#define CODEGEN_IAMD64ONLYCONVERTER_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace Codegen {

class IAmd64OnlyConverter {
public:
    virtual ~IAmd64OnlyConverter() = default;

    [[nodiscard]] virtual std::vector<std::uint8_t> Convert(
        std::vector<std::uint8_t> fileBytes,
        const std::vector<Domain::ProgramHeader>& codeSegments
    ) const = 0;
};

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter();

}

#endif
