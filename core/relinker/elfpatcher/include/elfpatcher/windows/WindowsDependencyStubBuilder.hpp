#ifndef ELFPATCHER_WINDOWS_DEPENDENCYSTUBBUILDER_HPP
#define ELFPATCHER_WINDOWS_DEPENDENCYSTUBBUILDER_HPP

#include <elfpatcher/windows/WindowsImportBuilder.hpp>
#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <array>
#include <map>

namespace Elfpatcher::Windows {

struct WindowsDependencyStub {
    std::uint32_t EntryRva;
    std::vector<std::array<std::uint32_t, 3>> Functions;
};

class WindowsDependencyStubBuilder {
public:
    explicit WindowsDependencyStubBuilder(PeSection& data);
    WindowsDependencyStub Build(WindowsStubEmitter& code, const WindowsImports& imports) const;

private:
    std::map<std::string, std::uint32_t> storage;
    std::uint32_t unwindRva;
};

}

#endif
