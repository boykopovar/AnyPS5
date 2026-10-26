#include <ConsoleProgress.hpp>
#include <RuntimeLayout.hpp>
#include <NativeModules.hpp>
#include <relinker/pipeline/Conversion.hpp>
#include <Cli.hpp>
#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/NativePath.hpp>
#include <io/FileIdentity.hpp>
#include <elfpatcher/linux/LinuxDesktopEntryWriter.hpp>
#include <io/FileWriter.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <codegen/CodegenException.hpp>
#include <relinker/patching/GamePatch.hpp>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <cwchar>
#endif

namespace {

struct ConvertedExecutable {
    std::string InputPath;
    std::string OutputPath;
    Relinker::RelinkResult Result;
    std::vector<Relinker::GuestArtifact> GuestArtifacts;
    std::vector<std::uint8_t> Bytes;
    std::string Diagnostics;
    std::vector<Relinker::GamePatch::AppliedPatch> AppliedPatches;
    std::optional<std::uint64_t> LinuxBase;
};

ConvertedExecutable Convert(const Cli::Args& args, const std::string& inputPath, const std::string& absPath, const bool mainExecutable, const std::string& targetName, const std::vector<Relinker::GamePatch::PatchEntry>& patches, Cli::ConsoleProgress& progress) {
    Io::FileReader fileReader;
    Relinker::ConversionInput input;
    Relinker::ElfReader(fileReader.ReadPrefix(inputPath, 0x40)).ValidateMagic();
    input.Executable = fileReader.Read(inputPath);
    input.InputPath = std::filesystem::absolute(Io::NativePath(inputPath)).lexically_normal();
    input.OutputPath = Io::NativePath(absPath);
    Cli::NativeModules modules;
    if (!args.skipSceModule) {
        modules = Cli::ReadModules(std::filesystem::absolute(Io::NativePath(args.inputPath)).lexically_normal(), Io::NativePath(args.sceModulePath), args.excludedSceModules, args.moduleDirectories);
        input.ModuleRoot = modules.Root;
        input.Modules = std::move(modules.Modules);
        input.RuntimeDirectories = modules.RuntimeDirectories;
    }
    const auto iconPath = Io::NativePath(args.inputPath).parent_path() / "sce_sys" / "icon0.png";
    if (args.toWindows && std::filesystem::exists(iconPath)) input.Icon = fileReader.Read(Io::Utf8Path(iconPath));
    Relinker::ConversionOptions options;
    options.Target = args.toWindows ? Relinker::TargetSystem::Windows : args.toMacos ? Relinker::TargetSystem::MacOs : Relinker::TargetSystem::Linux;
    options.ToIntel = args.toIntel;
    options.ToRosetta = args.toRosetta;
    options.FrameRedZone = args.frameRedZone;
    options.SkipSyscallCheck = args.skipSyscallCheck;
    options.TrapSdkSyscallStubs = args.trapSdkSyscallStubs;
    options.UnusedFilterLevel = args.unusedFilterLevel;
    options.LazyBinding = args.lazyBinding;
    options.RunPath = args.runPath;
    options.WindowsGui = args.windowsGui;
    options.TraceImports = args.traceImports;
    options.WindowsFixedBase = mainExecutable ? args.windowsFixedBase : std::nullopt;
    options.WindowsDiagnostics = args.windowsDiagnostics && mainExecutable;
    options.LinuxFixedBase = args.linuxFixedBase;
    options.MacosFixedBase = args.macosFixedBase;
    options.ExcludedModules = args.excludedSceModules;
    options.Patches = patches;
    options.TargetName = targetName;
    progress.IncludeFile(Io::Utf8Path(input.InputPath));
    progress.IncludeFile(Io::Utf8Path(input.OutputPath));
    for (const auto& module : input.Modules) progress.IncludeFile(Io::Utf8Path(module.Path));
    auto converted = Relinker::Conversion(progress).Convert(std::move(input), options);
    if (!args.skipSceModule) Cli::ReportUnconvertedNeeded(modules, converted.Analysis.DynamicSection.NeededLibraries, args.toWindows, std::cout);
    return {inputPath, absPath, std::move(converted.Analysis), std::move(converted.Modules), std::move(converted.Executable), std::move(converted.WindowsDiagnostics), std::move(converted.AppliedPatches), converted.LinuxBase};
}

class OutputIdentityDirectory {
public:
    OutputIdentityDirectory(const OutputIdentityDirectory&) = delete;
    OutputIdentityDirectory& operator=(const OutputIdentityDirectory&) = delete;

    explicit OutputIdentityDirectory(const std::filesystem::path& parent) {
        std::random_device random;
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            const auto candidate = parent / (".anyps5-output-identity-" + std::to_string(random()) + "-" + std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                directory = candidate;
                return;
            }
            if (error && error != std::errc::file_exists)
                throw std::filesystem::filesystem_error("Cannot create output identity directory", candidate, error);
        }
        throw Domain::RelinkerException("Cannot reserve an output identity directory in " + Io::Utf8Path(parent));
    }

    ~OutputIdentityDirectory() {
        if (directory.empty()) return;
        try {
            Remove();
        } catch (const std::exception& error) {
            std::cerr << "FAIL: Output identity cleanup: " << error.what() << '\n';
        }
    }

    const std::filesystem::path& GetDirectory() const { return directory; }

    void Remove() {
        std::filesystem::remove(directory);
        directory.clear();
    }

private:
    std::filesystem::path directory;
};

bool SameOutputFile(const std::filesystem::path& first, const std::filesystem::path& second) {
    return Io::SameFile(first, second);
}

bool SameOutputDestination(const std::filesystem::path& first, const std::filesystem::path& second,
                           std::filesystem::path& firstPath, std::filesystem::path& secondPath) {
    if (firstPath.empty()) firstPath = std::filesystem::weakly_canonical(first);
    if (secondPath.empty()) secondPath = std::filesystem::weakly_canonical(second);
    if (firstPath == secondPath) return true;
    if (std::filesystem::exists(first) && std::filesystem::exists(second)) return SameOutputFile(first, second);
#ifdef _WIN32
    const auto firstName = firstPath.filename().native();
    const auto secondName = secondPath.filename().native();
    const auto comparison = CompareStringOrdinal(firstName.c_str(), -1, secondName.c_str(), -1, TRUE);
    if (comparison == 0) throw Domain::RelinkerException("Cannot compare output filenames");
    if (comparison == CSTR_EQUAL && std::filesystem::exists(firstPath.parent_path()) &&
        std::filesystem::exists(secondPath.parent_path()) && std::filesystem::equivalent(firstPath.parent_path(), secondPath.parent_path())) {
        OutputIdentityDirectory probe(firstPath.parent_path());
        auto alternateName = Io::Utf8Path(probe.GetDirectory().filename());
        for (auto& character : alternateName)
            if (character >= 'a' && character <= 'z') character = static_cast<char>(character - ('a' - 'A'));
        const auto alternate = firstPath.parent_path() / Io::NativePath(alternateName);
        const bool same = std::filesystem::exists(alternate) && std::filesystem::equivalent(probe.GetDirectory(), alternate);
        probe.Remove();
        return same;
    }
#else
    if (std::filesystem::exists(firstPath.parent_path()) && std::filesystem::exists(secondPath.parent_path()) &&
        std::filesystem::equivalent(firstPath.parent_path(), secondPath.parent_path())) {
        OutputIdentityDirectory probe(firstPath.parent_path());
        const auto name = probe.GetDirectory() / firstPath.filename();
        const auto alternate = probe.GetDirectory() / secondPath.filename();
        std::filesystem::create_directory(name);
        const bool same = std::filesystem::exists(alternate) && std::filesystem::equivalent(name, alternate);
        std::filesystem::remove(name);
        probe.Remove();
        return same;
    }
#endif
    return false;
}

std::string ReadText(const std::string& path) {
    const auto bytes = Io::FileReader().Read(path);
    return std::string(bytes.begin(), bytes.end());
}

std::filesystem::path ExecutableRelative(const std::string& executable) {
    const auto relative = Io::NativePath(executable).lexically_normal();
    if (relative.empty() || relative.has_root_path() || *relative.begin() == ".." || !relative.has_filename())
        throw Domain::RelinkerException("--executable must be a file inside the guest module parent directory: " + executable);
    return relative;
}

std::vector<std::filesystem::path> ModuleDirectories(const Cli::Args& args, const std::filesystem::path& moduleRoot) {
    std::vector<std::filesystem::path> moduleDirectories;
    for (const auto* name : {"sce_module", "sce_modules", "prx"}) moduleDirectories.push_back(moduleRoot / name);
    for (const auto& name : args.moduleDirectories) moduleDirectories.push_back((moduleRoot / Io::NativePath(name)).lexically_normal());
    for (auto& directory : moduleDirectories) if (!directory.has_filename()) directory = directory.parent_path();
    return moduleDirectories;
}

std::filesystem::path ResolveExtraExecutable(const std::filesystem::path& moduleRoot, const std::vector<std::filesystem::path>& moduleDirectories, const std::filesystem::path& relative, const std::string& executable) {
    const auto input = moduleRoot / relative;
    if (!std::filesystem::is_regular_file(input)) throw Domain::RelinkerException("--executable is not a file: " + Io::Utf8Path(input));
    if (std::any_of(moduleDirectories.begin(), moduleDirectories.end(), [&](const auto& directory) { return Cli::SameDirectory(directory, input.parent_path()); }))
        throw Domain::RelinkerException("--executable is in a guest module directory: " + executable);
    return input;
}

std::vector<Relinker::GamePatch::PatchEntry> LoadPatches(const Cli::Args& args) {
    if (args.noPatches) return {};
    std::filesystem::path path;
    if (!args.patchesPath.empty()) {
        path = Io::NativePath(args.patchesPath);
        if (!std::filesystem::is_regular_file(path)) throw Domain::RelinkerException("Patch file not found: " + args.patchesPath);
    } else {
        path = Io::NativePath(args.inputPath).parent_path() / Relinker::GamePatch::PatchFileName;
        if (!std::filesystem::is_regular_file(path)) return {};
    }
    std::cout << "Patches: " << Io::Utf8Path(path) << '\n';
    return Relinker::GamePatch::ParsePatchFile(ReadText(Io::Utf8Path(path))).Entries;
}

int RunVerifyPatches(const Cli::Args& args) {
    using namespace Relinker::GamePatch;
    const auto file = ParsePatchFile(ReadText(args.verifyPatchesPath));
    Io::FileReader reader;
    std::vector<VerifyTarget> executables;
    std::vector<VerifyTarget> modules;
    const auto inputPath = Io::NativePath(args.inputPath);
    if (!std::filesystem::is_regular_file(inputPath))
        throw Domain::RelinkerException((args.verifyOutput ? "output not found: " : "input not found: ") + args.inputPath);
    if (args.verifyOutput) {
        executables.push_back({file.MainTarget.value_or(std::string()), reader.Read(args.inputPath)});
        for (const auto& executable : args.executables) {
            const auto relative = ExecutableRelative(executable);
            const auto output = OutputExecutablePath(inputPath, executable);
            if (!std::filesystem::is_regular_file(output)) throw Domain::RelinkerException("output not found: " + Io::Utf8Path(output));
            executables.push_back({relative.generic_string(), reader.Read(Io::Utf8Path(output))});
        }
    } else {
        executables.push_back({Io::Utf8Path(inputPath.filename()), reader.Read(args.inputPath)});
        std::filesystem::path moduleRoot = args.executables.empty() ? std::filesystem::path() : std::filesystem::absolute(Io::NativePath(args.sceModulePath)).lexically_normal();
        if (!args.skipSceModule) {
            auto native = Cli::ReadModules(std::filesystem::absolute(inputPath).lexically_normal(), Io::NativePath(args.sceModulePath), args.excludedSceModules, args.moduleDirectories);
            for (auto& module : native.Modules) modules.push_back({ModuleTargetName(module.Path, native.Root), std::move(module.Bytes)});
        }
        const auto moduleDirectories = ModuleDirectories(args, moduleRoot);
        for (const auto& executable : args.executables) {
            const auto relative = ExecutableRelative(executable);
            const auto input = ResolveExtraExecutable(moduleRoot, moduleDirectories, relative, executable);
            executables.push_back({relative.generic_string(), reader.Read(Io::Utf8Path(input))});
        }
    }
    const auto results = VerifyPatches(file, executables, modules, args.verifyOutput);
    std::size_t mismatches = 0;
    std::size_t notChecked = 0;
    for (const auto& result : results) {
        std::cout << StatusName(result.Status) << ": " << (result.Name.empty() ? std::string() : result.Name + " ") << result.Target << " va=0x" << std::hex << result.VirtualAddress;
        if (result.Status != VerifyStatus::NotChecked) std::cout << " off=0x" << result.Offset;
        std::cout << std::dec << ' ' << (result.Status == VerifyStatus::NotChecked ? "(" + result.Detail + ")" : result.Detail) << '\n';
        if (result.Status == VerifyStatus::Mismatch) ++mismatches;
        if (result.Status == VerifyStatus::NotChecked) ++notChecked;
    }
    std::cout << "Verified " << results.size() << " patches: " << results.size() - mismatches - notChecked << " ok, " << notChecked << " not-checked, " << mismatches << " mismatch\n";
    return mismatches == 0 ? 0 : 1;
}

int Run(const int argc, char* argv[]) {
    Cli::Args args;
    try {
        args = Cli::ParseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }

    if (args.showHelp) {
        std::cout << Cli::Usage() << '\n';
        return 0;
    }

    Cli::ConsoleProgress progress(std::cout);
    try {
        if (!args.verifyPatchesPath.empty()) return RunVerifyPatches(args);
        auto extension = Io::Utf8Path(Io::NativePath(args.outputPath).extension());
        for (auto& character : extension) if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
        if (!args.toWindows && !args.toMacos && extension == ".exe") std::cerr << "WARNING: Output filename ends with .exe, but --windows was not specified. The output will be a Linux ELF executable.\n";
        Io::FileWriter fileWriter;

        if (std::filesystem::exists(Io::NativePath(args.inputPath)) && std::filesystem::exists(Io::NativePath(args.outputPath)) && SameOutputFile(Io::NativePath(args.inputPath), Io::NativePath(args.outputPath)))
            throw Domain::RelinkerException("Executable output would overwrite the input executable");
        if (Io::NativePath(args.outputPath).filename().empty() || std::filesystem::is_directory(Io::NativePath(args.outputPath)))
            throw Domain::RelinkerException("Executable output is a directory: " + args.outputPath);
        const std::string absPath = Io::Utf8Path(std::filesystem::absolute(Io::NativePath(args.outputPath)));

        struct ExtraExecutable {
            std::string Argument;
            std::filesystem::path Relative;
            std::filesystem::path Input;
            std::filesystem::path Output;
        };
        std::vector<ExtraExecutable> extraExecutables;
        const auto outputDirectory = Io::NativePath(absPath).parent_path();
        const auto moduleRoot = args.executables.empty() ? std::filesystem::path() : std::filesystem::absolute(Io::NativePath(args.sceModulePath)).lexically_normal();
        const auto moduleDirectories = ModuleDirectories(args, moduleRoot);
        for (const auto& executable : args.executables) {
            const auto relative = ExecutableRelative(executable);
            const auto input = ResolveExtraExecutable(moduleRoot, moduleDirectories, relative, executable);
            const auto output = Relinker::GamePatch::OutputExecutablePath(Io::NativePath(absPath), executable);
            if (Io::SameFile(Io::NativePath(args.inputPath), input)) throw Domain::RelinkerException("Executable is converted twice: " + executable);
            if (output.lexically_normal() == Io::NativePath(absPath).lexically_normal()) throw Domain::RelinkerException("Executable outputs collide: " + Io::Utf8Path(output));
            for (const auto& extra : extraExecutables) {
                if (Io::SameFile(extra.Input, input)) throw Domain::RelinkerException("Executable is converted twice: " + executable);
                if (extra.Output.lexically_normal() == output.lexically_normal()) throw Domain::RelinkerException("Executable outputs collide: " + Io::Utf8Path(output));
            }
            extraExecutables.push_back({executable, relative, input, output});
        }

        const auto patches = LoadPatches(args);
        const std::string mainTarget = Io::Utf8Path(Io::NativePath(args.inputPath).filename());
        progress.IncludeFile(args.inputPath);
        progress.IncludeFile(absPath);
        for (const auto& extra : extraExecutables) {
            progress.IncludeFile(Io::Utf8Path(extra.Input));
            progress.IncludeFile(Io::Utf8Path(extra.Output));
        }
        std::vector<ConvertedExecutable> executables;
        executables.push_back(Convert(args, args.inputPath, absPath, true, mainTarget, patches, progress));
        for (const auto& [executable, relative, input, output] : extraExecutables) {
            std::cout << "Executable: " << relative.generic_string() << '\n';
            auto converted = Convert(args, Io::Utf8Path(input), Io::Utf8Path(output), false, relative.generic_string(), patches, progress);
            const auto& mainArtifacts = executables.front().GuestArtifacts;
            const bool shared = converted.GuestArtifacts.size() == mainArtifacts.size() && std::equal(mainArtifacts.begin(), mainArtifacts.end(), converted.GuestArtifacts.begin(), [](const auto& left, const auto& right) {
                return left.Path == right.Path && left.Bytes == right.Bytes;
            });
            if (!shared) throw Domain::RelinkerException("Guest modules convert differently for " + executable + ", so it cannot share them with the input executable");
            executables.push_back(std::move(converted));
        }
        const auto& guestArtifacts = executables.front().GuestArtifacts;
        if (!patches.empty()) {
            std::vector<std::string> targetNames{mainTarget};
            for (const auto& extra : extraExecutables) targetNames.push_back(extra.Relative.generic_string());
            if (!guestArtifacts.empty()) {
                const auto patchModuleRoot = std::filesystem::absolute(Io::NativePath(args.sceModulePath)).lexically_normal();
                for (const auto& artifact : guestArtifacts) targetNames.push_back(Relinker::GamePatch::ModuleTargetName(artifact.SourcePath, patchModuleRoot));
            }
            Relinker::GamePatch::MatchTargets(patches, targetNames);
        }
        std::vector<std::vector<Relinker::GamePatch::AppliedPatch>> appliedRecords;
        for (const auto& executable : executables) appliedRecords.push_back(executable.AppliedPatches);
        const auto appliedPatches = Relinker::GamePatch::AggregateManifest(appliedRecords);
        const auto patchFile = args.patchesPath.empty() ? Io::NativePath(args.inputPath).parent_path() / Relinker::GamePatch::PatchFileName : Io::NativePath(args.patchesPath);
        std::filesystem::path manifestPath;
        std::string manifestText;
        if (!appliedPatches.empty()) manifestPath = Io::NativePath(absPath).parent_path() / Io::NativePath(Io::Utf8Path(Io::NativePath(absPath).stem()) + Relinker::GamePatch::ManifestSuffix);
        if (!manifestPath.empty()) manifestText = Relinker::GamePatch::WriteManifest(appliedPatches, mainTarget, Io::Utf8Path(patchFile), executables.front().LinuxBase);

        const Elfpatcher::Linux::LinuxDesktopEntryWriter desktopEntryWriter;
        const auto metadataRoot = Io::NativePath(args.inputPath).parent_path() / "sce_sys";
        const auto desktopEntry = args.toWindows || args.toMacos ? std::nullopt : desktopEntryWriter.Prepare(metadataRoot, Io::NativePath(absPath));
        std::vector<std::filesystem::path> metadataInputs;
        if (args.toWindows || desktopEntry) metadataInputs.push_back(metadataRoot / "icon0.png");
        if (desktopEntry) metadataInputs.push_back(metadataRoot / "param.json");

        const auto checkOutputInputs = [&](const std::filesystem::path& path, const char* kind) {
            if (!std::filesystem::exists(path)) return;
            for (const auto& input : metadataInputs)
                if (std::filesystem::exists(input) && SameOutputFile(input, path))
                    throw Domain::RelinkerException(std::string(kind) + " output would overwrite input metadata: " + Io::Utf8Path(path));
            for (const auto& executable : executables)
                if (SameOutputFile(Io::NativePath(executable.InputPath), path))
                    throw Domain::RelinkerException(std::string(kind) + " output would overwrite the input executable: " + Io::Utf8Path(path));
            for (const auto& artifact : guestArtifacts)
                if (SameOutputFile(artifact.SourcePath, path))
                    throw Domain::RelinkerException(std::string(kind) + " output would overwrite an input module: " + Io::Utf8Path(path));
        };
        for (std::size_t index = 1; index < executables.size(); ++index) checkOutputInputs(Io::NativePath(executables[index].OutputPath), "Executable");
        if (desktopEntry) {
            checkOutputInputs(desktopEntry->IconPath, "Icon");
            checkOutputInputs(desktopEntry->EntryPath, "Desktop entry");
        }
        if (args.windowsDiagnostics)
            checkOutputInputs(std::filesystem::absolute("windows-diagnostics-imports.txt"), "Windows import diagnostics");

        std::vector<std::filesystem::path> registryPaths;
        std::vector<std::filesystem::path> neededPaths;
        if (args.writeRegistry) {
            for (const auto& executable : executables) {
                const std::filesystem::path outFsPath = Io::NativePath(executable.OutputPath);
                registryPaths.push_back(outFsPath.parent_path() / Io::NativePath(Io::Utf8Path(outFsPath.stem()) + ".registry.json"));
            }
            const std::filesystem::path outFsPath = Io::NativePath(absPath);
            for (const auto& artifact : guestArtifacts)
                registryPaths.push_back(outFsPath.parent_path() / Io::NativePath(Io::Utf8Path(outFsPath.stem()) + "." + Io::Utf8Path(artifact.Path.filename()) + ".registry.json"));
            for (const auto& path : registryPaths) checkOutputInputs(path, "Registry");
            for (const auto& path : registryPaths) {
                auto name = Io::Utf8Path(path.filename());
                name.resize(name.size() - std::string(".registry.json").size());
                neededPaths.push_back(path.parent_path() / Io::NativePath(name + ".needed.json"));
            }
            for (const auto& path : neededPaths) checkOutputInputs(path, "Needed libraries");
        }

        checkOutputInputs(Io::NativePath(absPath), "Executable");
        if (!manifestPath.empty()) {
            checkOutputInputs(manifestPath, "Patch manifest");
            if (std::filesystem::exists(manifestPath) && std::filesystem::exists(patchFile) && std::filesystem::equivalent(manifestPath, patchFile))
                throw Domain::RelinkerException("Patch manifest output would overwrite the patch file: " + Io::Utf8Path(manifestPath));
        }
        for (const auto& artifact : guestArtifacts) checkOutputInputs(artifact.Path, "Guest module");

        std::vector<std::filesystem::path> outputPaths;
        for (const auto& executable : executables) outputPaths.push_back(Io::NativePath(executable.OutputPath));
        for (const auto& artifact : guestArtifacts) outputPaths.push_back(artifact.Path);
        outputPaths.insert(outputPaths.end(), registryPaths.begin(), registryPaths.end());
        outputPaths.insert(outputPaths.end(), neededPaths.begin(), neededPaths.end());
        if (!manifestPath.empty()) outputPaths.push_back(manifestPath);
        if (desktopEntry) {
            outputPaths.push_back(desktopEntry->IconPath);
            outputPaths.push_back(desktopEntry->EntryPath);
        }
        if (args.windowsDiagnostics) outputPaths.push_back(std::filesystem::absolute("windows-diagnostics-imports.txt"));
        std::vector<std::filesystem::path> canonicalOutputPaths(outputPaths.size());
        for (std::size_t index = 0; index < outputPaths.size(); ++index) {
            for (std::size_t previous = 0; previous < index; ++previous) {
                const auto& first = outputPaths[index];
                const auto& second = outputPaths[previous];
                if (SameOutputDestination(first, second, canonicalOutputPaths[index], canonicalOutputPaths[previous]))
                    throw Domain::RelinkerException("Output paths collide: " + Io::Utf8Path(first) + " and " + Io::Utf8Path(second));
            }
        }

        if (args.windowsDiagnostics) fileWriter.Write("windows-diagnostics-imports.txt", executables.front().Diagnostics);
        const auto outputCount = guestArtifacts.size() + executables.size();
        progress.Report({Domain::ProgressStage::Write, absPath, 0, outputCount});
        std::size_t written = 0;
        for (const auto& artifact : guestArtifacts) {
            std::filesystem::create_directories(artifact.Path.parent_path());
            fileWriter.Write(Io::Utf8Path(artifact.Path), artifact.Bytes);
            progress.Report({Domain::ProgressStage::Write, absPath, ++written, outputCount});
        }
        for (const auto& executable : executables) {
            fileWriter.Write(executable.OutputPath, executable.Bytes);
            if (!args.toWindows) {
                std::error_code error;
                std::filesystem::permissions(Io::NativePath(executable.OutputPath), std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec, std::filesystem::perm_options::add, error);
                if (error) throw Domain::RelinkerException("Cannot make the output executable: " + executable.OutputPath + " (" + error.message() + ")");
            }
            progress.Report({Domain::ProgressStage::Write, absPath, ++written, outputCount});
        }
        if (args.writeRegistry) {
            const auto registryWriter = std::make_shared<Relinker::CallRegistryWriter>();
            for (std::size_t index = 0; index < executables.size(); ++index) {
                fileWriter.Write(Io::Utf8Path(registryPaths[index]), registryWriter->WriteCallRegistry(executables[index].Result.RegistryEntries));
                fileWriter.Write(Io::Utf8Path(neededPaths[index]), registryWriter->WriteNeededLibraries(executables[index].Result.DynamicSection.NeededLibraries));
            }
            for (std::size_t index = 0; index < guestArtifacts.size(); ++index) {
                fileWriter.Write(Io::Utf8Path(registryPaths[executables.size() + index]), registryWriter->WriteModuleImports(guestArtifacts[index].Imports));
                fileWriter.Write(Io::Utf8Path(neededPaths[executables.size() + index]), registryWriter->WriteNeededLibraries(guestArtifacts[index].NeededLibraries));
            }
        }
        if (!manifestPath.empty()) fileWriter.Write(Io::Utf8Path(manifestPath), manifestText);
        progress.Finish();
        if (!manifestPath.empty()) std::cout << "Patch manifest: " << Io::Utf8Path(manifestPath) << '\n';
        if (desktopEntry) std::cout << "Desktop entry: " << Io::Utf8Path(desktopEntryWriter.Write(*desktopEntry)) << '\n';
        std::cout << "\nConversion complete\n"
                  << "Output file: " << absPath << '\n'
                  << "External prx references: " << executables.front().Result.RegistryEntries.size() << '\n'
                  << "Converted modules: " << guestArtifacts.size() << "\n\n";
        std::vector<std::string> extraPaths;
        for (std::size_t index = 1; index < executables.size(); ++index) {
            std::cout << "Executable file: " << executables[index].OutputPath << '\n';
            extraPaths.push_back(Io::Utf8Path(Io::NativePath(executables[index].OutputPath).filename()));
        }
        std::vector<std::string> modulePaths;
        for (const auto& artifact : guestArtifacts) {
            std::string relative = Io::Utf8Path(artifact.Path.lexically_relative(Io::NativePath(absPath).parent_path().lexically_normal() / "app0"));
            std::replace(relative.begin(), relative.end(), '\\', '/');
            modulePaths.push_back(std::move(relative));
        }
        Cli::RuntimeLayout::Print(std::cout, Io::Utf8Path(Io::NativePath(absPath).filename()), modulePaths, extraPaths);
        if (args.runPath != "$ORIGIN/libs") std::cout << "Custom library search path (--rpath): " << args.runPath << '\n';

        if (args.autorun) return Cli::Autorun(absPath, args.toWindows);

    } catch (const Domain::RelinkerException& e) {
        progress.Finish();
        std::cerr << "FAIL: " << e.what();
        if (e.FailureOffset != 0) std::cerr << " (offset 0x" << std::hex << e.FailureOffset << ")";
        std::cerr << "\n";
        if (!e.InputPath.empty()) std::cerr << "Input: " << e.InputPath << '\n';
        return 2;
    } catch (const Codegen::CodegenException& e) {
        progress.Finish();
        std::cerr << "FAIL: " << e.what();
        if (e.FailureOffset != 0) std::cerr << " (offset 0x" << std::hex << e.FailureOffset << ")";
        std::cerr << "\n";
        return 2;
    } catch (const std::exception& e) {
        progress.Finish();
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }

    return 0;
}

#ifdef _WIN32

std::string Utf8Argument(const wchar_t* argument) {
    const std::size_t argumentSize = std::wcslen(argument);
    if (argumentSize == 0) return {};
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument, static_cast<int>(argumentSize), nullptr, 0, nullptr, nullptr);
    if (size <= 0) throw std::runtime_error("Cannot encode the command line as UTF-8");
    std::string utf8(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argument, static_cast<int>(argumentSize), utf8.data(), size, nullptr, nullptr) != size)
        throw std::runtime_error("Cannot encode the command line as UTF-8");
    return utf8;
}

}

int wmain(const int argc, wchar_t* argv[]) {
    try {
        std::vector<std::string> arguments;
        arguments.reserve(static_cast<std::size_t>(argc));
        arguments.emplace_back();
        for (int index = 1; index < argc; ++index) arguments.push_back(Utf8Argument(argv[index]));
        std::vector<char*> pointers;
        pointers.reserve(arguments.size() + 1);
        for (auto& argument : arguments) pointers.push_back(argument.data());
        pointers.push_back(nullptr);
        return Run(static_cast<int>(arguments.size()), pointers.data());
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}

#else

}

int main(const int argc, char* argv[]) {
    return Run(argc, argv);
}

#endif
