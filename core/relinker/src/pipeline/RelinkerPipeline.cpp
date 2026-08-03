#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <sstream>
#include <iostream>
#include <cstring>

namespace Relinker {

RelinkerPipeline::RelinkerPipeline(
    std::shared_ptr<IElfReader> elfReader,
    std::shared_ptr<ISceDynlibParser> dynlibParser,
    std::shared_ptr<ISyscallScanner> syscallScanner,
    std::shared_ptr<ICallSiteResolver> callSiteResolver,
    std::shared_ptr<IValidationPolicy> validationPolicy,
    std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder)
    : _elfReader(std::move(elfReader))
    , _dynlibParser(std::move(dynlibParser))
    , _syscallScanner(std::move(syscallScanner))
    , _callSiteResolver(std::move(callSiteResolver))
    , _validationPolicy(std::move(validationPolicy))
    , _dynamicSectionBuilder(std::move(dynamicSectionBuilder))
{}

std::string RelinkerPipeline::_relocationTypeName(std::uint32_t type) {
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

RelinkResult RelinkerPipeline::Relink(const std::vector<std::uint8_t>& sourceElf) {
    auto programHeaders = _elfReader->ReadProgramHeaders();

    std::vector<std::uint8_t> sceDynlibData;
    std::vector<std::uint8_t> textSection;
    VirtualAddress textVAddr = 0;
    VirtualAddress gotVAddr = 0;
    ByteCount gotSize = 0;

    for (const auto& ph : programHeaders)
        if (ph.Type == PT_SCE_DYNLIBDATA)
            sceDynlibData = _elfReader->ReadSegment(ph);

    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
            textSection = _elfReader->ReadSegment(ph);
            textVAddr = ph.MappedAddress;
            break;
        }
    }

    FileByteOffset dynStrTabOffset = 0;
    ByteCount dynStrTabSize = 0;
    FileByteOffset dynSymTabOffset = 0;
    FileByteOffset dynRelaOffset = 0;
    ByteCount dynRelaSize = 0;
    FileByteOffset dynJmpRelOffset = 0;
    ByteCount dynJmpRelSize = 0;
    std::vector<std::pair<std::uint64_t, std::string>> neededLibraryNamesByStrOffset;

    for (const auto& ph : programHeaders) {
        if (ph.Type != PT_DYNAMIC)
            continue;

        for (const auto& tag : _elfReader->ReadDynamicTags(ph)) {
            if (tag.Tag == DT_PLTGOT || tag.Tag == DT_OS_PLTGOT)
                gotVAddr = tag.Value;
            else if (tag.Tag == DT_PLTRELSZ || tag.Tag == DT_OS_PLTRELSZ)
                gotSize = tag.Value;
            else if (tag.Tag == DT_STRTAB)
                dynStrTabOffset = _elfReader->TranslateVirtualAddress(tag.Value);
            else if (tag.Tag == DT_STRSZ)
                dynStrTabSize = tag.Value;
            else if (tag.Tag == DT_SYMTAB)
                dynSymTabOffset = _elfReader->TranslateVirtualAddress(tag.Value);
            else if (tag.Tag == DT_RELA)
                dynRelaOffset = _elfReader->TranslateVirtualAddress(tag.Value);
            else if (tag.Tag == DT_RELASZ)
                dynRelaSize = tag.Value;
            else if (tag.Tag == DT_SYSV_JMPREL)
                dynJmpRelOffset = _elfReader->TranslateVirtualAddress(tag.Value);
            else if (tag.Tag == DT_PLTRELSZ_SYSV)
                dynJmpRelSize = tag.Value;
            else if (tag.Tag == DT_NEEDED)
                neededLibraryNamesByStrOffset.emplace_back(tag.Value, std::string());
        }
        break;
    }

    std::vector<NidReference> nidRefs;
    std::vector<std::string> neededLibraries;
    auto policy = std::dynamic_pointer_cast<ValidationPolicy>(_validationPolicy);

    if (!sceDynlibData.empty()) {
        auto imports = _dynlibParser->ParseLibraryImports(sceDynlibData);
        auto relocations = _dynlibParser->ParseRelocationTable(sceDynlibData);
        nidRefs = _dynlibParser->ExtractNidReferences(relocations, imports);

        for (const auto& imp : imports) {
            if (policy) policy->RegisterLibraryImport(imp.LibraryName);
            neededLibraries.push_back(imp.LibraryName);
        }
        for (const auto& ref : nidRefs) {
            _validationPolicy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);
            _validationPolicy->ValidateNidBelongsToLibrary(ref.Nid, ref.Library);
        }
    } else if (dynStrTabOffset != 0) {
        const std::vector<std::uint8_t>& raw = _elfReader->GetRawBytes();

        auto readCStr = [&](FileByteOffset strOff) -> std::string {
            std::string result;
            FileByteOffset pos = dynStrTabOffset + strOff;
            while (pos < raw.size() && raw[pos] != 0)
                result.push_back(static_cast<char>(raw[pos++]));
            return result;
        };

        for (auto& [fst, snd] : neededLibraryNamesByStrOffset) {
            snd = readCStr(fst);
            neededLibraries.push_back(snd);
            if (policy) policy->RegisterLibraryImport(snd);
        }

        auto extractRela = [&](const FileByteOffset relaOff, const ByteCount relaSize) {
            constexpr std::size_t entrySize = 24;
            for (ByteCount off = 0; off + entrySize <= relaSize; off += entrySize) {
                const FileByteOffset pos = relaOff + off;
                if (pos + entrySize > raw.size())
                    break;

                std::uint64_t rOffset = 0, rInfo = 0;
                std::memcpy(&rOffset, raw.data() + pos, 8);
                std::memcpy(&rInfo, raw.data() + pos + 8, 8);

                const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
                const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

                if (dynSymTabOffset == 0)
                    continue;

                const FileByteOffset symOff = dynSymTabOffset + static_cast<FileByteOffset>(symIdx) * 24;
                if (symOff + 4 > raw.size())
                    continue;

                std::uint32_t nameOff = 0;
                std::memcpy(&nameOff, raw.data() + symOff, 4);

                NidReference ref;
                ref.Nid = readCStr(nameOff);
                ref.Library = {};
                ref.RelocationTypeValue = relType;
                ref.RelocationTableOffset = pos;
                ref.RelocationAddress = rOffset;
                nidRefs.push_back(ref);
            }
        };

        if (dynRelaOffset != 0) extractRela(dynRelaOffset, dynRelaSize);
        if (dynJmpRelOffset != 0) extractRela(dynJmpRelOffset, dynJmpRelSize);

        for (const auto& ref : nidRefs)
            _validationPolicy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);
    } else {
        throw RelinkerException("Neither SCE_DYNLIBDATA segment nor hybrid PT_DYNAMIC NID data found");
    }

    if (!textSection.empty())
        _syscallScanner->ScanCodeSectionForSyscalls(textSection, textVAddr, textSection.size());

    _validationPolicy->ValidateSyscallAbsence();

    auto dynSection = _dynamicSectionBuilder->BuildDynamicSection(nidRefs, neededLibraries);

    std::vector<FileByteOffset> callSites;
    bool callSitesResolved = false;
    if (!textSection.empty() && gotSize > 0) {
        callSites = _callSiteResolver->ResolveCallSites(textSection, textVAddr, gotVAddr, gotSize);
        callSitesResolved = !callSites.empty();
    }

    std::vector<CallRegistryEntry> entries;
    entries.reserve(nidRefs.size());
    for (const auto& ref : nidRefs) {
        CallRegistryEntry entry;
        entry.Nid = ref.Nid;
        entry.Library = ref.Library;
        entry.RelocationTypeString = _relocationTypeName(ref.RelocationTypeValue);
        entry.RelocationOffset = ref.RelocationTableOffset;
        entry.TargetSection = ".got";
        entry.TargetOffset = ref.RelocationAddress;
        entry.CallSites = callSites;
        entry.CallSitesResolved = callSitesResolved;
        entries.push_back(std::move(entry));
    }

    std::cout << "OK: " << entries.size() << " NID references processed\n";

    return RelinkResult{std::move(entries), std::move(programHeaders), std::move(dynSection)};
}

}
