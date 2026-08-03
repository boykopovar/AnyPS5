#ifndef ELFPATCHER_ELFPATCHER_HPP
#define ELFPATCHER_ELFPATCHER_HPP

#include <elfpatcher/IElfPatcher.hpp>

namespace Elfpatcher {

class ElfPatcher : public IElfPatcher {
public:
    std::vector<std::uint8_t> Patch(
        const std::vector<std::uint8_t>& sourceElf,
        const std::vector<Domain::ProgramHeader>& originalHeaders,
        const Domain::SysVDynamicSection& dynamicSection
    ) override;

private:
    static constexpr std::uint32_t PT_LOAD = 1;
    static constexpr std::uint32_t PT_DYNAMIC = 2;
    static constexpr std::uint32_t PT_INTERP = 3;
    static constexpr std::uint32_t PT_NOTE = 4;
    static constexpr std::uint32_t PT_GNU_RELRO = 0x6474e552;
    static constexpr std::uint32_t PF_X = 0x1;
    static constexpr std::uint32_t PF_W = 0x2;
    static constexpr std::uint32_t PF_R = 0x4;
    static constexpr std::uint32_t ET_DYN = 3;
    static constexpr std::uint32_t PT_SCE_DYNLIBDATA = 0x61000000;
    static constexpr std::uint32_t PT_OS_PROCPARAM = 0x61000001;
    static constexpr std::uint32_t PT_OS_RELRO = 0x61000010;

    static constexpr std::int64_t DT_NULL = 0;
    static constexpr std::int64_t DT_NEEDED = 1;
    static constexpr std::int64_t DT_STRTAB = 5;
    static constexpr std::int64_t DT_SYMTAB = 6;
    static constexpr std::int64_t DT_RELA = 7;
    static constexpr std::int64_t DT_RELASZ = 8;
    static constexpr std::int64_t DT_RELAENT = 9;
    static constexpr std::int64_t DT_STRSZ = 10;
    static constexpr std::int64_t DT_SYMENT = 11;
    static constexpr std::int64_t DT_JMPREL = 23;
    static constexpr std::int64_t DT_PLTRELSZ = 2;
    static constexpr std::int64_t DT_PLTREL = 20;

    [[nodiscard]] bool _isSceSpecificSegment(std::uint32_t type) const;
    [[nodiscard]] bool _isNullPageLoad(const Domain::ProgramHeader& ph) const;
    [[nodiscard]] bool _shouldSkip(const Domain::ProgramHeader& ph) const;
    [[nodiscard]] Domain::ProgramHeader _makeLoadHeader(std::uint64_t offset, std::uint64_t size) const;
    [[nodiscard]] std::vector<std::uint8_t> _buildEntryStub(std::uint64_t stubVaddr, std::uint64_t realEntryVaddr) const;
    void _writeInterp(std::vector<std::uint8_t>& buf, std::size_t phEntOff, std::uint64_t interpOff, std::uint64_t interpSize) const;
    void _writeU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const;
    void _writeU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) const;
    void _writeU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) const;
    void _appendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const;
    void _appendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const;
    void _appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const;
    void _writeProgramHeader(std::vector<std::uint8_t>& buf, std::size_t offset, const Domain::ProgramHeader& ph) const;
    [[nodiscard]] std::uint32_t _fixLoadFlags(std::uint32_t originalFlags) const;
};

}

#endif
