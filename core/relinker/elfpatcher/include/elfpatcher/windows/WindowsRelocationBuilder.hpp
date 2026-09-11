#ifndef ELFPATCHER_WINDOWS_RELOCATIONBUILDER_HPP
#define ELFPATCHER_WINDOWS_RELOCATIONBUILDER_HPP

#include <elfpatcher/windows/WindowsLoadImage.hpp>

namespace Elfpatcher::Windows {

class WindowsRelocationBuilder {
public:
    PeRelocations Apply(WindowsLoadImage& image, const Domain::SysVDynamicSection& dynamicSection) const;
    std::vector<std::uint8_t> BuildBaseRelocations(const std::vector<std::uint32_t>& targets) const;
};

}

#endif
