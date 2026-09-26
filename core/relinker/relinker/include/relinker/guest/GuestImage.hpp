#ifndef RELINKER_GUESTIMAGE_HPP
#define RELINKER_GUESTIMAGE_HPP

#include <domain/Types.hpp>
#include <filesystem>

namespace Relinker {

struct GuestSymbol {
    std::string Name;
    std::uint8_t Info;
    std::uint8_t Visibility;
    std::uint16_t Section;
    std::uint64_t Value;
    std::uint64_t Size;
};

struct GuestImage {
    std::filesystem::path SourcePath;
    std::string OutputName;
    std::vector<std::uint8_t> Bytes;
    std::vector<Domain::ProgramHeader> Headers;
    std::vector<GuestSymbol> Symbols;
    std::vector<std::string> Dependencies;
    Domain::SysVDynamicSection Dynamic;
    std::uint64_t Init = 0;
    std::uint64_t Fini = 0;
    std::uint64_t Got = 0;
    bool UsePlatformTlsResolver = true;
};

class GuestImageReader {
public:
    GuestImage Read(const std::filesystem::path& path, std::vector<std::uint8_t> bytes) const;
};

struct GuestArtifact {
    std::filesystem::path Path;
    std::vector<std::uint8_t> Bytes;
};

class GuestModuleBuilder {
public:
    std::vector<GuestArtifact> Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, bool windows, bool toIntel, bool skipSyscallCheck, bool lazyBinding, const std::string& runPath) const;
};

}

#endif
