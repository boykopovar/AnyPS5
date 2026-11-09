#ifndef ELFPATCHER_WINDOWS_IMPORTBUILDER_HPP
#define ELFPATCHER_WINDOWS_IMPORTBUILDER_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <domain/Types.hpp>
#include <map>

namespace Elfpatcher::Windows {

struct WindowsImports {
    PeSection Section;
    PeDirectory Directory;
    PeDirectory AddressTable;
    std::map<std::string, std::uint32_t> Functions;
};

class WindowsImportBuilder {
public:
    WindowsImports Build(std::uint32_t sectionRva) const;
    std::vector<std::string> ReadLibraries(const Domain::SysVDynamicSection& dynamicSection) const;
};

}

#endif
