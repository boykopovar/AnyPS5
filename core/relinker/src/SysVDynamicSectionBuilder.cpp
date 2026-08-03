#include <relinker/ISysVDynamicSectionBuilder.hpp>
#include <memory>

namespace Relinker {

static constexpr std::uint32_t STB_GLOBAL = 1;
static constexpr std::uint32_t STT_FUNC = 2;
static constexpr std::uint32_t STV_DEFAULT = 0;
static constexpr std::uint32_t R_X86_64_JUMP_SLOT = 7;
static constexpr std::uint32_t R_X86_64_GLOB_DAT = 6;

static constexpr std::int64_t DT_NEEDED = 1;
static constexpr std::int64_t DT_SYMTAB = 6;
static constexpr std::int64_t DT_STRTAB = 5;
static constexpr std::int64_t DT_STRSZ = 10;
static constexpr std::int64_t DT_RELA = 7;
static constexpr std::int64_t DT_RELASZ = 8;
static constexpr std::int64_t DT_RELAENT = 9;
static constexpr std::int64_t DT_JMPREL = 23;
static constexpr std::int64_t DT_PLTRELSZ = 2;
static constexpr std::int64_t DT_PLTREL = 20;
static constexpr std::int64_t DT_SYMENT = 11;
static constexpr std::int64_t DT_NULL = 0;

static void _appendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) {
    for (int i = 0; i < 8; ++i)
        buf.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
}

static void _appendI64(std::vector<std::uint8_t>& buf, std::int64_t v) {
    _appendU64(buf, static_cast<std::uint64_t>(v));
}

static void _appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) {
    _appendI64(buf, tag);
    _appendU64(buf, val);
}

static std::uint32_t _appendStr(std::vector<std::uint8_t>& strtab, const std::string& s) {
    auto offset = static_cast<std::uint32_t>(strtab.size());
    for (char c : s) strtab.push_back(static_cast<std::uint8_t>(c));
    strtab.push_back(0);
    return offset;
}

static void _appendElfSym(
    std::vector<std::uint8_t>& dynsym,
    std::uint32_t nameOff,
    std::uint8_t info,
    std::uint8_t other,
    std::uint16_t shndx,
    std::uint64_t value,
    std::uint64_t size)
{
    _appendU64(dynsym, nameOff);
    dynsym.push_back(info);
    dynsym.push_back(other);
    dynsym.push_back(shndx & 0xFF);
    dynsym.push_back((shndx >> 8) & 0xFF);
    _appendU64(dynsym, value);
    _appendU64(dynsym, size);
}

static void _appendRela(
    std::vector<std::uint8_t>& rela,
    const std::uint64_t offset,
    const std::uint64_t info,
    std::int64_t addend)
{
    _appendU64(rela, offset);
    _appendU64(rela, info);
    _appendI64(rela, addend);
}

class SysVDynamicSectionBuilder : public ISysVDynamicSectionBuilder {
public:
    SysVDynamicSection BuildDynamicSection(
        const std::vector<NidReference>& nidReferences,
        const std::vector<std::string>& neededLibraries) override;
};

SysVDynamicSection SysVDynamicSectionBuilder::BuildDynamicSection(
    const std::vector<NidReference>& nidReferences,
    const std::vector<std::string>& neededLibraries)
{
    SysVDynamicSection result;

    result.DynStrData.push_back(0);

    std::vector<std::uint32_t> neededOffsets;
    for (const auto& lib : neededLibraries)
        neededOffsets.push_back(_appendStr(result.DynStrData, lib));

    _appendElfSym(result.DynSymData, 0, 0, 0, 0, 0, 0);

    std::uint32_t symIdx = 1;
    for (const auto& ref : nidReferences) {
        const std::uint32_t nameOff = _appendStr(result.DynStrData, ref.Nid);
        const auto info = static_cast<std::uint8_t>((STB_GLOBAL << 4) | STT_FUNC);
        _appendElfSym(result.DynSymData, nameOff, info, STV_DEFAULT, 0, 0, 0);

        std::uint32_t relType = ref.RelocationTypeValue;
        if (relType == 0) relType = R_X86_64_JUMP_SLOT;

        const std::uint64_t relaInfo = (static_cast<std::uint64_t>(symIdx) << 32) | relType;

        if (relType == R_X86_64_JUMP_SLOT)
            _appendRela(result.RelaPltData, ref.RelocationAddress, relaInfo, 0);
        else
            _appendRela(result.RelaData, ref.RelocationAddress, relaInfo, 0);

        ++symIdx;
    }

    std::uint64_t strsz = result.DynStrData.size();

    for (const std::uint32_t off : neededOffsets)
        _appendDynEntry(result.DynamicSegmentData, DT_NEEDED, off);

    _appendDynEntry(result.DynamicSegmentData, DT_STRTAB, 0);
    _appendDynEntry(result.DynamicSegmentData, DT_STRSZ, strsz);
    _appendDynEntry(result.DynamicSegmentData, DT_SYMTAB, 0);
    _appendDynEntry(result.DynamicSegmentData, DT_SYMENT, 24);
    _appendDynEntry(result.DynamicSegmentData, DT_RELA, 0);
    _appendDynEntry(result.DynamicSegmentData, DT_RELASZ, result.RelaData.size());
    _appendDynEntry(result.DynamicSegmentData, DT_RELAENT, 24);
    _appendDynEntry(result.DynamicSegmentData, DT_JMPREL, 0);
    _appendDynEntry(result.DynamicSegmentData, DT_PLTRELSZ, result.RelaPltData.size());
    _appendDynEntry(result.DynamicSegmentData, DT_PLTREL, static_cast<std::uint64_t>(DT_RELA));
    _appendDynEntry(result.DynamicSegmentData, DT_NULL, 0);

    return result;
}

std::shared_ptr<ISysVDynamicSectionBuilder> MakeSysVDynamicSectionBuilder() {
    return std::make_shared<SysVDynamicSectionBuilder>();
}

}
