#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <sstream>
#include <iostream>
#include <cstring>

namespace Relinker {

RelinkerPipeline::RelinkerPipeline(
    std::shared_ptr<IElfReader> elfReader,
    std::shared_ptr<ISyscallScanner> syscallScanner,
    std::shared_ptr<ICallSiteResolver> callSiteResolver,
    std::shared_ptr<IValidationPolicy> validationPolicy,
    std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder)
    : _elfReader(std::move(elfReader))
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

    std::vector<std::uint8_t> textSection;
    VirtualAddress textVAddr = 0;
    VirtualAddress gotVAddr = 0;
    ByteCount gotSize = 0;

    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
            textSection = _elfReader->ReadSegment(ph);
            textVAddr = ph.MappedAddress;
            break;
        }
    }

    std::vector<DynamicTag> dynTags;
    bool hasDynamicSegment = false;

    for (const auto& ph : programHeaders) {
        if (ph.Type != PT_DYNAMIC)
            continue;

        hasDynamicSegment = true;
        dynTags = _elfReader->ReadDynamicTags(ph);
        break;
    }

    if (!hasDynamicSegment)
        throw RelinkerException("No PT_DYNAMIC segment found");

    auto hasTag = [&](const std::int64_t tag) {
        for (const auto& t : dynTags)
            if (t.Tag == tag)
                return true;
        return false;
    };

    auto getTagValue = [&](const std::int64_t tag) -> std::uint64_t {
        for (const auto& t : dynTags)
            if (t.Tag == tag)
                return t.Value;
        throw RelinkerException("DT tag not found");
    };

    auto requireExactlyOneOf = [&](const std::int64_t osTag, const std::int64_t sysvTag, const char* name) {
        const bool hasOs = hasTag(osTag);
        const bool hasSysv = hasTag(sysvTag);
        if (hasOs && hasSysv)
            throw RelinkerException(std::string("Both DT_OS_ and DT_ variants present for ") + name);
        if (!hasOs && !hasSysv)
            throw RelinkerException(std::string("Neither DT_OS_ nor DT_ variant present for ") + name);
        return hasOs;
    };

    auto readAsOffset = [&](const std::int64_t osTag, const std::int64_t sysvTag, const char* name) -> FileByteOffset {
        if (requireExactlyOneOf(osTag, sysvTag, name))
            return getTagValue(osTag);
        return _elfReader->TranslateVirtualAddress(getTagValue(sysvTag));
    };

    auto readAsSize = [&](const std::int64_t osTag, const std::int64_t sysvTag, const char* name) -> ByteCount {
        requireExactlyOneOf(osTag, sysvTag, name);
        return hasTag(osTag) ? getTagValue(osTag) : getTagValue(sysvTag);
    };

    if (requireExactlyOneOf(DT_OS_PLTGOT, DT_PLTGOT, "DT_PLTGOT"))
        gotVAddr = getTagValue(DT_OS_PLTGOT);
    else
        gotVAddr = getTagValue(DT_PLTGOT);
    gotSize = readAsSize(DT_OS_PLTRELSZ, DT_PLTRELSZ, "DT_PLTRELSZ");

    const FileByteOffset dynStrTabOffset = readAsOffset(DT_OS_STRTAB, DT_STRTAB, "DT_STRTAB");
    requireExactlyOneOf(DT_OS_STRSZ, DT_STRSZ, "DT_STRSZ");

    const FileByteOffset dynSymTabOffset = readAsOffset(DT_OS_SYMTAB, DT_SYMTAB, "DT_SYMTAB");
    requireExactlyOneOf(DT_OS_SYMENT, DT_SYMENT, "DT_SYMENT");

    const std::int64_t jmprelType = requireExactlyOneOf(DT_OS_PLTREL, DT_PLTREL, "DT_PLTREL")
        ? getTagValue(DT_OS_PLTREL)
        : getTagValue(DT_PLTREL);
    if (jmprelType != DT_RELA)
        throw RelinkerException("Unsupported DT_PLTREL type");

    const FileByteOffset dynJmpRelOffset = readAsOffset(DT_OS_JMPREL, DT_JMPREL, "DT_JMPREL");
    const ByteCount dynJmpRelSize = gotSize;

    const FileByteOffset dynRelaOffset = readAsOffset(DT_OS_RELA, DT_RELA, "DT_RELA");
    const ByteCount dynRelaSize = readAsSize(DT_OS_RELASZ, DT_RELASZ, "DT_RELASZ");
    requireExactlyOneOf(DT_OS_RELAENT, DT_RELAENT, "DT_RELAENT");

    std::vector<std::pair<std::uint64_t, std::string>> neededLibraryNamesByStrOffset;
    for (const auto& tag : dynTags)
        if (tag.Tag == DT_NEEDED)
            neededLibraryNamesByStrOffset.emplace_back(tag.Value, std::string());

    std::vector<NidReference> nidRefs;
    std::vector<std::string> neededLibraries;
    auto policy = std::dynamic_pointer_cast<ValidationPolicy>(_validationPolicy);

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
                throw RelinkerException("Relocation entry out of bounds", pos);

            std::uint64_t rOffset = 0, rInfo = 0;
            std::memcpy(&rOffset, raw.data() + pos, 8);
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);

            const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

            const FileByteOffset symOff = dynSymTabOffset + static_cast<FileByteOffset>(symIdx) * 24;
            if (symOff + 4 > raw.size())
                throw RelinkerException("Symbol table entry out of bounds", symOff);

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

    extractRela(dynRelaOffset, dynRelaSize);
    extractRela(dynJmpRelOffset, dynJmpRelSize);

    for (const auto& ref : nidRefs)
        _validationPolicy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);

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
