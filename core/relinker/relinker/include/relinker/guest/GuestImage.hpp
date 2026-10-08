#ifndef RELINKER_GUESTIMAGE_HPP
#define RELINKER_GUESTIMAGE_HPP

#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <relinker/domain/ISyscallScanner.hpp>
#include <filesystem>
#include <set>
#include <string>

namespace Relinker {

inline constexpr char GuestSymbolSuffix[] = "#guest";
inline constexpr char GuestModuleSuffix[] = ".guest.prx";
inline constexpr std::uint16_t AbsoluteSection = 0xfff1;

struct GuestSymbol {
    std::string Name;
    std::uint8_t Info;
    std::uint8_t Visibility;
    std::uint16_t Section;
    std::uint64_t Value;
    std::uint64_t Size;
    std::string Library;
};

struct GuestImage {
    std::filesystem::path SourcePath;
    std::string OutputName;
    std::string Soname;
    std::vector<std::string> ModuleNames;
    std::vector<std::uint8_t> Bytes;
    std::vector<Codegen::TrampolineSite> Trampolines;
    std::vector<Domain::ProgramHeader> Headers;
    std::vector<GuestSymbol> Symbols;
    std::vector<std::string> Dependencies;
    Domain::SysVDynamicSection Dynamic;
    std::vector<std::uint64_t> InitArray;
    std::vector<std::uint64_t> FiniArray;
    std::uint64_t Init = 0;
    std::uint64_t Fini = 0;
    std::uint64_t Got = 0;
    bool UsePlatformTlsResolver = true;
};

class GuestImageReader {
public:
    std::vector<std::string> ReadModuleNames(const std::vector<std::uint8_t>& bytes) const;
    GuestImage Read(const std::filesystem::path& path, std::vector<std::uint8_t> bytes) const;
};

struct GuestArtifact {
    std::filesystem::path Path;
    std::vector<std::uint8_t> Bytes;
};

class GuestModuleBuilder {
public:
    std::vector<GuestArtifact> Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, bool windows, bool toIntel, ISyscallScanner& syscallScanner, bool lazyBinding, const std::string& runPath, const std::set<std::string>& excludedModules) const;
};

}

#endif
