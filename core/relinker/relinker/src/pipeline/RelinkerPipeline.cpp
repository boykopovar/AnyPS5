#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <relinker/analysis/ValidationPolicy.hpp>
#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <sstream>
#include <iostream>
#include <cstring>
#include <domain/ImportModule.hpp>

namespace Relinker {

using namespace Elfpatcher;

RelinkerPipeline::RelinkerPipeline(std::shared_ptr<IElfReader> elfReader, std::shared_ptr<ISyscallScanner> syscallScanner, std::shared_ptr<ICallSiteResolver> callSiteResolver, std::shared_ptr<IValidationPolicy> validationPolicy, std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder, std::shared_ptr<IUnusedNidFilter> unusedNidFilter, std::uint32_t unusedFilterLevel)
    : _elfReader(std::move(elfReader))
    , _syscallScanner(std::move(syscallScanner))
    , _callSiteResolver(std::move(callSiteResolver))
    , _validationPolicy(std::move(validationPolicy))
    , _dynamicSectionBuilder(std::move(dynamicSectionBuilder))
    , _unusedNidFilter(std::move(unusedNidFilter))
    , unusedFilterLevel(unusedFilterLevel)
{
    if (unusedFilterLevel > 2) throw RelinkerException("Unused NID filter level must be 0, 1 or 2");
}

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
    std::vector<std::pair<std::vector<std::uint8_t>, VirtualAddress>> executableSegments;
    VirtualAddress gotVAddr = 0;
    ByteCount gotSize = 0;

    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
            auto segment = _elfReader->ReadSegment(ph);
            if (!segment.empty()) {
                if (textSection.empty()) {
                    textSection = segment;
                    textVAddr = ph.MappedAddress;
                }
                executableSegments.emplace_back(std::move(segment), ph.MappedAddress);
            }
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

    const bool hasPltRelocations = hasTag(DT_OS_PLTRELSZ) || hasTag(DT_PLTRELSZ)
        || hasTag(DT_OS_PLTREL) || hasTag(DT_PLTREL)
        || hasTag(DT_OS_JMPREL) || hasTag(DT_JMPREL);
    if (hasPltRelocations || hasTag(DT_OS_PLTGOT) || hasTag(DT_PLTGOT)) {
        gotVAddr = requireExactlyOneOf(DT_OS_PLTGOT, DT_PLTGOT, "DT_PLTGOT")
            ? getTagValue(DT_OS_PLTGOT)
            : getTagValue(DT_PLTGOT);
    }

    const FileByteOffset dynStrTabOffset = readAsOffset(DT_OS_STRTAB, DT_STRTAB, "DT_STRTAB");
    const ByteCount dynStrTabSize = readAsSize(DT_OS_STRSZ, DT_STRSZ, "DT_STRSZ");

    const FileByteOffset dynSymTabOffset = readAsOffset(DT_OS_SYMTAB, DT_SYMTAB, "DT_SYMTAB");
    constexpr std::size_t symEntSize = 24;
    if (readAsSize(DT_OS_SYMENT, DT_SYMENT, "DT_SYMENT") != symEntSize)
        throw RelinkerException("Unsupported DT_SYMENT value");

    FileByteOffset dynJmpRelOffset = 0;
    if (hasPltRelocations) {
        gotSize = readAsSize(DT_OS_PLTRELSZ, DT_PLTRELSZ, "DT_PLTRELSZ");
        const std::int64_t jmprelType = requireExactlyOneOf(DT_OS_PLTREL, DT_PLTREL, "DT_PLTREL")
            ? getTagValue(DT_OS_PLTREL)
            : getTagValue(DT_PLTREL);
        if (jmprelType != DT_RELA)
            throw RelinkerException("Unsupported DT_PLTREL type");
        dynJmpRelOffset = readAsOffset(DT_OS_JMPREL, DT_JMPREL, "DT_JMPREL");
        if (gotSize % 24 != 0)
            throw RelinkerException("Invalid DT_PLTRELSZ value");
    }
    const ByteCount dynJmpRelSize = gotSize;

    const FileByteOffset dynRelaOffset = readAsOffset(DT_OS_RELA, DT_RELA, "DT_RELA");
    const ByteCount dynRelaSize = readAsSize(DT_OS_RELASZ, DT_RELASZ, "DT_RELASZ");
    constexpr std::size_t relaEntSize = 24;
    if (readAsSize(DT_OS_RELAENT, DT_RELAENT, "DT_RELAENT") != relaEntSize)
        throw RelinkerException("Unsupported DT_RELAENT value");
    if (dynRelaSize % relaEntSize != 0)
        throw RelinkerException("Invalid DT_RELASZ value");

    std::vector<std::pair<std::uint64_t, std::string>> neededLibraryNamesByStrOffset;
    for (const auto& tag : dynTags)
        if (tag.Tag == DT_NEEDED)
            neededLibraryNamesByStrOffset.emplace_back(tag.Value, std::string());

    std::vector<NidReference> nidRefs;
    std::vector<std::string> neededLibraries;
    auto policy = std::dynamic_pointer_cast<ValidationPolicy>(_validationPolicy);

    const std::vector<std::uint8_t>& raw = _elfReader->GetRawBytes();

    if (dynStrTabOffset > raw.size() || dynStrTabSize > raw.size() - dynStrTabOffset)
        throw RelinkerException("Dynamic string table is out of bounds", dynStrTabOffset);

    if (hasPltRelocations && (dynJmpRelOffset > raw.size() || dynJmpRelSize > raw.size() - dynJmpRelOffset))
        throw RelinkerException("Jump relocation table is out of bounds", dynJmpRelOffset);

    auto readCStr = [&](FileByteOffset strOff) -> std::string {
        if (strOff >= dynStrTabSize)
            throw RelinkerException("Dynamic string offset is outside DT_STRSZ", strOff);
        std::string result;
        FileByteOffset pos = dynStrTabOffset + strOff;
        const FileByteOffset end = dynStrTabOffset + dynStrTabSize;
        while (pos < end && raw[pos] != 0)
            result.push_back(static_cast<char>(raw[pos++]));
        if (pos == end)
            throw RelinkerException("Dynamic string is not NUL-terminated within DT_STRSZ", strOff);
        return result;
    };

    for (auto& [fst, snd] : neededLibraryNamesByStrOffset) {
        snd = readCStr(fst);
        neededLibraries.push_back(snd);
        if (policy) policy->RegisterLibraryImport(snd);
    }

    std::map<std::uint64_t, std::string> importModules;
    for (const auto& tag : dynTags) {
        if (tag.Tag == 0x61000045 && !importModules.emplace(tag.Value >> 48, readCStr(tag.Value & 0xffffffffu)).second)
            throw RelinkerException("Duplicate import module ID");
    }

    auto relaEntryPos = [&](const FileByteOffset relaOff, const ByteCount off) -> FileByteOffset {
        if (relaOff > raw.size() || off > raw.size() - relaOff || relaEntSize > raw.size() - relaOff - off)
            throw RelinkerException("Relocation entry out of bounds", relaOff);
        return relaOff + off;
    };

    auto extractRela = [&](const FileByteOffset relaOff, const ByteCount relaSize) {
        for (ByteCount off = 0; relaSize >= relaEntSize && off <= relaSize - relaEntSize; off += relaEntSize) {
            const FileByteOffset pos = relaEntryPos(relaOff, off);

            std::uint64_t rOffset = 0, rInfo = 0;
            std::int64_t rAddend = 0;
            std::memcpy(&rOffset, raw.data() + pos, 8);
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);
            std::memcpy(&rAddend, raw.data() + pos + 16, 8);

            const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

            if (relType == 8) {
                if (symIdx != 0)
                    throw RelinkerException("RELATIVE relocation has a nonzero symbol index", pos);
                continue;
            }

            const FileByteOffset symOff = dynSymTabOffset + static_cast<FileByteOffset>(symIdx) * symEntSize;
            if (symOff < dynSymTabOffset || symOff > raw.size() || raw.size() - symOff < 4)
                throw RelinkerException("Symbol table entry out of bounds", symOff);

            std::uint32_t nameOff = 0;
            std::memcpy(&nameOff, raw.data() + symOff, 4);

            const auto name = readCStr(nameOff);
            nidRefs.push_back({name, Domain::ImportModule(name, importModules, neededLibraries), relType, pos, rOffset, rAddend});
        }
    };

    extractRela(dynRelaOffset, dynRelaSize);
    extractRela(dynJmpRelOffset, dynJmpRelSize);

    for (const auto& ref : nidRefs)
        _validationPolicy->ValidateRelocationTypeSupported(ref.RelocationTypeValue, ref.RelocationTableOffset);

    for (const auto& [segment, segmentVAddr] : executableSegments)
        _syscallScanner->ScanCodeSectionForSyscalls(segment, segmentVAddr, segment.size());

    _validationPolicy->ValidateSyscallAbsence();

    const std::size_t originalNidCount = nidRefs.size();
    const auto originalNidRefs = nidRefs;
    std::cout << "NID input: " << originalNidCount << " references\n";

    if (unusedFilterLevel == 2) {
        if (executableSegments.size() > 1)
            throw RelinkerException("Strict NID filtering does not support multiple executable segments");
        nidRefs = _unusedNidFilter->Filter(nidRefs, raw, textSection, textVAddr);
        if (nidRefs.size() > originalNidCount) throw RelinkerException("Strict NID filter increased the reference count");
        std::cout << "Strict filtering total: " << originalNidCount << " -> " << nidRefs.size() << "; filtered=" << originalNidCount - nidRefs.size() << "\n";
    } else if (unusedFilterLevel == 1) {
        std::vector<NidReference> pltRefs;
        std::vector<NidReference> nonPltRefs;
        for (const auto& ref : nidRefs) {
            if (ref.RelocationTypeValue == R_X86_64_JUMP_SLOT)
                pltRefs.push_back(ref);
            else
                nonPltRefs.push_back(ref);
        }

        std::cout << "PLT preservation: " << pltRefs.size() << " -> " << pltRefs.size() << "; filtered=0\n";
        const std::size_t nonPltCount = nonPltRefs.size();
        if (executableSegments.size() > 1) {
            std::cout << "CFG/GOT filtering skipped: multiple executable segments are not modeled; "
                      << nonPltCount << " -> " << nonPltCount << "; filtered=0\n";
        } else {
            nonPltRefs = _unusedNidFilter->Filter(nonPltRefs, raw, textSection, textVAddr);
            if (nonPltRefs.size() > nonPltCount) throw RelinkerException("Unused NID filter increased the reference count");
            std::cout << "CFG/GOT filtering: " << nonPltCount << " -> " << nonPltRefs.size() << "; filtered=" << nonPltCount - nonPltRefs.size() << "\n";
        }

        nidRefs.clear();
        nidRefs.reserve(pltRefs.size() + nonPltRefs.size());
        for (auto& ref : pltRefs) nidRefs.push_back(std::move(ref));
        for (auto& ref : nonPltRefs) nidRefs.push_back(std::move(ref));
    } else {
        std::cout << "Unused NID filtering: disabled; filtered=0\n";
    }

    std::cout << "NID total: " << originalNidCount << " -> " << nidRefs.size() << "; filtered=" << originalNidCount - nidRefs.size() << "\n";

    auto dynamicRefs = nidRefs;
    std::vector<RelinkPatch> patches;
    auto pltCount = static_cast<std::uint32_t>(dynJmpRelSize / relaEntSize);
    if (unusedFilterLevel == 2) {
        auto compacted = UnusedNidFilter::CompactPlt(originalNidRefs, nidRefs, textSection, textVAddr, _elfReader->TranslateVirtualAddress(textVAddr), dynJmpRelOffset);
        dynamicRefs = std::move(compacted.References);
        patches = std::move(compacted.Patches);
        std::cout << "PLT compaction: " << pltCount << " -> " << compacted.SlotCount << "\n";
        pltCount = compacted.SlotCount;
    }
    auto dynSection = _dynamicSectionBuilder->BuildDynamicSection(dynamicRefs, neededLibraries, dynJmpRelOffset, pltCount);

    auto appendRela = [&](std::vector<std::uint8_t>& buf, std::uint64_t offset, std::uint64_t info, std::int64_t addend) {
        std::size_t pos = buf.size();
        buf.resize(pos + 24);
        std::memcpy(buf.data() + pos, &offset, 8);
        std::memcpy(buf.data() + pos + 8, &info, 8);
        std::memcpy(buf.data() + pos + 16, &addend, 8);
    };

    auto extractRelative = [&](const FileByteOffset relaOff, const ByteCount relaSize) {
        for (ByteCount off = 0; relaSize >= relaEntSize && off <= relaSize - relaEntSize; off += relaEntSize) {
            const FileByteOffset pos = relaEntryPos(relaOff, off);
            std::uint64_t rOffset = 0, rInfo = 0;
            std::int64_t rAddend = 0;
            std::memcpy(&rOffset, raw.data() + pos, 8);
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);
            std::memcpy(&rAddend, raw.data() + pos + 16, 8);
            const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);
            if (symIdx == 0 && relType == R_X86_64_RELATIVE)
                appendRela(dynSection.RelaData, rOffset, static_cast<std::uint64_t>(R_X86_64_RELATIVE), rAddend);
        }
    };

    extractRelative(dynRelaOffset, dynRelaSize);
    extractRelative(dynJmpRelOffset, dynJmpRelSize);

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
        entry.CallSitesResolved = false;
        entries.push_back(std::move(entry));
    }

    for (const auto& [segment, segmentVAddr] : executableSegments) {
        for (auto& entry : entries) {
            auto segmentSites = _callSiteResolver->ResolveCallSites(segment, segmentVAddr, entry.TargetOffset, 8);
            entry.CallSites.insert(entry.CallSites.end(), segmentSites.begin(), segmentSites.end());
            entry.CallSitesResolved = !entry.CallSites.empty();
        }
    }

    return RelinkResult{std::move(entries), std::move(programHeaders), std::move(dynSection), gotVAddr, std::move(patches)};
}

}
