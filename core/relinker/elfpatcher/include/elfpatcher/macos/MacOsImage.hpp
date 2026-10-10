#ifndef ELFPATCHER_MACOS_IMAGE_HPP
#define ELFPATCHER_MACOS_IMAGE_HPP

#include <elfpatcher/windows/WindowsLoadImage.hpp>
#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Elfpatcher::MacOs {

struct MacOsBind {
    std::string Symbol;
    std::uint32_t Rva;
    std::int64_t Addend = 0;
};

struct MacOsExport {
    std::string Symbol;
    std::uint32_t Rva;
};

struct MacOsTlsImport {
    std::string Symbol;
    std::uint32_t Rva;
    bool Module;
    std::int64_t Addend = 0;
};

struct MacOsTlsExport {
    std::string Symbol;
    std::uint64_t Offset;
};

struct MacOsImageInput {
    bool Executable = true;
    std::string InstallName;
    const std::vector<std::uint8_t>* Source = nullptr;
    const std::vector<Domain::ProgramHeader>* Headers = nullptr;
    Windows::WindowsLoadImage* Image = nullptr;
    std::vector<std::uint32_t> Rebases;
    std::vector<MacOsBind> Binds;
    std::vector<MacOsExport> Exports;
    std::vector<std::string> Dylibs;
    std::vector<std::string> RunPaths;
    std::uint32_t EntryRva = 0;
    std::uint32_t InitRva = 0;
    std::uint32_t FiniRva = 0;
    std::vector<std::uint32_t> InitArrayRvas;
    std::vector<std::uint32_t> FiniArrayRvas;
    std::vector<std::uint32_t> TlsModuleSlots;
    std::vector<MacOsTlsImport> TlsImports;
    std::vector<MacOsTlsExport> TlsExports;
    const std::vector<Codegen::TrampolineSite>* Trampolines = nullptr;
};

std::uint64_t GuestTlsBlockSize(const Domain::ProgramHeader& tls);

std::vector<std::uint8_t> WriteMacOsImage(MacOsImageInput& input);

std::string MachOLoadPath(const std::string& path, const std::string& origin);

std::string MachODependency(const std::string& name, const std::string& origin);

std::vector<std::string> ReadNeededLibraries(const Domain::SysVDynamicSection& dynamicSection);

}

#endif
