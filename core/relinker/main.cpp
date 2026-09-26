#include <Cli.hpp>
#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/windows/WindowsElfPatcher.hpp>
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
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

int main(const int argc, char* argv[]) {
    Cli::Args args;
    try {
        args = Cli::ParseArgs(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }

    try {
        Io::FileReader fileReader;
        Io::FileWriter fileWriter;

        auto sourceBytes = fileReader.Read(args.inputPath);
        const std::string absPath = std::filesystem::absolute(args.outputPath).string();

        if (args.toIntel) {
            std::cout << "Mode: Intel instruction conversion; system unchanged; unused-filter=" << args.unusedFilterLevel << " (not applied)\n";

            const Relinker::ElfReader elfReader(sourceBytes);
            const auto converter = Codegen::MakeAmd64OnlyConverter();

            auto codeSegments = elfReader.ReadCodeSegments();
            auto result = converter->Convert(std::move(sourceBytes), codeSegments);

            sourceBytes = std::move(result.Bytes);
            std::cout << "OK: " << result.ReplacedCount << " instructions replaced\n";
        }

        auto elfReader = std::make_shared<Relinker::ElfReader>(sourceBytes);

        const auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
            elfReader,
            args.skipSyscallCheck ? Relinker::MakeNullSyscallScanner() : Relinker::MakeSyscallScanner(),
            Relinker::MakeCallSiteResolver(),
            std::make_shared<Relinker::ValidationPolicy>(),
            std::make_shared<Relinker::SysVDynamicSectionBuilder>(),
            args.unusedFilterLevel == 2 ? Relinker::MakeStrictUnusedNidFilter() : Relinker::MakeUnusedNidFilter(),
            args.unusedFilterLevel
        );

        std::cout << "System: " << (args.toWindows ? "Windows" : "Linux") << "; unused-filter=" << args.unusedFilterLevel << "\n";
        auto result = pipeline->Relink(sourceBytes);
        for (const auto& patch : result.Patches) {
            if (patch.Offset > sourceBytes.size() || patch.Bytes.size() > sourceBytes.size() - patch.Offset)
                throw Domain::RelinkerException("Relinker patch exceeds source image", patch.Offset);
            for (std::size_t index = 0; index < patch.Bytes.size(); ++index) sourceBytes[patch.Offset + index] = patch.Bytes[index];
        }

        const auto guestArtifacts = Relinker::GuestModuleBuilder().Build(args.inputPath, absPath, result.DynamicSection, args.toWindows, args.toIntel, args.skipSyscallCheck, args.lazyBinding, args.runPath);

        if (args.writeRegistry) {
            const std::filesystem::path outFsPath(absPath);
            const std::string registryPath = (outFsPath.parent_path() / (outFsPath.stem().string() + ".registry.json")).string();
            fileWriter.Write(registryPath, std::make_shared<Relinker::CallRegistryWriter>()->WriteCallRegistry(result.RegistryEntries));
        }

        auto byteWriter = std::make_shared<Io::ByteWriter>();

        std::shared_ptr<Elfpatcher::IElfPatcher> patcher;
        if (args.toWindows) {
            patcher = std::make_shared<Elfpatcher::Windows::WindowsPePatcher>();
        } else {
            patcher = std::make_shared<Elfpatcher::Linux::LinuxElfPatcher>(
                std::make_shared<Elfpatcher::EntryStubBuilder>(),
                std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(
                    std::make_shared<Elfpatcher::SegmentFilter>(),
                    byteWriter
                ),
                std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
                byteWriter
            );
        }

        const auto executableBytes = patcher->Patch(sourceBytes, result.OriginalHeaders, result.DynamicSection, result.OriginalPltGotVaddr, args.runPath, args.lazyBinding, args.windowsDiagnostics);
        for (const auto& artifact : guestArtifacts) {
            std::filesystem::create_directories(artifact.Path.parent_path());
            fileWriter.Write(artifact.Path.string(), artifact.Bytes);
            std::cout << "Guest module: " << artifact.Path.string() << '\n';
        }
        fileWriter.Write(absPath, executableBytes);
        std::cout << "External prx references: " << result.RegistryEntries.size() << "\nOutput file: " << absPath << '\n';

        if (args.autorun) return Cli::Autorun(absPath, args.toWindows);

    } catch (const Domain::RelinkerException& e) {
        std::cerr << "FAIL: " << e.what();
        if (e.FailureOffset != 0) std::cerr << " (offset 0x" << std::hex << e.FailureOffset << ")";
        std::cerr << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }

    return 0;
}
