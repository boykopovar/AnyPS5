#include <domain/Types.hpp>
#include <io/NativePath.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/windows/WindowsElfPatcher.hpp>
#include <elfpatcher/macos/MacOsMachOPatcher.hpp>
#include <io/ByteWriter.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CallSiteResolver.hpp>
#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <relinker/guest/GuestImage.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <map>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <relinker/pipeline/Conversion.hpp>
#include <sstream>
#include <bit>
#include <domain/InputProgress.hpp>
#include <relinker/patching/GamePatch.hpp>

namespace Relinker {

Conversion::Conversion(Domain::IProgress& progress) : progress(progress) {}

ConversionOutput Conversion::Convert(ConversionInput input, const ConversionOptions& options) const try {
    Domain::InputProgress inputProgress(progress, Io::Utf8Path(input.InputPath));
    static_assert(std::endian::native == std::endian::little);
    if (options.Target != TargetSystem::Linux && options.Target != TargetSystem::Windows && options.Target != TargetSystem::MacOs) throw Domain::RelinkerException("Unknown target system");
    if (options.ToRosetta && (!options.ToIntel || options.Target == TargetSystem::Windows)) throw Domain::RelinkerException("Rosetta conversion requires Intel lowering and a non-Windows target");
    if (options.UnusedFilterLevel > 2) throw Domain::RelinkerException("Unused NID filter level must be 0, 1 or 2");
    if (input.InputPath.empty() || input.OutputPath.empty()) throw Domain::RelinkerException("Input and output names are required");
    if (input.InputPath.lexically_normal() == input.OutputPath.lexically_normal()) throw Domain::RelinkerException("Executable output would overwrite the input executable");
    if (!input.Modules.empty() && input.ModuleRoot.empty()) throw Domain::RelinkerException("Module root is required");
    if (options.WindowsDiagnostics && options.Target != TargetSystem::Windows) throw Domain::RelinkerException("Windows diagnostics require the Windows target");
    if (options.LinuxFixedBase && options.Target != TargetSystem::Linux) throw Domain::RelinkerException("A fixed Linux base requires the Linux target");
    if (options.MacosFixedBase && options.Target != TargetSystem::MacOs) throw Domain::RelinkerException("A fixed macOS base requires the macOS target");
    const bool windows = options.Target == TargetSystem::Windows;
    const bool macos = options.Target == TargetSystem::MacOs;
    inputProgress.Report({Domain::ProgressStage::Executable, Io::Utf8Path(input.InputPath), 0, 1});
    auto sourceBytes = std::move(input.Executable);
    std::vector<GamePatch::AppliedPatch> appliedPatches;
    std::vector<GamePatch::PatchedRange> patchedRanges;
    if (!options.Patches.empty()) {
        auto patched = GamePatch::PatchTarget(sourceBytes, options.TargetName, options.Patches);
        patchedRanges = std::move(patched.Ranges);
        for (auto& patch : patched.Applied) {
            inputProgress.Message(GamePatch::FormatApplied(patch) + "\n");
            appliedPatches.push_back(std::move(patch));
        }
    }
    std::vector<Codegen::TrampolineSite> trampolines;
    if (options.ToIntel) {
        const auto segments = Relinker::ElfReader(sourceBytes).ReadProgramHeaders();
        auto converted = Codegen::MakeAmd64OnlyConverter(options.ToRosetta ? Codegen::Amd64OnlyTarget::Rosetta : Codegen::Amd64OnlyTarget::Intel, [&](auto completed, auto total) { inputProgress.Report({Domain::ProgressStage::Lowering, {}, completed, total}); })->Convert(std::move(sourceBytes), segments);
        sourceBytes = std::move(converted.Bytes);
        trampolines = std::move(converted.Trampolines);
        GamePatch::CheckOverlaps(options.TargetName, patchedRanges, {.Reports = converted.Reports, .Trampolines = trampolines});
        std::map<std::string, std::size_t> stubsByName;
        const auto counted = [](const std::string& name) { return name == "VRSQRTPS" || name == "VRCPPS" || name == "256-bit store" || name == "scalar compare"; };
        for (const auto& report : converted.Reports) {
            if (report.Lowering == Codegen::Amd64OnlyLowering::Kept)
                { std::ostringstream message; message << "Intel substitution: " << report.InstructionName << " at 0x" << std::hex << report.Offset << std::dec << " (" << report.OriginalLength << " bytes) kept: no room for a jump\n"; inputProgress.Message(message.str()); }
            else if (counted(report.InstructionName))
                ++stubsByName[report.InstructionName];
            else
                { std::ostringstream message; message << "Intel substitution: " << report.InstructionName << " at 0x" << std::hex << report.Offset << std::dec << " (" << report.OriginalLength << " bytes) -> " << (report.Lowering == Codegen::Amd64OnlyLowering::InPlace ? "in place " : "stub ") << report.ReplacementLength << " bytes\n"; inputProgress.Message(message.str()); }
        }
        for (const auto& [name, count] : stubsByName)
            { std::ostringstream message; message << "Intel substitution: " << name << " -> stub at " << count << " sites\n"; inputProgress.Message(message.str()); }
        { std::ostringstream message; message << "Intel conversion: " << converted.ReplacedCount << " in place, " << trampolines.size() << " stubs, " << converted.KeptCount << " kept\n"; inputProgress.Message(message.str()); }
    }

    auto elfReader = std::make_shared<Relinker::ElfReader>(std::move(sourceBytes));
    const std::shared_ptr<Relinker::ISyscallScanner> syscallScanner = options.SkipSyscallCheck ? Relinker::MakeNullSyscallScanner() : Relinker::MakeSyscallScanner(inputProgress, options.TrapSdkSyscallStubs);

    const auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
        elfReader,
        syscallScanner,
        Relinker::MakeCallSiteResolver(),
        std::make_shared<Relinker::ValidationPolicy>(),
        std::make_shared<Relinker::SysVDynamicSectionBuilder>(),
        options.UnusedFilterLevel == 2 ? Relinker::MakeStrictUnusedNidFilter(inputProgress) : Relinker::MakeUnusedNidFilter(),
        options.UnusedFilterLevel, inputProgress
    );

    { std::ostringstream message; message << "System: " << (windows ? "Windows" : macos ? "macOS" : "Linux") << "; unused-filter=" << options.UnusedFilterLevel << "\n"; inputProgress.Message(message.str()); }
    for (const auto& name : options.ExcludedModules) { std::ostringstream message; message << "Guest module excluded: " << name << '\n'; inputProgress.Message(message.str()); }
    auto result = pipeline->Relink();
    sourceBytes = elfReader->TakeBytes();
    for (const auto& patch : result.Patches) {
        if (patch.Offset > sourceBytes.size() || patch.Bytes.size() > sourceBytes.size() - patch.Offset)
            throw Domain::RelinkerException("Relinker patch exceeds source image", patch.Offset);
        for (std::size_t index = 0; index < patch.Bytes.size(); ++index) sourceBytes[patch.Offset + index] = patch.Bytes[index];
    }
    GamePatch::CheckOverlaps(options.TargetName, patchedRanges, {.RelinkPatches = result.Patches, .RelaData = result.DynamicSection.RelaData, .RelaPltData = result.DynamicSection.RelaPltData});

    if (!options.Patches.empty()) {
        for (auto& module : input.Modules) {
            auto patched = GamePatch::PatchTarget(module.Bytes, GamePatch::ModuleTargetName(module.Path, input.ModuleRoot), options.Patches);
            module.PatchedRanges = std::move(patched.Ranges);
            for (auto& patch : patched.Applied) {
                inputProgress.Message(GamePatch::FormatApplied(patch) + "\n");
                appliedPatches.push_back(std::move(patch));
            }
        }
    }

    std::vector<Relinker::GuestArtifact> guestArtifacts;
    if (!input.Modules.empty()) {
        guestArtifacts = Relinker::GuestModuleBuilder().Build(input.InputPath, input.OutputPath, result.DynamicSection, windows, macos, options.ToIntel, options.ToRosetta, *syscallScanner, options.LazyBinding, options.RunPath, options.ExcludedModules, input.ModuleRoot, input.RuntimeDirectories, std::move(input.Modules), inputProgress);
    }

    auto byteWriter = std::make_shared<Io::ByteWriter>();

    std::optional<std::uint64_t> linuxBase;
    if (!windows && !macos)
        linuxBase = options.LinuxFixedBase || options.LazyBinding ? options.LinuxFixedBase : std::optional<std::uint64_t>(Elfpatcher::Linux::DefaultFixedBase);
    if (!windows && !macos && options.LazyBinding)
        inputProgress.Message("Warning: with --lazy-binding the Linux executable stays position independent and is not placed at 0x400000; a title whose own sceLibcMspaceCreate checks the 40-bit address window stops at start-up\n");
    std::shared_ptr<Elfpatcher::IElfPatcher> patcher;
    if (macos) {
        patcher = std::make_shared<Elfpatcher::MacOs::MacOsMachOPatcher>(options.MacosFixedBase, [&](std::string_view text) { inputProgress.Message(text); });
    } else if (windows) {
        patcher = std::make_shared<Elfpatcher::Windows::WindowsPePatcher>(options.WindowsGui, std::move(input.Icon), input.InputPath.parent_path() / "sce_sys" / "icon0.png", [&](auto completed, auto total) { inputProgress.Report({Domain::ProgressStage::Image, {}, completed, total}); }, options.WindowsFixedBase, options.TraceImports, [&](std::string_view text) { inputProgress.Message(text); });
    } else {
        patcher = std::make_shared<Elfpatcher::Linux::LinuxElfPatcher>(
            std::make_shared<Elfpatcher::EntryStubBuilder>(),
            std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(
                std::make_shared<Elfpatcher::SegmentFilter>(options.LazyBinding),
                byteWriter
            ),
            std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
            byteWriter,
            linuxBase,
            options.TraceImports
        );
    }

    if (!windows) inputProgress.Report({Domain::ProgressStage::Image, Io::Utf8Path(input.InputPath), 0, 1});
    std::vector<std::uint8_t> executableBytes;
    try {
        executableBytes = patcher->Patch(std::move(sourceBytes), result.OriginalHeaders, result.DynamicSection, result.OriginalPltGotVaddr, options.RunPath, options.LazyBinding, options.WindowsDiagnostics, trampolines);
    } catch (Domain::RelinkerException& error) {
        if (error.InputPath.empty()) error.InputPath = Io::Utf8Path(input.InputPath);
        throw;
    }

    if (!windows) inputProgress.Report({Domain::ProgressStage::Image, Io::Utf8Path(input.InputPath), 1, 1});
    std::string diagnostics;
    if (windows) diagnostics = std::static_pointer_cast<Elfpatcher::Windows::WindowsPePatcher>(patcher)->GetDiagnostics();
    inputProgress.Report({Domain::ProgressStage::Executable, Io::Utf8Path(input.InputPath), 1, 1});
    return {std::move(executableBytes), std::move(guestArtifacts), std::move(result), std::move(diagnostics), std::move(appliedPatches), linuxBase};
} catch (Domain::RelinkerException& error) {
    if (error.InputPath.empty()) error.InputPath = Io::Utf8Path(input.InputPath);
    throw;
}

}
