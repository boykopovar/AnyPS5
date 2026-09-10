#ifndef ELFPATCHER_WINDOWS_WINDOWSTLSBUILDER_HPP
#define ELFPATCHER_WINDOWS_WINDOWSTLSBUILDER_HPP

#include "WindowsLoadImage.hpp"

namespace Elfpatcher::Windows {

class WindowsTlsBuilder {
public:
    PeDirectory Build(const std::vector<std::uint8_t>& source, const std::vector<Domain::ProgramHeader>& headers, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::vector<std::uint32_t>& relocations, std::uint32_t& nextRva) const;
};

}

#endif
