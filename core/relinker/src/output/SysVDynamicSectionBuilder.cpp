#include <relinker/output/SysVDynamicSectionBuilder.hpp>
#include <cstring>

namespace Relinker {

void SysVDynamicSectionBuilder::_appendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const {
    std::size_t pos = buf.size();
    buf.resize(pos + 8);
    std::memcpy(buf.data() + pos, &v, 8);
}

void SysVDynamicSectionBuilder::_appendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const {
    _appendU64(buf, static_cast<std::uint64_t>(v));
}

void SysVDynamicSectionBuilder::_appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const {
    _appendI64(buf, tag);
    _appendU64(buf, val);
}

std::uint32_t SysVDynamicSectionBuilder::_appendStr(std::vector<std::uint8_t>& strtab, const std::string& s) const {
    auto offset = static_cast<std::uint32_t>(strtab.size());
    for (char c : s)
        strtab.push_back(static_cast<std::uint8_t>(c));
    strtab.push_back(0);
    return offset;
}

void SysVDynamicSectionBuilder::_appendElfSym(
    std::vector<std::uint8_t>& dynsym,
    std::uint32_t nameOff,
    std::uint8_t info,
    std::uint8_t other,
    std::uint16_t shndx,
    std::uint64_t value,
    std::uint64_t size) const
{
    _appendU64(dynsym, nameOff);
    dynsym.push_back(info);
    dynsym.push_back(other);
    dynsym.push_back(shndx & 0xFF);
    dynsym.push_back((shndx >> 8) & 0xFF);
    _appendU64(dynsym, value);
    _appendU64(dynsym, size);
}

void SysVDynamicSectionBuilder::_appendRela(
    std::vector<std::uint8_t>& rela,
    std::uint64_t offset,
    std::uint64_t info,
    std::int64_t addend) const
{
    _appendU64(rela, offset);
    _appendU64(rela, info);
    _appendI64(rela, addend);
}

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

    for (const std::uint32_t off : neededOffsets)
        _appendDynEntry(result.DynamicSegmentData, DT_NEEDED, off);

    return result;
}

}
