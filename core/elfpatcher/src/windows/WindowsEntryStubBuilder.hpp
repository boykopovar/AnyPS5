#ifndef ELFPATCHER_WINDOWS_ENTRYSTUBBUILDER_HPP
#define ELFPATCHER_WINDOWS_ENTRYSTUBBUILDER_HPP

#include "WindowsImportBuilder.hpp"

namespace Elfpatcher::Windows {

struct WindowsEntryStub {
    PeSection Data;
    PeSection Code;
    PeDirectory ExceptionDirectory;
};

class WindowsEntryStubBuilder {
public:
    WindowsEntryStub Build(std::uint32_t dataRva, std::uint32_t entryRva, const WindowsImports& nativeImports, const std::vector<std::string>& libraries, const std::vector<PeImport>& imports, const std::string& runPath) const;
};

}

#endif
