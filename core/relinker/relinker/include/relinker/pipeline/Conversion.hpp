#pragma once

#include <domain/IProgress.hpp>
#include <relinker/domain/RelinkResult.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <relinker/patching/GamePatch.hpp>
#include <optional>

namespace Relinker {

enum class TargetSystem { Linux, Windows, MacOs };

struct ConversionOptions {
    TargetSystem Target = TargetSystem::Linux;
    std::uint32_t UnusedFilterLevel = 0;
    bool ToIntel = false;
    bool ToRosetta = false;
    bool LowerBitPermutation = false;
    bool FrameRedZone = false;
    bool SkipSyscallCheck = false;
    bool TrapSdkSyscallStubs = false;
    bool LazyBinding = false;
    bool WindowsGui = false;
    bool TraceImports = false;
    std::optional<std::uint64_t> WindowsFixedBase;
    bool WindowsDiagnostics = false;
    std::optional<std::uint64_t> LinuxFixedBase;
    std::optional<std::uint64_t> MacosFixedBase;
    std::string RunPath = "$ORIGIN/libs";
    std::set<std::string> ExcludedModules;
    std::vector<GamePatch::PatchEntry> Patches;
    std::string TargetName;
};

struct ConversionInput {
    std::vector<std::uint8_t> Executable;
    std::filesystem::path InputPath;
    std::filesystem::path OutputPath;
    std::filesystem::path ModuleRoot;
    std::vector<GuestModuleInput> Modules;
    std::vector<std::filesystem::path> RuntimeDirectories;
    std::optional<std::vector<std::uint8_t>> Icon;
};

struct ConversionOutput {
    std::vector<std::uint8_t> Executable;
    std::vector<GuestArtifact> Modules;
    RelinkResult Analysis;
    std::string WindowsDiagnostics;
    std::vector<GamePatch::AppliedPatch> AppliedPatches;
    std::optional<std::uint64_t> LinuxBase;
};

class Conversion {
public:
    explicit Conversion(Domain::IProgress& progress);
    ConversionOutput Convert(ConversionInput input, const ConversionOptions& options) const;

private:
    Domain::IProgress& progress;
};

}
