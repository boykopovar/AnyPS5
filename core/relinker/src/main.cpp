#include <relinker/ElfReader.hpp>
#include <relinker/SceDynlibParser.hpp>
#include <relinker/SdkRevisionProfile.hpp>
#include <relinker/ICallSiteResolver.hpp>
#include <relinker/ISysVDynamicSectionBuilder.hpp>
#include <relinker/ICallRegistryWriter.hpp>
#include <relinker/ISyscallScanner.hpp>
#include <relinker/IValidationPolicy.hpp>
#include "ValidationPolicy.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace Relinker {

std::unique_ptr<ISyscallScanner> MakeSyscallScanner();
std::shared_ptr<ICallSiteResolver> MakeCallSiteResolver();
std::shared_ptr<ISysVDynamicSectionBuilder> MakeSysVDynamicSectionBuilder();
std::shared_ptr<ICallRegistryWriter> MakeCallRegistryWriter();

}

static constexpr std::uint32_t PT_LOAD = 1;
static constexpr std::uint32_t PT_DYNAMIC = 2;
static constexpr std::uint32_t PT_SCE_DYNLIBDATA = 0x61000000;
static constexpr std::uint32_t SHT_NULL = 0;

static std::string _relocationTypeName(std::uint32_t type) {
    switch (type) {
        case 1: return "R_X86_64_64";
        case 6: return "R_X86_64_GLOB_DAT";
        case 7: return "R_X86_64_JUMP_SLOT";
        case 10: return "R_X86_64_32";
        default: {
            std::ostringstream oss;
            oss << "UNKNOWN(" << type << ")";
            return oss.str();
        }
    }
}

static void _writeFile(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) throw Relinker::RelinkerException("Cannot open output file: " + path);
    f << content;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: relinker <input.elf> <output_registry.json>\n";
        return 1;
    }

    const std::string inputPath = argv[1];
    const std::string registryPath = argv[2];

    try {
        auto elfReader = std::make_unique<Relinker::ElfReader>(inputPath);

        auto header = elfReader->ReadHeader();
        auto programHeaders = elfReader->ReadProgramHeaders();
        auto sectionHeaders = elfReader->ReadSectionHeaders();

        std::vector<std::uint8_t> sceDynlibData;
        std::vector<std::uint8_t> textSection;
        Relinker::VirtualAddress textVAddr = 0;
        Relinker::VirtualAddress gotVAddr = 0;
        Relinker::ByteCount gotSize = 0;

        for (const auto& ph : programHeaders) {
            if (ph.Type == PT_SCE_DYNLIBDATA)
                sceDynlibData = elfReader->ReadSegment(ph);
        }

        for (const auto& sh : sectionHeaders) {
            if (sh.Name == ".text") {
                textSection = elfReader->ReadSection(sh);
                textVAddr   = sh.MappedAddress;
            } else if (sh.Name == ".got" || sh.Name == ".got.plt") {
                gotVAddr = sh.MappedAddress;
                gotSize = sh.SectionSize;
            }
        }

        if (sceDynlibData.empty())
            throw Relinker::RelinkerException("SCE_DYNLIBDATA segment not found");

        auto sdkProfile = std::make_shared<Relinker::SdkRevisionProfile>(sceDynlibData);
        auto parser = std::make_shared<Relinker::SceDynlibParser>(sdkProfile);
        auto policy = std::make_shared<Relinker::ValidationPolicy>();
        auto syscallScan = Relinker::MakeSyscallScanner();
        auto csResolver = Relinker::MakeCallSiteResolver();
        auto dynBuilder = Relinker::MakeSysVDynamicSectionBuilder();
        auto regWriter = Relinker::MakeCallRegistryWriter();

        auto imports = parser->ParseLibraryImports(sceDynlibData);
        auto relocations = parser->ParseRelocationTable(sceDynlibData);
        auto nidRefs = parser->ExtractNidReferences(relocations, imports);

        for (const auto& imp : imports)
            policy->RegisterLibraryImport(imp.LibraryName);

        for (const auto& ref : nidRefs) {
            policy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);
            policy->ValidateNidBelongsToLibrary(ref.Nid, ref.Library);
        }

        if (!textSection.empty())
            syscallScan->ScanCodeSectionForSyscalls(textSection, textVAddr, textSection.size());

        policy->ValidateSyscallAbsence();

        std::vector<std::string> neededLibraries;
        for (const auto& imp : imports)
            neededLibraries.push_back(imp.LibraryName);

        auto sysVSection = dynBuilder->BuildDynamicSection(nidRefs, neededLibraries);

        std::vector<Relinker::CallRegistryEntry> entries;
        entries.reserve(nidRefs.size());

        for (const auto& ref : nidRefs) {
            std::vector<Relinker::FileByteOffset> callSites;
            bool resolved = false;

            if (!textSection.empty() && gotSize > 0) {
                callSites = csResolver->ResolveCallSites(
                    textSection, textVAddr, gotVAddr, gotSize);
                resolved = !callSites.empty();
            }

            Relinker::CallRegistryEntry entry;
            entry.Nid = ref.Nid;
            entry.Library = ref.Library;
            entry.RelocationTypeString = _relocationTypeName(ref.RelocationTypeValue);
            entry.RelocationOffset = ref.RelocationTableOffset;
            entry.TargetSection = ".got";
            entry.TargetOffset = ref.RelocationAddress;
            entry.CallSites = std::move(callSites);
            entry.CallSitesResolved = resolved;
            entries.push_back(std::move(entry));
        }

        _writeFile(registryPath, regWriter->WriteCallRegistry(entries));

        std::cout << "OK: " << entries.size() << " NID references processed\n";
        std::cout << "Registry: " << registryPath << "\n";

    } catch (const Relinker::RelinkerException& e) {
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
