#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/general/SegmentFilter.hpp>
#include <elfpatcher/general/EntryStubBuilder.hpp>
#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CallSiteResolver.hpp>
#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <codegen/IAmd64OnlyConverter.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <optional>
#include <elfpatcher/windows/WindowsElfPatcher.hpp>

int main(const int argc, char* argv[]) {
    bool skipSyscallCheck = false;
    bool toIntel = false;
    std::uint32_t unusedFilterLevel = 2;
    bool unusedFilterSpecified = false;
    bool writeRegistry = false;
    bool toWindows = false;

    std::string inputPath;
    std::string outputPath;
    std::string runPath = "$ORIGIN/libs";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--skip-syscall-check") {
            skipSyscallCheck = true;
        } else if (arg == "--to-intel") {
            toIntel = true;
        } else if (arg.rfind("unused-filter=", 0) == 0) {
            const std::string value = arg.substr(14);
            if (unusedFilterSpecified || value.size() != 1 || value[0] < '0' || value[0] > '2') {
                std::cerr << "FAIL: unused-filter must be specified once with a value of 0, 1 or 2\n";
                return 1;
            }
            unusedFilterLevel = static_cast<std::uint32_t>(value[0] - '0');
            unusedFilterSpecified = true;
        } else if (arg == "--registry") {
            writeRegistry = true;
        } else if (arg == "--rpath") {
            if (i + 1 >= argc) {
                std::cerr << "FAIL: --rpath requires a value\n";
                return 1;
            }
            runPath = argv[++i];
        } else if (arg == "--windows") {
            toWindows = true;
        } else if (arg.rfind("--", 0) == 0 || arg == "unused-filter") {
            std::cerr << "FAIL: unknown option: " << arg << "\n";
            return 1;
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else if (outputPath.empty()) {
            outputPath = arg;
        } else {
            std::cerr << "FAIL: unexpected argument: " << arg << "\n";
            return 1;
        }
    }

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: relinker [--windows] [--skip-syscall-check] [--to-intel] [unused-filter=0|1|2] [--registry] [--rpath <path>] <input.elf> <output.elf>\n"
             "Example: relinker input.elf output.elf\n";
        return 1;
    }

    try {
        Io::FileReader fileReader;
        Io::FileWriter fileWriter;

        auto sourceBytes = fileReader.Read(inputPath);

        if (toIntel) {
            std::cout << "Mode: Intel instruction conversion; system unchanged; unused-filter=" << unusedFilterLevel << " (not applied)\n";
            const Relinker::ElfReader elfReader(sourceBytes);
            const auto converter = Codegen::MakeAmd64OnlyConverter();
            auto result = converter->Convert(std::move(sourceBytes), elfReader.ReadCodeSegments());
            fileWriter.Write(outputPath, std::move(result.Bytes));
            std::cout << "OK: " << result.ReplacedCount << " instructions replaced\n";
            return 0;
        }

        auto elfReader = std::make_shared<Relinker::ElfReader>(sourceBytes);

        auto syscallScanner = skipSyscallCheck
            ? Relinker::MakeNullSyscallScanner()
            : Relinker::MakeSyscallScanner();

        const auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
            elfReader,
            std::move(syscallScanner),
            Relinker::MakeCallSiteResolver(),
            std::make_shared<Relinker::ValidationPolicy>(),
            std::make_shared<Relinker::SysVDynamicSectionBuilder>(),
            unusedFilterLevel == 2 ? Relinker::MakeStrictUnusedNidFilter() : Relinker::MakeUnusedNidFilter(),
            unusedFilterLevel
        );

        std::cout << "System: " << (toWindows ? "Windows" : "Linux") << "; unused-filter=" << unusedFilterLevel << "\n";
        auto result = pipeline->Relink(sourceBytes);
        for (const auto& patch : result.Patches) {
            if (patch.Offset > sourceBytes.size() || patch.Bytes.size() > sourceBytes.size() - patch.Offset)
                throw Domain::RelinkerException("Relinker patch exceeds source image", patch.Offset);
            for (std::size_t index = 0; index < patch.Bytes.size(); ++index) sourceBytes[patch.Offset + index] = patch.Bytes[index];
        }

        if (writeRegistry) {
            const std::filesystem::path outFsPath(outputPath);
            const std::string registryPath = (outFsPath.parent_path() / (outFsPath.stem().string() + ".registry.json")).string();
            auto callRegistryWriter = std::make_shared<Relinker::CallRegistryWriter>();
            fileWriter.Write(registryPath, callRegistryWriter->WriteCallRegistry(result.RegistryEntries));
        }

        auto byteWriter = std::make_shared<Io::ByteWriter>();

        std::shared_ptr<Elfpatcher::IElfPatcher> patcher;
        if (toWindows) {
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
        auto patched = patcher->Patch(sourceBytes, result.OriginalHeaders, result.DynamicSection, result.OriginalPltGotVaddr, runPath);
        fileWriter.Write(outputPath, patched);
        std::cout << "OK: " << result.RegistryEntries.size() << " NID references processed; output written\n";

    } catch (const Domain::RelinkerException& e) {
        std::cerr << "FAIL: " << e.what();
        if (e.FailureOffset != 0)
            std::cerr << " (offset 0x" << std::hex << e.FailureOffset << ")";
        std::cerr << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << "\n";
        return 2;
    }

    return 0;
}
