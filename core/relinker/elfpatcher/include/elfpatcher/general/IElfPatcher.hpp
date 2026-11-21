#ifndef ELFPATCHER_IELFPATCHER_HPP
#define ELFPATCHER_IELFPATCHER_HPP

#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Elfpatcher {

class IElfPatcher {
public:
    virtual ~IElfPatcher() = default;

    virtual std::vector<std::uint8_t> Patch(
        std::vector<std::uint8_t> sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics,
        const std::vector<Codegen::TrampolineSite>& trampolines,
        std::optional<std::uint64_t> hostCpuFeatures = std::nullopt
    ) = 0;
};

}

#endif
