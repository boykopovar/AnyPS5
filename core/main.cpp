#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <elfpatcher/linux/LinuxElfPatcher.hpp>
#include <elfpatcher/filtering/SegmentFilter.hpp>
#include <elfpatcher/stub/EntryStubBuilder.hpp>
#include <elfpatcher/layout/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/sections/SectionHeaderTableBuilder.hpp>
#include <io/ByteWriter.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CallSiteResolver.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <optional>

int main(const int argc, char* argv[]) {
    bool skipSyscallCheck = false;
    std::string inputPath;
    std::string outputElfPath;
    std::optional<std::string> registryPath;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--skip-syscall-check") {
            skipSyscallCheck = true;
        } else if (inputPath.empty()) {
            inputPath = arg;
        } else if (outputElfPath.empty()) {
            outputElfPath = arg;
        } else if (!registryPath.has_value()) {
            registryPath = arg;
        }
    }

    if (inputPath.empty() || outputElfPath.empty()) {
        std::cerr << "Usage: relinker [--skip-syscall-check] <input.elf> <output.elf> [output_registry.json]\n";
        return 1;
    }

    try {
        Io::FileReader fileReader;
        Io::FileWriter fileWriter;

        auto sourceBytes = fileReader.Read(inputPath);

        auto elfReader = std::make_shared<Relinker::ElfReader>(sourceBytes);

        auto syscallScanner = skipSyscallCheck
            ? Relinker::MakeNullSyscallScanner()
            : Relinker::MakeSyscallScanner();

        const auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
            elfReader,
            std::move(syscallScanner),
            Relinker::MakeCallSiteResolver(),
            std::make_shared<Relinker::ValidationPolicy>(),
            std::make_shared<Relinker::SysVDynamicSectionBuilder>()
        );

        auto result = pipeline->Relink(sourceBytes);

        if (registryPath.has_value()) {
            auto callRegistryWriter = std::make_shared<Relinker::CallRegistryWriter>();
            fileWriter.Write(*registryPath, callRegistryWriter->WriteCallRegistry(result.RegistryEntries));
        }

        auto byteWriter = std::make_shared<Io::ByteWriter>();
        Elfpatcher::Linux::LinuxElfPatcher elfPatcher(
            std::make_shared<Elfpatcher::EntryStubBuilder>(),
            std::make_shared<Elfpatcher::ProgramHeaderLayoutBuilder>(
                std::make_shared<Elfpatcher::SegmentFilter>(),
                byteWriter
            ),
            std::make_shared<Elfpatcher::SectionHeaderTableBuilder>(byteWriter),
            byteWriter
        );
        auto patchedElf = elfPatcher.Patch(sourceBytes, result.OriginalHeaders, result.DynamicSection);
        fileWriter.Write(outputElfPath, patchedElf);

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
