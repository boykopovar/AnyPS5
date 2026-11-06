#ifndef ELFPATCHER_WINDOWS_RESOURCEBUILDER_HPP
#define ELFPATCHER_WINDOWS_RESOURCEBUILDER_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>

namespace Elfpatcher::Windows {

struct WindowsResources {
    PeSection Section;
    PeDirectory Directory;
};

class WindowsResourceBuilder {
public:
    WindowsResources Build(const std::vector<std::uint8_t>& icon, std::uint32_t sectionRva) const;
};

}

#endif
