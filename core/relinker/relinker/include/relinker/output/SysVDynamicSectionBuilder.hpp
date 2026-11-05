#ifndef RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP

#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>

namespace Relinker {

class SysVDynamicSectionBuilder : public ISysVDynamicSectionBuilder {
public:
    SysVDynamicSection BuildDynamicSection(
        const std::vector<NidReference>& nidReferences,
        const std::vector<std::string>& neededLibraries,
        FileByteOffset originalJmprelOffset,
        std::uint32_t originalJmprelCount
    ) override;

private:
    void _appendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const;
    void _appendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const;
    void _appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const;
    std::uint32_t _appendStr(std::vector<std::uint8_t>& strtab, const std::string& s) const;
    void _appendElfSym(
        std::vector<std::uint8_t>& dynsym,
        std::uint32_t nameOff,
        std::uint8_t info,
        std::uint8_t other,
        std::uint16_t shndx,
        std::uint64_t value,
        std::uint64_t size
    ) const;
    void _appendRela(
        std::vector<std::uint8_t>& rela,
        std::uint64_t offset,
        std::uint64_t info,
        std::int64_t addend
    ) const;
};

}

#endif
