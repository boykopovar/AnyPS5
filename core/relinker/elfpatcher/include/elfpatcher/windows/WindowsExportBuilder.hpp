#ifndef ELFPATCHER_WINDOWS_EXPORTBUILDER_HPP
#define ELFPATCHER_WINDOWS_EXPORTBUILDER_HPP

#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <map>

namespace Elfpatcher::Windows {

class WindowsExportBuilder {
public:
    PeDirectory Build(const std::map<std::string, std::uint32_t>& exports, const std::string& name, std::vector<PeSection>& sections, std::uint32_t& nextRva) const;
    PeDirectory BuildExecutable(const Domain::SysVDynamicSection& dynamic, const WindowsLoadImage& image, const std::vector<Domain::ProgramHeader>& headers, std::uint32_t tlsIndex, std::vector<std::uint32_t>& relocations, const std::string& name, std::vector<PeSection>& sections, std::uint32_t& nextRva) const;
};

}

#endif
