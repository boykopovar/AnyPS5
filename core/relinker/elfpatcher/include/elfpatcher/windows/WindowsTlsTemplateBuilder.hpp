#ifndef ELFPATCHER_WINDOWSTLSTEMPLATEBUILDER_HPP
#define ELFPATCHER_WINDOWSTLSTEMPLATEBUILDER_HPP

#include <elfpatcher/windows/WindowsLoadImage.hpp>

namespace Elfpatcher::Windows {

class WindowsTlsTemplateBuilder {
public:
    std::vector<std::uint8_t> Build(const Domain::ProgramHeader& tls, const WindowsLoadImage& image, const std::vector<PeSection>& sections, std::uint32_t templateRva, std::vector<std::uint32_t>& relocations) const;
};

}

#endif
