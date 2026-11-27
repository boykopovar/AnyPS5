#ifndef ELFPATCHER_WINDOWS_ICONRESOURCEBUILDER_HPP
#define ELFPATCHER_WINDOWS_ICONRESOURCEBUILDER_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <filesystem>
#include <optional>

namespace Elfpatcher::Windows {

class WindowsIconResourceBuilder {
public:
    PeDirectory Build(const std::optional<std::vector<std::uint8_t>>& png, const std::filesystem::path& iconPath, std::vector<PeSection>& sections, std::uint32_t nextRva) const;
};

}

#endif
