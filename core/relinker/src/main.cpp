#include <relinker/ElfReader.hpp>
#include <relinker/SceDynlibParser.hpp>
#include <relinker/SdkRevisionProfile.hpp>
#include <relinker/ICallSiteResolver.hpp>
#include <relinker/ISysVDynamicSectionBuilder.hpp>
#include <relinker/ICallRegistryWriter.hpp>
#include <relinker/ISyscallScanner.hpp>
#include <relinker/IValidationPolicy.hpp>
#include "relinker/ValidationPolicy.hpp"

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
static constexpr std::uint32_t PF_X = 0x1;

static constexpr std::int64_t DT_PLTGOT = 0x00000003;
static constexpr std::int64_t DT_OS_PLTGOT = 0x61000027;
static constexpr std::int64_t DT_PLTRELSZ = 0x00000002;
static constexpr std::int64_t DT_OS_PLTRELSZ = 0x6100002d;
static constexpr std::int64_t DT_NEEDED = 0x00000001;
static constexpr std::int64_t DT_STRTAB = 0x00000005;
static constexpr std::int64_t DT_STRSZ = 0x0000000a;
static constexpr std::int64_t DT_RELA = 0x00000007;
static constexpr std::int64_t DT_RELASZ = 0x00000008;
static constexpr std::int64_t DT_JMPREL = 0x61000029;
static constexpr std::int64_t DT_SYSV_JMPREL = 0x00000017;
static constexpr std::int64_t DT_PLTRELSZ_SYSV = 0x00000002;
static constexpr std::int64_t DT_OS_IMPORT_LIB_1 = 0x61000049;
static constexpr std::uint32_t R_X86_64_JUMP_SLOT_VALUE = 7;

static std::string _relocationTypeName(const std::uint32_t type) {
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

        std::vector<std::uint8_t> sceDynlibData;
        std::vector<std::uint8_t> textSection;
        Relinker::VirtualAddress textVAddr = 0;
        Relinker::VirtualAddress gotVAddr = 0;
        Relinker::ByteCount gotSize = 0;

        for (const auto& ph : programHeaders) {
            if (ph.Type == PT_SCE_DYNLIBDATA)
                sceDynlibData = elfReader->ReadSegment(ph);
        }

        for (const auto& ph : programHeaders) {
            if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
                textSection = elfReader->ReadSegment(ph);
                textVAddr = ph.MappedAddress;
                break;
            }
        }

        Relinker::FileByteOffset dynStrTabOffset = 0;
        Relinker::ByteCount dynStrTabSize = 0;
        Relinker::FileByteOffset dynSymTabOffset = 0;
        Relinker::FileByteOffset dynRelaOffset = 0;
        Relinker::ByteCount dynRelaSize = 0;
        Relinker::FileByteOffset dynJmpRelOffset = 0;
        Relinker::ByteCount dynJmpRelSize = 0;
        std::vector<std::pair<std::uint64_t, std::string>> neededLibraryNamesByStrOffset;

        for (const auto& ph : programHeaders) {
            if (ph.Type != PT_DYNAMIC)
                continue;

            auto dynTags = elfReader->ReadDynamicTags(ph);

            for (const auto& tag : dynTags) {
                if (tag.Tag == DT_PLTGOT || tag.Tag == DT_OS_PLTGOT)
                    gotVAddr = tag.Value;
                else if (tag.Tag == DT_PLTRELSZ || tag.Tag == DT_OS_PLTRELSZ)
                    gotSize = tag.Value;
                else if (tag.Tag == DT_STRTAB)
                    dynStrTabOffset = elfReader->TranslateVirtualAddress(tag.Value);
                else if (tag.Tag == DT_STRSZ)
                    dynStrTabSize = tag.Value;
                else if (tag.Tag == 0x6)
                    dynSymTabOffset = elfReader->TranslateVirtualAddress(tag.Value);
                else if (tag.Tag == DT_RELA)
                    dynRelaOffset = elfReader->TranslateVirtualAddress(tag.Value);
                else if (tag.Tag == DT_RELASZ)
                    dynRelaSize = tag.Value;
                else if (tag.Tag == DT_SYSV_JMPREL)
                    dynJmpRelOffset = elfReader->TranslateVirtualAddress(tag.Value);
                else if (tag.Tag == DT_PLTRELSZ_SYSV)
                    dynJmpRelSize = tag.Value;
                else if (tag.Tag == DT_NEEDED)
                    neededLibraryNamesByStrOffset.emplace_back(tag.Value, std::string());
            }

            break;
        }

        std::vector<Relinker::NidReference> hybridNidRefs;
        std::vector<std::string> hybridNeededLibraries;

        if (sceDynlibData.empty() && dynStrTabOffset != 0) {
            auto readCString = [&](Relinker::FileByteOffset strOffset) -> std::string {
                std::string result;
                Relinker::FileByteOffset pos = dynStrTabOffset + strOffset;
                std::ifstream f(inputPath, std::ios::binary);
                f.seekg(static_cast<std::streamoff>(pos));
                char c;
                while (f.get(c) && c != '\0')
                    result.push_back(c);
                return result;
            };

            for (auto& entry : neededLibraryNamesByStrOffset) {
                entry.second = readCString(entry.first);
                hybridNeededLibraries.push_back(entry.second);
            }

            auto extractRelaEntries = [&](Relinker::FileByteOffset relaOffset, Relinker::ByteCount relaSize) {
                const std::size_t entrySize = 24;
                for (Relinker::ByteCount off = 0; off + entrySize <= relaSize; off += entrySize) {
                    std::ifstream f(inputPath, std::ios::binary);
                    f.seekg(static_cast<std::streamoff>(relaOffset + off));
                    std::uint64_t rOffset = 0, rInfo = 0;
                    std::int64_t rAddend = 0;
                    f.read(reinterpret_cast<char*>(&rOffset), 8);
                    f.read(reinterpret_cast<char*>(&rInfo), 8);
                    f.read(reinterpret_cast<char*>(&rAddend), 8);

                    const std::uint32_t symIndex = static_cast<std::uint32_t>(rInfo >> 32);
                    const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

                    if (dynSymTabOffset == 0)
                        continue;

                    const std::size_t symEntrySize = 24;
                    Relinker::FileByteOffset symOff = dynSymTabOffset + static_cast<Relinker::FileByteOffset>(symIndex) * symEntrySize;

                    std::ifstream fs(inputPath, std::ios::binary);
                    fs.seekg(static_cast<std::streamoff>(symOff));
                    std::uint32_t nameOffset = 0;
                    fs.read(reinterpret_cast<char*>(&nameOffset), 4);

                    std::string symName = readCString(nameOffset);

                    Relinker::NidReference ref;
                    ref.Nid = symName;
                    ref.Library = std::string();
                    ref.RelocationTypeValue = relType;
                    ref.RelocationTableOffset = relaOffset + off;
                    ref.RelocationAddress = rOffset;
                    hybridNidRefs.push_back(ref);
                }
            };

            if (dynRelaOffset != 0)
                extractRelaEntries(dynRelaOffset, dynRelaSize);
            if (dynJmpRelOffset != 0)
                extractRelaEntries(dynJmpRelOffset, dynJmpRelSize);
        }

        if (sceDynlibData.empty() && hybridNidRefs.empty())
            throw Relinker::RelinkerException("Neither SCE_DYNLIBDATA segment nor hybrid PT_DYNAMIC NID data found");

        std::vector<Relinker::NidReference> nidRefs;
        std::vector<std::string> neededLibraries;
        std::shared_ptr<Relinker::ValidationPolicy> policy = std::make_shared<Relinker::ValidationPolicy>();

        if (!sceDynlibData.empty()) {
            auto sdkProfile = std::make_shared<Relinker::SdkRevisionProfile>(sceDynlibData);
            auto parser = std::make_shared<Relinker::SceDynlibParser>(sdkProfile);

            auto imports = parser->ParseLibraryImports(sceDynlibData);
            auto relocations = parser->ParseRelocationTable(sceDynlibData);
            nidRefs = parser->ExtractNidReferences(relocations, imports);

            for (const auto& imp : imports) {
                policy->RegisterLibraryImport(imp.LibraryName);
                neededLibraries.push_back(imp.LibraryName);
            }

            for (const auto& ref : nidRefs) {
                policy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);
                policy->ValidateNidBelongsToLibrary(ref.Nid, ref.Library);
            }
        } else {
            nidRefs = std::move(hybridNidRefs);
            neededLibraries = std::move(hybridNeededLibraries);

            for (const auto& lib : neededLibraries)
                policy->RegisterLibraryImport(lib);

            for (const auto& ref : nidRefs)
                policy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);
        }

        auto syscallScan = Relinker::MakeSyscallScanner();
        auto csResolver = Relinker::MakeCallSiteResolver();
        auto dynBuilder = Relinker::MakeSysVDynamicSectionBuilder();
        auto regWriter = Relinker::MakeCallRegistryWriter();

        if (!textSection.empty())
            syscallScan->ScanCodeSectionForSyscalls(textSection, textVAddr, textSection.size());

        policy->ValidateSyscallAbsence();

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
