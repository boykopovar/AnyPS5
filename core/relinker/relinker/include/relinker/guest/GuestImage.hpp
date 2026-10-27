#ifndef RELINKER_GUESTIMAGE_HPP
#define RELINKER_GUESTIMAGE_HPP

#include <codegen/CodegenTypes.hpp>
#include <domain/Types.hpp>
#include <relinker/domain/ISyscallScanner.hpp>
#include <relinker/patching/GamePatch.hpp>
#include <filesystem>
#include <domain/IProgress.hpp>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Relinker {

inline constexpr char GuestSymbolSuffix[] = "#guest";
inline constexpr char GuestModuleSuffix[] = ".guest.prx";
inline constexpr char GuestModulePattern[] = "sce_module/*.prx";
inline constexpr std::uint16_t AbsoluteSection = 0xfff1;
inline constexpr char GuestInitializeExport[] = "__aps5_guest_initialize";

struct GuestSymbol {
    std::string Name;
    std::uint8_t Info;
    std::uint8_t Visibility;
    std::uint16_t Section;
    std::uint64_t Value;
    std::uint64_t Size;
    std::string Library;
    std::string QualifiedName;
};

inline bool IdenticalOrdinaryFunctionExport(const GuestSymbol& left, const GuestSymbol& right) {
    return left.Section != 0 && left.Section < 0xff00 && (left.Info >> 4) != 0 &&
        left.Visibility != 1 && left.Visibility != 2 && (left.Info & 15) == 2 &&
        left.Info == right.Info && left.Visibility == right.Visibility &&
        left.Section == right.Section && left.Value == right.Value && left.Size == right.Size;
}

struct GuestImage {
    std::filesystem::path SourcePath;
    std::string OutputName;
    std::string Soname;
    std::vector<std::string> ModuleNames;
    std::vector<std::uint8_t> Bytes;
    std::vector<Codegen::TrampolineSite> Trampolines;
    std::vector<GamePatch::PatchedRange> PatchedRanges;
    std::vector<Domain::ProgramHeader> Headers;
    std::vector<GuestSymbol> Symbols;
    std::vector<std::string> Dependencies;
    Domain::SysVDynamicSection Dynamic;
    std::vector<std::uint64_t> InitArray;
    std::vector<std::uint64_t> FiniArray;
    std::uint64_t Init = 0;
    bool HasInit = false;
    std::uint64_t Fini = 0;
    std::uint64_t Got = 0;
    bool UsePlatformTlsResolver = true;
    bool Symbolic = false;
    std::vector<std::size_t> NamedImports;
};

class GuestImageReader {
public:
    GuestImage Read(const std::filesystem::path& path, std::vector<std::uint8_t> bytes, bool windows = false) const;
};

struct GuestArtifact {
    std::filesystem::path Path;
    std::vector<std::uint8_t> Bytes;
    std::vector<Domain::CallRegistryEntry> Imports;
    std::filesystem::path SourcePath;
    std::vector<std::string> NeededLibraries;
};

struct GuestModuleInput {
    std::filesystem::path Path;
    std::vector<std::uint8_t> Bytes;
    std::vector<GamePatch::PatchedRange> PatchedRanges;
};

std::vector<std::pair<std::filesystem::path, std::filesystem::path>> RemoveBackupCopies(std::vector<GuestModuleInput>& modules);

class GuestModuleBuilder {
public:
    std::vector<GuestArtifact> Build(const std::filesystem::path& inputPath, const std::filesystem::path& outputPath, Domain::SysVDynamicSection& dynamic, bool windows, bool macos, bool toIntel, bool toRosetta, ISyscallScanner& syscallScanner, bool lazyBinding, const std::string& runPath, const std::set<std::string>& excludedModules, const std::filesystem::path& root, const std::vector<std::filesystem::path>& runtimeDirectories, std::vector<GuestModuleInput> modules, Domain::IProgress& progress) const;
};

}

#endif
