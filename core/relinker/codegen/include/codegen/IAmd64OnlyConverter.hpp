#ifndef CODEGEN_IAMD64ONLYCONVERTER_HPP
#define CODEGEN_IAMD64ONLYCONVERTER_HPP

#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <memory>
#include <functional>
#include <vector>

namespace Codegen {

struct CodeSegmentExtent {
    Domain::VirtualAddress MappedAddress = 0;
    std::size_t CodeSize = 0;
    std::vector<Domain::CodeRange> UnwindRanges;
};

class IAmd64OnlyConverter {
public:
    virtual ~IAmd64OnlyConverter() = default;

    [[nodiscard]] virtual ConvertResult Convert(std::vector<std::uint8_t> fileBytes, const std::vector<Domain::ProgramHeader>& segments, const std::vector<CodeSegmentExtent>& extents = {}) const = 0;
};

std::unique_ptr<IAmd64OnlyConverter> MakeAmd64OnlyConverter(Amd64OnlyTarget target = Amd64OnlyTarget::Intel, std::function<void(std::uint64_t, std::uint64_t)> progress = {});

}

#endif
