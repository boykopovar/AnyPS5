#ifndef RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP
#define RELINKER_OUTPUT_SYSVDYNAMICSECTIONBUILDER_HPP

#include <relinker/domain/ISysVDynamicSectionBuilder.hpp>

namespace Relinker {

class SysVDynamicSectionBuilder : public ISysVDynamicSectionBuilder {
public:
    SysVDynamicSection BuildDynamicSection(
        const std::vector<NidReference>& nidReferences,
        const std::vector<std::string>& neededLibraries
    ) override;

private:
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
