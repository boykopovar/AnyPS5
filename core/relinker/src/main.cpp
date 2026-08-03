#include <domain/Types.hpp>
#include <io/FileReader.hpp>
#include <io/FileWriter.hpp>
#include <elfpatcher/ElfPatcher.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <relinker/parsing/SceDynlibParser.hpp>
#include <relinker/parsing/SdkRevisionProfile.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <relinker/analysis/SyscallScanner.hpp>
#include <relinker/analysis/CallSiteResolver.hpp>
#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <relinker/output/CallRegistryWriter.hpp>
#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <iostream>
#include <memory>

int main(const int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: relinker <input.elf> <output_registry.json> <output.elf>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string registryPath = argv[2];
    const std::string outputElfPath = argv[3];

    try {
        Io::FileReader fileReader;
        Io::FileWriter fileWriter;

        auto sourceBytes = fileReader.Read(inputPath);

        auto elfReader = std::make_shared<Relinker::ElfReader>(sourceBytes);
        auto programHeaders = elfReader->ReadProgramHeaders();

        std::vector<std::uint8_t> sceDynlibData;
        for (const auto& ph : programHeaders)
            if (ph.Type == 0x61000000)
                sceDynlibData = elfReader->ReadSegment(ph);

        auto sdkProfile = std::make_shared<Relinker::SdkRevisionProfile>(sceDynlibData);

        const auto pipeline = std::make_shared<Relinker::RelinkerPipeline>(
            elfReader,
            std::make_shared<Relinker::SceDynlibParser>(sdkProfile),
            Relinker::MakeSyscallScanner(),
            Relinker::MakeCallSiteResolver(),
            std::make_shared<Relinker::ValidationPolicy>(),
            std::make_shared<Relinker::SysVDynamicSectionBuilder>(),
            std::make_shared<Relinker::CallRegistryWriter>()
        );

        auto result = pipeline->Relink(sourceBytes);

        auto callRegistryWriter = std::make_shared<Relinker::CallRegistryWriter>();
        fileWriter.Write(registryPath, callRegistryWriter->WriteCallRegistry(result.RegistryEntries));

        Elfpatcher::ElfPatcher elfPatcher;
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
