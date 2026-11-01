#ifndef ELFPATCHER_MACOS_MACHOPATCHER_HPP
#define ELFPATCHER_MACOS_MACHOPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace Elfpatcher::MacOs {

class MacOsMachOPatcher final : public IElfPatcher {
public:
    explicit MacOsMachOPatcher(std::optional<std::uint64_t> fixedBase = std::nullopt, std::function<void(std::string_view)> message = {}, bool traceImports = false);

    std::vector<std::uint8_t> Patch(
        std::vector<std::uint8_t> sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection,
        std::uint64_t originalPltGotVaddr,
        const std::string& runPath,
        bool lazyBinding,
        bool dependencyDiagnostics,
        const std::vector<Codegen::TrampolineSite>& trampolines
    ) override;

private:
    std::optional<std::uint64_t> fixedBase;
    std::function<void(std::string_view)> message;
    bool traceImports;
};

}

#endif
