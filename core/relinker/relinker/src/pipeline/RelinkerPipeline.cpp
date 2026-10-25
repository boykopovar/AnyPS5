#include <relinker/pipeline/RelinkerPipeline.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <relinker/analysis/UnusedNidFilter/PltCompactor.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/SceCodeExtent.hpp>
#include <relinker/analysis/RelativeJumpTables.hpp>
#include <algorithm>
#include <sstream>

#include <cstring>
#include <limits>
#include <tuple>
#include <utility>
#include <domain/ImportModule.hpp>

namespace Relinker {

using namespace Elfpatcher;

namespace {

constexpr std::uint16_t kEtNone = 0;
constexpr std::uint16_t kEtRel = 1;
constexpr std::uint16_t kEtCore = 4;

}

RelinkerPipeline::RelinkerPipeline(std::shared_ptr<IElfReader> elfReader, std::shared_ptr<ISyscallScanner> syscallScanner, std::shared_ptr<ICallSiteResolver> callSiteResolver, std::shared_ptr<IValidationPolicy> validationPolicy, std::shared_ptr<ISysVDynamicSectionBuilder> dynamicSectionBuilder, std::shared_ptr<IUnusedNidFilter> unusedNidFilter, std::uint32_t unusedFilterLevel, Domain::IProgress& progress)
    : elfReader(std::move(elfReader))
    , syscallScanner(std::move(syscallScanner))
    , callSiteResolver(std::move(callSiteResolver))
    , validationPolicy(std::move(validationPolicy))
    , dynamicSectionBuilder(std::move(dynamicSectionBuilder))
    , unusedNidFilter(std::move(unusedNidFilter))
    , unusedFilterLevel(unusedFilterLevel)
    , progress(progress)
{
    if (!this->elfReader || !this->syscallScanner || !this->callSiteResolver || !this->validationPolicy || !this->dynamicSectionBuilder || !this->unusedNidFilter) throw RelinkerException("Missing relinker pipeline dependency");
    if (unusedFilterLevel > 2) throw RelinkerException("Unused NID filter level must be 0, 1 or 2");
}

std::string RelinkerPipeline::relocationTypeName(std::uint32_t type) {
    switch (type) {
        case 1: return "R_X86_64_64";
        case 6: return "R_X86_64_GLOB_DAT";
        case 7: return "R_X86_64_JUMP_SLOT";
        case 10: return "R_X86_64_32";
        case 16: return "R_X86_64_DTPMOD64";
        case 17: return "R_X86_64_DTPOFF64";
        case 18: return "R_X86_64_TPOFF64";
        default: {
            std::ostringstream oss;
            oss << "UNKNOWN(" << type << ")";
            return oss.str();
        }
    }
}

RelinkResult RelinkerPipeline::Relink() {
    const auto inputType = elfReader->ReadHeader().Type;
    if (inputType == kEtRel)
        throw RelinkerException("ET_REL relocatable inputs are not supported");
    if (inputType == kEtNone)
        throw RelinkerException("ET_NONE inputs do not specify an ELF object type");
    if (inputType == kEtCore)
        throw RelinkerException("ET_CORE core dumps are not executable images");

    auto programHeaders = elfReader->ReadProgramHeaders();

    std::vector<std::uint8_t> textSection;
    VirtualAddress textVAddr = 0;
    std::vector<std::tuple<std::vector<std::uint8_t>, VirtualAddress, FileByteOffset>> executableSegments;
    VirtualAddress gotVAddr = 0;
    ByteCount gotSize = 0;

    for (const auto& ph : programHeaders) {
        if (ph.Type == PT_LOAD && (ph.Flags & PF_X) != 0) {
            auto segment = elfReader->ReadSegment(ph);
            if (!segment.empty()) {
                if (executableSegments.empty()) {
                    if (unusedFilterLevel != 0) textSection = segment;
                    textVAddr = ph.MappedAddress;
                }
                executableSegments.emplace_back(std::move(segment), ph.MappedAddress, ph.Offset);
            }
        }
    }

    FileByteOffset sceTableBase = 0;
    bool hasSceDynlibData = false;
    for (const auto& ph : programHeaders) {
        if (ph.Type != PT_SCE_DYNLIBDATA)
            continue;
        if (hasSceDynlibData)
            throw RelinkerException("Multiple PT_SCE_DYNLIBDATA segments", ph.Offset);
        hasSceDynlibData = true;
        sceTableBase = ph.Offset;
    }

    std::vector<DynamicTag> dynTags;
    bool hasDynamicSegment = false;

    for (const auto& ph : programHeaders) {
        if (ph.Type != PT_DYNAMIC)
            continue;

        hasDynamicSegment = true;
        dynTags = elfReader->ReadDynamicTags(ph);
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
        const char* name = "unknown";
        switch (tag) {
        case DT_OS_STRTAB: name = "DT_OS_STRTAB"; break;
        case DT_STRTAB: name = "DT_STRTAB"; break;
        case DT_OS_STRSZ: name = "DT_OS_STRSZ"; break;
        case DT_STRSZ: name = "DT_STRSZ"; break;
        case DT_OS_SYMTAB: name = "DT_OS_SYMTAB"; break;
        case DT_SYMTAB: name = "DT_SYMTAB"; break;
        case DT_OS_SYMENT: name = "DT_OS_SYMENT"; break;
        case DT_SYMENT: name = "DT_SYMENT"; break;
        case DT_OS_PLTGOT: name = "DT_OS_PLTGOT"; break;
        case DT_PLTGOT: name = "DT_PLTGOT"; break;
        case DT_OS_PLTREL: name = "DT_OS_PLTREL"; break;
        case DT_PLTREL: name = "DT_PLTREL"; break;
        case DT_OS_PLTRELSZ: name = "DT_OS_PLTRELSZ"; break;
        case DT_PLTRELSZ: name = "DT_PLTRELSZ"; break;
        case DT_OS_JMPREL: name = "DT_OS_JMPREL"; break;
        case DT_JMPREL: name = "DT_JMPREL"; break;
        case DT_OS_RELA: name = "DT_OS_RELA"; break;
        case DT_RELA: name = "DT_RELA"; break;
        case DT_OS_RELASZ: name = "DT_OS_RELASZ"; break;
        case DT_RELASZ: name = "DT_RELASZ"; break;
        case DT_OS_RELAENT: name = "DT_OS_RELAENT"; break;
        case DT_RELAENT: name = "DT_RELAENT"; break;
        case DT_OS_SYMTABSZ: name = "DT_OS_SYMTABSZ"; break;
        }
        std::optional<std::uint64_t> value;
        for (const auto& t : dynTags) {
            if (t.Tag != tag)
                continue;
            if (value && *value != t.Value)
                throw RelinkerException(std::string("Conflicting duplicate ") + name + " tags");
            value = t.Value;
        }
        if (value)
            return *value;
        std::ostringstream message;
        message << "Required dynamic tag " << name << " (0x" << std::hex << tag << ") not found";
        throw RelinkerException(message.str());
    };

    auto requireExactlyOneOf = [&](const std::int64_t osTag, const std::int64_t sysvTag, const char* name) {
        const bool hasOs = hasTag(osTag);
        const bool hasSysv = hasTag(sysvTag);
        if (hasOs && hasSysv)
            throw RelinkerException(std::string("Both DT_OS_ and DT_ variants present for ") + name);
        if (!hasOs && !hasSysv) {
            std::ostringstream message;
            message << "Neither DT_OS_ nor DT_ variant present for " << name << " (tags 0x" << std::hex << osTag << " and 0x" << sysvTag << ")";
            throw RelinkerException(message.str());
        }
        return hasOs;
    };

    auto readAsOffset = [&](const std::int64_t osTag, const std::int64_t sysvTag, const char* name) -> FileByteOffset {
        if (requireExactlyOneOf(osTag, sysvTag, name)) {
            const std::uint64_t value = getTagValue(osTag);
            if (value > std::numeric_limits<FileByteOffset>::max() - sceTableBase)
                throw RelinkerException(std::string("Adding PT_SCE_DYNLIBDATA base to ") + name + " overflows", value);
            return sceTableBase + value;
        }
        return elfReader->TranslateVirtualAddress(getTagValue(sysvTag));
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
    const auto symbolEntrySize = readAsSize(DT_OS_SYMENT, DT_SYMENT, "DT_SYMENT");
    if (symbolEntrySize != symEntSize)
        throw RelinkerException("Unsupported DT_SYMENT value: " + std::to_string(symbolEntrySize) + "; expected 24 bytes");

    FileByteOffset dynJmpRelOffset = 0;
    if (hasPltRelocations) {
        gotSize = readAsSize(DT_OS_PLTRELSZ, DT_PLTRELSZ, "DT_PLTRELSZ");
        const std::int64_t jmprelType = requireExactlyOneOf(DT_OS_PLTREL, DT_PLTREL, "DT_PLTREL")
            ? getTagValue(DT_OS_PLTREL)
            : getTagValue(DT_PLTREL);
        if (jmprelType != DT_RELA)
            throw RelinkerException("Unsupported DT_PLTREL type: " + std::to_string(jmprelType) + "; expected DT_RELA (7)");
        dynJmpRelOffset = readAsOffset(DT_OS_JMPREL, DT_JMPREL, "DT_JMPREL");
        if (gotSize % 24 != 0)
            throw RelinkerException("Invalid DT_PLTRELSZ value: " + std::to_string(gotSize) + " bytes; expected a multiple of 24 bytes");
    }
    const ByteCount dynJmpRelSize = gotSize;

    if (hasTag(DT_REL) || hasTag(DT_RELSZ) || hasTag(DT_RELENT))
        throw RelinkerException("DT_REL relocations are not supported");
    if (hasTag(DT_RELR) || hasTag(DT_RELRSZ) || hasTag(DT_RELRENT))
        throw RelinkerException("DT_RELR relocations are not supported");

    const bool hasRelaTable = hasTag(DT_OS_RELA) || hasTag(DT_RELA)
        || hasTag(DT_OS_RELASZ) || hasTag(DT_RELASZ);
    FileByteOffset dynRelaOffset = 0;
    ByteCount dynRelaSize = 0;
    if (hasRelaTable) {
        dynRelaOffset = readAsOffset(DT_OS_RELA, DT_RELA, "DT_RELA");
        dynRelaSize = readAsSize(DT_OS_RELASZ, DT_RELASZ, "DT_RELASZ");
    }
    constexpr std::size_t relaEntSize = 24;
    if (hasRelaTable || hasTag(DT_OS_RELAENT) || hasTag(DT_RELAENT)) {
        const auto relocationEntrySize = readAsSize(DT_OS_RELAENT, DT_RELAENT, "DT_RELAENT");
        if (relocationEntrySize != relaEntSize)
            throw RelinkerException("Unsupported DT_RELAENT value: " + std::to_string(relocationEntrySize) + "; expected 24 bytes");
    }
    if (dynRelaSize % relaEntSize != 0)
        throw RelinkerException("Invalid DT_RELASZ value: " + std::to_string(dynRelaSize) + " bytes; expected a multiple of 24 bytes");

    std::vector<std::pair<std::uint64_t, std::string>> neededLibraryNamesByStrOffset;
    for (const auto& tag : dynTags)
        if (tag.Tag == DT_NEEDED)
            neededLibraryNamesByStrOffset.emplace_back(tag.Value, std::string());

    std::vector<NidReference> nidRefs;
    std::vector<std::string> neededLibraries;

    const std::vector<std::uint8_t>& raw = elfReader->GetRawBytes();
    const std::optional<ByteCount> dynSymTabSize = hasTag(DT_OS_SYMTABSZ)
        ? std::optional<ByteCount>(getTagValue(DT_OS_SYMTABSZ)) : std::nullopt;

    if (dynStrTabOffset > raw.size() || dynStrTabSize > raw.size() - dynStrTabOffset)
        throw RelinkerException("Dynamic string table is out of bounds", dynStrTabOffset);

    if (hasPltRelocations && (dynJmpRelOffset > raw.size() || dynJmpRelSize > raw.size() - dynJmpRelOffset))
        throw RelinkerException("Jump relocation table is out of bounds", dynJmpRelOffset);

    if (dynRelaOffset > raw.size() || dynRelaSize > raw.size() - dynRelaOffset)
        throw RelinkerException("Relocation table is out of bounds", dynRelaOffset);

    if (dynRelaSize != 0 && dynJmpRelSize != 0
        && dynRelaOffset < dynJmpRelOffset + dynJmpRelSize && dynJmpRelOffset < dynRelaOffset + dynRelaSize
        && dynRelaOffset % relaEntSize != dynJmpRelOffset % relaEntSize)
        throw RelinkerException("Unaligned overlapping relocation tables", dynJmpRelOffset);

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
        if (snd.empty() || snd.find_first_of("\\:$\r\n") != std::string::npos)
            throw RelinkerException("Invalid dependency: " + snd, dynStrTabOffset + fst);
        neededLibraries.push_back(snd);
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

    auto symbolEntryPos = [&](const std::uint32_t symIdx) -> FileByteOffset {
        const ByteCount off = static_cast<ByteCount>(symIdx) * symEntSize;
        const FileByteOffset symOff = dynSymTabOffset + off;
        if (symOff < dynSymTabOffset || symOff > raw.size() || raw.size() - symOff < symEntSize
            || (dynSymTabSize && (off > *dynSymTabSize || symEntSize > *dynSymTabSize - off)))
            throw RelinkerException("Symbol table entry out of bounds", symOff);
        return symOff;
    };

    auto isZeroSectionSymbol = [&](const std::uint32_t symIdx) {
        const FileByteOffset symOff = symbolEntryPos(symIdx);
        std::uint64_t value = 0;
        std::memcpy(&value, raw.data() + symOff + 8, sizeof(value));
        return (raw[symOff + 4] & 0xf) == 3 && value == 0;
    };

    const auto isTlsRelocation = [](const std::uint32_t type) {
        return type == R_X86_64_DTPMOD64 || type == R_X86_64_DTPOFF64 || type == R_X86_64_TPOFF64;
    };

    const bool hasNonEmptyTlsSegment = std::any_of(programHeaders.begin(), programHeaders.end(),
                                                   [](const ProgramHeader& ph) { return ph.Type == PT_TLS && ph.MemorySize != 0; });

    const auto definedTlsSymbolValue = [&](const std::uint32_t symIdx, const FileByteOffset pos) -> std::optional<std::uint64_t> {
        const FileByteOffset symOff = symbolEntryPos(symIdx);
        std::uint16_t section = 0;
        std::memcpy(&section, raw.data() + symOff + 6, sizeof(section));
        if (section == SHN_UNDEF)
            return std::nullopt;
        if ((raw[symOff + 4] & 15) != STT_TLS)
            throw RelinkerException("TLS relocation names a defined symbol that is not STT_TLS", pos);
        std::uint64_t value = 0;
        std::memcpy(&value, raw.data() + symOff + 8, sizeof(value));
        return value;
    };
    const auto definedTlsSymbol = [&](const std::uint32_t symIdx, const FileByteOffset pos) {
        return definedTlsSymbolValue(symIdx, pos).has_value();
    };

    auto extractRela = [&](const FileByteOffset relaOff, const ByteCount relaSize, const bool plt) {
        for (ByteCount off = 0; relaSize >= relaEntSize && off <= relaSize - relaEntSize; off += relaEntSize) {
            const FileByteOffset pos = relaEntryPos(relaOff, off);
            if (!plt && pos >= dynJmpRelOffset && pos - dynJmpRelOffset < dynJmpRelSize)
                continue;

            std::uint64_t rOffset = 0, rInfo = 0;
            std::int64_t rAddend = 0;
            std::memcpy(&rOffset, raw.data() + pos, 8);
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);
            std::memcpy(&rAddend, raw.data() + pos + 16, 8);

            const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

            validationPolicy->ValidateRelocationTypeSupported(relType, pos);

            if (relType == R_X86_64_NONE)
                continue;

            if (plt && relType != R_X86_64_JUMP_SLOT)
                throw RelinkerException("Unsupported PLT relocation type", pos);

            if (relType == 8) {
                if (symIdx != 0 && !isZeroSectionSymbol(symIdx))
                    throw RelinkerException("RELATIVE relocation has a nonzero symbol index", pos);
                continue;
            }

            if (isTlsRelocation(relType) && (symIdx == 0 || definedTlsSymbol(symIdx, pos)))
                continue;

            const FileByteOffset symOff = symbolEntryPos(symIdx);

            std::uint32_t nameOff = 0;
            std::memcpy(&nameOff, raw.data() + symOff, 4);

            const auto name = readCStr(nameOff);
            if (name.empty() && (raw.size() - symOff < symEntSize
                                 || std::any_of(raw.begin() + static_cast<std::ptrdiff_t>(symOff), raw.begin() + static_cast<std::ptrdiff_t>(symOff + symEntSize),
                                                [](const std::uint8_t byte) { return byte != 0; })))
                throw RelinkerException("Unnamed import symbol is not a null symbol entry", symOff);
            const bool weak = (raw[symOff + 4] >> 4) == STB_WEAK;
            const auto symbolType = static_cast<std::uint8_t>(raw[symOff + 4] & 15);
            const bool function = symbolType == STT_FUNC;
            nidRefs.push_back({name, Domain::ImportModule(name, importModules, neededLibraries), relType, pos, rOffset, rAddend, weak, function, symbolType});
        }
    };

    extractRela(dynRelaOffset, dynRelaSize, false);
    extractRela(dynJmpRelOffset, dynJmpRelSize, true);

    std::vector<std::pair<VirtualAddress, VirtualAddress>> unwindRanges;
    std::vector<std::vector<CodeRange>> segmentUnwindRanges(executableSegments.size());
    if (std::any_of(programHeaders.begin(), programHeaders.end(), [](const ProgramHeader& ph) { return ph.Type == PT_SCE_DYNLIBDATA; })) {
        const auto functions = UnusedNidFilter::ReadExceptionFunctions(raw, programHeaders, {}, {});
        for (const auto& function : functions) unwindRanges.emplace_back(function.Begin, function.End);
        std::sort(unwindRanges.begin(), unwindRanges.end());
        for (std::size_t index = 0; index < executableSegments.size(); ++index) {
            auto& [segment, segmentVAddr, segmentFileOffset] = executableSegments[index];
            static_cast<void>(segmentFileOffset);
            if (const auto codeEnd = FindSceCodeEnd(segment, segmentVAddr, functions, gotVAddr, dynJmpRelSize))
                segment.resize(*codeEnd - segmentVAddr);
            segmentUnwindRanges[index] = SceUnwindRanges(segment, segmentVAddr, functions);
        }
    }

    std::vector<std::vector<std::uint8_t>> scanSegments;
    for (const auto& [segment, segmentVAddr, segmentFileOffset] : executableSegments) {
        static_cast<void>(segmentFileOffset);
        auto& scanSegment = scanSegments.emplace_back(segment);
        for (const auto& table : FindRelativeJumpTables(segment, segmentVAddr, unwindRanges))
            std::fill(scanSegment.begin() + static_cast<std::ptrdiff_t>(table.Begin - segmentVAddr), scanSegment.begin() + static_cast<std::ptrdiff_t>(table.End - segmentVAddr), std::uint8_t{0x90});
    }

    std::vector<RelinkPatch> syscallPatches;
    for (std::size_t index = 0; index < executableSegments.size(); ++index) {
        auto& [segment, segmentVAddr, segmentFileOffset] = executableSegments[index];
        auto& scanSegment = scanSegments[index];
        for (const auto offset : syscallScanner->ScanCodeSectionForSyscalls(scanSegment, segmentVAddr, scanSegment.size(), {}, segmentUnwindRanges[index])) {
            if (offset > segment.size() || segment.size() - offset < 2u)
                throw RelinkerException("Syscall trap patch exceeds executable segment", segmentVAddr + offset);
            segment[offset] = scanSegment[offset] = 0x0F;
            segment[offset + 1] = scanSegment[offset + 1] = 0x0B;
            syscallPatches.push_back({segmentFileOffset + offset, {0x0F, 0x0B}});
        }
    }


    std::vector<std::pair<ByteCount, std::string>> exportedSymbols;
    std::vector<VirtualAddress> exportedFunctions;
    if (hasTag(DT_OS_SYMTABSZ)) {
        const auto symbolBytes = getTagValue(DT_OS_SYMTABSZ);
        if (symbolBytes % symEntSize != 0 || dynSymTabOffset > raw.size() || symbolBytes > raw.size() - dynSymTabOffset)
            throw RelinkerException("Invalid executable dynamic symbol table size");
        for (ByteCount off = symEntSize; off < symbolBytes; off += symEntSize) {
            const auto* symbol = raw.data() + dynSymTabOffset + off;
            std::uint16_t section = 0;
            std::memcpy(&section, symbol + 6, sizeof(section));
            if (section == 0 || (symbol[4] >> 4) == 0 || (symbol[5] & 3) == 1 || (symbol[5] & 3) == 2) continue;
            std::uint32_t sourceName = 0;
            std::memcpy(&sourceName, symbol, sizeof(sourceName));
            auto name = readCStr(sourceName);
            name.resize(name.find('#') == std::string::npos ? name.size() : name.find('#'));
            const FileByteOffset entryOffset = dynSymTabOffset + off;
            const auto type = symbol[4] & 15;
            std::uint64_t value = 0;
            std::uint64_t size = 0;
            std::memcpy(&value, symbol + 8, sizeof(value));
            std::memcpy(&size, symbol + 16, sizeof(size));
            if (type == STT_TLS) {
                if (section >= SHN_LORESERVE)
                    throw RelinkerException("Unsupported executable TLS export section: " + name, entryOffset);
                const auto tls = std::find_if(programHeaders.begin(), programHeaders.end(),
                                              [](const ProgramHeader& ph) { return ph.Type == PT_TLS; });
                if (tls == programHeaders.end())
                    throw RelinkerException("Executable TLS export has no PT_TLS segment: " + name, entryOffset);
                if (value > tls->MemorySize || size > tls->MemorySize - value)
                    throw RelinkerException("Executable TLS export exceeds PT_TLS: " + name, entryOffset);
            } else if (section != SHN_ABS) {
                if (section >= SHN_LORESERVE)
                    throw RelinkerException("Unsupported executable export section: " + name, entryOffset);
                const std::uint32_t flags = type == STT_FUNC ? PF_X : 0;
                const auto extent = std::max<std::uint64_t>(size, 1);
                const bool mapped = std::any_of(programHeaders.begin(), programHeaders.end(), [&](const ProgramHeader& ph) {
                    return ph.Type == PT_LOAD && (ph.Flags & flags) == flags && value >= ph.MappedAddress &&
                           value - ph.MappedAddress < ph.MemorySize && extent <= ph.MemorySize - (value - ph.MappedAddress);
                });
                if (!mapped) {
                    std::ostringstream message;
                    message << "Executable export " << name << " at 0x" << std::hex << value
                            << (size > 1 ? " (" + std::to_string(size) + " bytes) does not fit inside " : " is not inside ")
                            << (flags != 0 ? "an executable" : "a") << " PT_LOAD segment";
                    throw RelinkerException(message.str(), entryOffset);
                }
            }
            exportedSymbols.emplace_back(off, std::move(name));
            const auto binding = symbol[4] >> 4;
            if ((binding == STB_GLOBAL || binding == STB_WEAK) && type == STT_FUNC)
                exportedFunctions.push_back(value);
        }
    }

    progress.Report({Domain::ProgressStage::Imports, {}, 0, 1});
    const std::size_t originalNidCount = nidRefs.size();
    const auto originalNidRefs = nidRefs;
    { std::ostringstream message; message << "NID input: " << originalNidCount << " references\n"; progress.Message(message.str()); }

    if (unusedFilterLevel == 2) {
        if (executableSegments.size() > 1)
            throw RelinkerException("Strict NID filtering does not support multiple executable segments");
        nidRefs = unusedNidFilter->Filter(nidRefs, raw, textSection, textVAddr);
        if (nidRefs.size() > originalNidCount) throw RelinkerException("Strict NID filter increased the reference count");
        { std::ostringstream message; message << "Strict filtering total: " << originalNidCount << " -> " << nidRefs.size() << "; filtered=" << originalNidCount - nidRefs.size() << "\n"; progress.Message(message.str()); }
    } else if (unusedFilterLevel == 1) {
        std::vector<NidReference> pltRefs;
        std::vector<NidReference> nonPltRefs;
        for (const auto& ref : nidRefs) {
            if (ref.RelocationTypeValue == R_X86_64_JUMP_SLOT)
                pltRefs.push_back(ref);
            else
                nonPltRefs.push_back(ref);
        }

        { std::ostringstream message; message << "PLT preservation: " << pltRefs.size() << " -> " << pltRefs.size() << "; filtered=0\n"; progress.Message(message.str()); }
        const std::size_t nonPltCount = nonPltRefs.size();
        if (executableSegments.size() > 1) {
            { std::ostringstream message; message << "CFG/GOT filtering skipped: multiple executable segments are not modeled; "
                      << nonPltCount << " -> " << nonPltCount << "; filtered=0\n"; progress.Message(message.str()); }
        } else {
            nonPltRefs = unusedNidFilter->Filter(nonPltRefs, raw, textSection, textVAddr, exportedFunctions);
            if (nonPltRefs.size() > nonPltCount) throw RelinkerException("Unused NID filter increased the reference count");
            { std::ostringstream message; message << "CFG/GOT filtering: " << nonPltCount << " -> " << nonPltRefs.size() << "; filtered=" << nonPltCount - nonPltRefs.size() << "\n"; progress.Message(message.str()); }
        }

        nidRefs.clear();
        nidRefs.reserve(pltRefs.size() + nonPltRefs.size());
        for (auto& ref : pltRefs) nidRefs.push_back(std::move(ref));
        for (auto& ref : nonPltRefs) nidRefs.push_back(std::move(ref));
    } else {
        { std::ostringstream message; message << "Unused NID filtering: disabled; filtered=0\n"; progress.Message(message.str()); }
    }

    { std::ostringstream message; message << "NID total: " << originalNidCount << " -> " << nidRefs.size() << "; filtered=" << originalNidCount - nidRefs.size() << "\n"; progress.Message(message.str()); }

    auto dynamicRefs = nidRefs;
    std::vector<RelinkPatch> patches;
    auto pltCount = static_cast<std::uint32_t>(dynJmpRelSize / relaEntSize);
    if (unusedFilterLevel == 2) {
        auto compacted = UnusedNidFilter::CompactPlt(originalNidRefs, nidRefs, textSection, textVAddr, elfReader->TranslateVirtualAddress(textVAddr), dynJmpRelOffset);
        dynamicRefs = std::move(compacted.References);
        patches = std::move(compacted.Patches);
        { std::ostringstream message; message << "PLT compaction: " << pltCount << " -> " << compacted.SlotCount << "\n"; progress.Message(message.str()); }
        pltCount = compacted.SlotCount;
    }
    auto dynSection = dynamicSectionBuilder->BuildDynamicSection(dynamicRefs, neededLibraries, dynJmpRelOffset, pltCount);
    for (const auto& ref : dynamicRefs) {
        if (!ref.Library.empty()) continue;
        if (auto declared = Domain::DeclaredImportModule(ref.Nid, importModules); !declared.empty()) dynSection.UnmatchedImportModules.insert(std::move(declared));
    }

    for (const auto& [off, name] : exportedSymbols) {
        const auto* symbol = raw.data() + dynSymTabOffset + off;
        const auto nameOffset = static_cast<std::uint32_t>(dynSection.DynStrData.size());
        dynSection.DynStrData.insert(dynSection.DynStrData.end(), name.begin(), name.end());
        dynSection.DynStrData.push_back(0);
        const auto position = dynSection.DynSymData.size();
        dynSection.DynSymData.insert(dynSection.DynSymData.end(), symbol, symbol + symEntSize);
        std::memcpy(dynSection.DynSymData.data() + position, &nameOffset, sizeof(nameOffset));
    }

    auto appendRela = [&](std::vector<std::uint8_t>& buf, std::uint64_t offset, std::uint64_t info, std::int64_t addend) {
        std::size_t pos = buf.size();
        buf.resize(pos + 24);
        std::memcpy(buf.data() + pos, &offset, 8);
        std::memcpy(buf.data() + pos + 8, &info, 8);
        std::memcpy(buf.data() + pos + 16, &addend, 8);
    };

    auto extractLocal = [&](const FileByteOffset relaOff, const ByteCount relaSize, const bool plt) {
        for (ByteCount off = 0; relaSize >= relaEntSize && off <= relaSize - relaEntSize; off += relaEntSize) {
            const FileByteOffset pos = relaEntryPos(relaOff, off);
            if (!plt && pos >= dynJmpRelOffset && pos - dynJmpRelOffset < dynJmpRelSize)
                continue;
            std::uint64_t rOffset = 0, rInfo = 0;
            std::int64_t rAddend = 0;
            std::memcpy(&rOffset, raw.data() + pos, 8);
            std::memcpy(&rInfo, raw.data() + pos + 8, 8);
            std::memcpy(&rAddend, raw.data() + pos + 16, 8);
            const std::uint32_t symIdx = static_cast<std::uint32_t>(rInfo >> 32);
            const std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);
            if (relType == R_X86_64_RELATIVE) {
                appendRela(dynSection.RelaData, rOffset, static_cast<std::uint64_t>(R_X86_64_RELATIVE), rAddend);
                continue;
            }
            if (!isTlsRelocation(relType))
                continue;
            std::uint64_t offset = 0;
            if (symIdx != 0) {
                const auto value = definedTlsSymbolValue(symIdx, pos);
                if (!value)
                    continue;
                offset = *value;
            }
            if (!hasNonEmptyTlsSegment)
                throw RelinkerException("TLS relocation in an executable without a non-empty PT_TLS segment", pos);
            if (relType == R_X86_64_DTPMOD64 && rAddend != 0)
                throw RelinkerException("DTPMOD64 relocation has a nonzero addend", pos);
            const auto addend = relType == R_X86_64_DTPMOD64 ? 0 : static_cast<std::int64_t>(offset + static_cast<std::uint64_t>(rAddend));
            appendRela(dynSection.RelaData, rOffset, static_cast<std::uint64_t>(relType), addend);
        }
    };

    extractLocal(dynRelaOffset, dynRelaSize, false);
    extractLocal(dynJmpRelOffset, dynJmpRelSize, true);

    std::vector<CallRegistryEntry> entries;
    entries.reserve(nidRefs.size());
    for (const auto& ref : nidRefs) {
        if (ref.Nid.empty()) continue;
        CallRegistryEntry entry;
        entry.Nid = ref.Nid;
        entry.Library = ref.Library;
        entry.RelocationTypeString = relocationTypeName(ref.RelocationTypeValue);
        entry.RelocationOffset = ref.RelocationTableOffset;
        entry.TargetSection = ".got";
        entry.TargetOffset = ref.RelocationAddress;
        entry.CallSitesResolved = false;
        entry.Weak = ref.Weak;
        entries.push_back(std::move(entry));
    }

    for (std::size_t index = 0; index < executableSegments.size(); ++index) {
        const auto& segmentVAddr = std::get<1>(executableSegments[index]);
        for (auto& entry : entries) {
            auto segmentSites = callSiteResolver->ResolveCallSites(scanSegments[index], segmentVAddr, entry.TargetOffset, 8, segmentUnwindRanges[index]);
            entry.CallSites.insert(entry.CallSites.end(), segmentSites.begin(), segmentSites.end());
            entry.CallSitesResolved = !entry.CallSites.empty();
        }
    }

    patches.insert(patches.end(), std::make_move_iterator(syscallPatches.begin()), std::make_move_iterator(syscallPatches.end()));
    progress.Report({Domain::ProgressStage::Imports, {}, 1, 1});
    return RelinkResult{std::move(entries), std::move(programHeaders), std::move(dynSection), gotVAddr, std::move(patches)};
}

}
