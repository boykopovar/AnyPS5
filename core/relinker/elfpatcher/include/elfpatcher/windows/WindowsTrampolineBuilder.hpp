#ifndef ELFPATCHER_WINDOWS_WINDOWSTRAMPOLINEBUILDER_HPP
#define ELFPATCHER_WINDOWS_WINDOWSTRAMPOLINEBUILDER_HPP

#include <codegen/CodegenTypes.hpp>
#include <elfpatcher/windows/WindowsLoadImage.hpp>

namespace Elfpatcher::Windows {

class WindowsTrampolineBuilder {
public:
    void Build(const std::vector<Codegen::TrampolineSite>& sites, const WindowsLoadImage& image, std::vector<PeSection>& sections, std::uint32_t& nextRva) const;
};

}

#endif
