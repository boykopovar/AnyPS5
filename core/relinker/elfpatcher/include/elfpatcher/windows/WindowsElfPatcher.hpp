#ifndef ELFPATCHER_WINDOWS_WINDOWSELFPATCHER_HPP
#define ELFPATCHER_WINDOWS_WINDOWSELFPATCHER_HPP

#include <elfpatcher/general/IElfPatcher.hpp>
#include <filesystem>

namespace Elfpatcher::Windows {

class WindowsPePatcher : public IElfPatcher {
public:
    explicit WindowsPePatcher(bool windowsGui = false, std::filesystem::path iconPath = {});

    std::vector<std::uint8_t> Patch(const std::vector<std::uint8_t>& sourceElf, const std::vector<Domain::ProgramHeader>& originalHeaders, const Domain::SysVDynamicSection& dynamicSection, std::uint64_t originalPltGotVaddr, const std::string& runPath, bool lazyBinding, bool dependencyDiagnostics, const std::vector<Codegen::TrampolineSite>& trampolines) override;

private:
    bool _windowsGui;
    std::filesystem::path _iconPath;
};

}

#endif
