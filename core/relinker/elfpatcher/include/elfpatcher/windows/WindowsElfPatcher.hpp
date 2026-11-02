#ifndef ELFPATCHER_WINDOWS_WINDOWSELFPATCHER_HPP
#define ELFPATCHER_WINDOWS_WINDOWSELFPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <optional>
#include <functional>
#include <filesystem>
#include <optional>
#include <string_view>

namespace Elfpatcher::Windows {

class WindowsPePatcher : public IElfPatcher {
public:
    explicit WindowsPePatcher(bool windowsGui = false, std::optional<std::vector<std::uint8_t>> icon = std::nullopt, std::filesystem::path iconName = "icon0.png", std::function<void(std::uint64_t, std::uint64_t)> progress = {}, std::optional<std::uint64_t> fixedBase = std::nullopt, bool traceImports = false, std::function<void(std::string_view)> message = {});
    const std::string& GetDiagnostics() const;

    std::vector<std::uint8_t> Patch(std::vector<std::uint8_t> sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, std::uint64_t originalPltGotVaddr, const std::string& runPath, bool lazyBinding, bool dependencyDiagnostics, const std::vector<Codegen::TrampolineSite>& trampolines) override;

private:
    std::function<void(std::uint64_t, std::uint64_t)> progress;
    bool windowsGui;
    std::optional<std::vector<std::uint8_t>> icon;
    std::string diagnostics;
    std::filesystem::path iconName;
    std::optional<std::uint64_t> fixedBase;
    bool traceImports;
    std::function<void(std::string_view)> message;
};

}

#endif
