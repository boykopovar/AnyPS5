#include <elfpatcher/ElfPatcher.hpp>
#include <cstring>

namespace Elfpatcher {

bool ElfPatcher::_isSceSpecificSegment(std::uint32_t type) const {
    return type == PT_SCE_DYNLIBDATA
        || type == PT_OS_PROCPARAM
        || type == PT_OS_RELRO
        || (type >= 0x61000000 && type <= 0x6fffffff);
}

bool ElfPatcher::_isNullPageLoad(const Domain::ProgramHeader& ph) const {
    return ph.Type == PT_LOAD && ph.MappedAddress == 0;
}

void ElfPatcher::_writeU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const {
    std::memcpy(buf.data() + offset, &v, 2);
}

void ElfPatcher::_writeU32(std::vector<std::uint8_t>& buf, const std::size_t offset, std::uint32_t v) const {
    std::memcpy(buf.data() + offset, &v, 4);
}

void ElfPatcher::_writeU64(std::vector<std::uint8_t>& buf, const std::size_t offset, std::uint64_t v) const {
    std::memcpy(buf.data() + offset, &v, 8);
}

void ElfPatcher::_appendU64(std::vector<std::uint8_t>& buf, std::uint64_t v) const {
    std::size_t pos = buf.size();
    buf.resize(pos + 8);
    std::memcpy(buf.data() + pos, &v, 8);
}

void ElfPatcher::_appendI64(std::vector<std::uint8_t>& buf, std::int64_t v) const {
    _appendU64(buf, static_cast<std::uint64_t>(v));
}

void ElfPatcher::_appendDynEntry(std::vector<std::uint8_t>& buf, std::int64_t tag, std::uint64_t val) const {
    _appendI64(buf, tag);
    _appendU64(buf, val);
}

void ElfPatcher::_writeProgramHeader(std::vector<std::uint8_t>& buf, std::size_t offset, const Domain::ProgramHeader& ph) const {
    _writeU32(buf, offset + 0, ph.Type);
    _writeU32(buf, offset + 4, ph.Flags);
    _writeU64(buf, offset + 8, ph.Offset);
    _writeU64(buf, offset + 16, ph.MappedAddress);
    _writeU64(buf, offset + 24, ph.PhysicalAddress);
    _writeU64(buf, offset + 32, ph.FileSize);
    _writeU64(buf, offset + 40, ph.MemorySize);
    _writeU64(buf, offset + 48, ph.Alignment);
}

Domain::ProgramHeader ElfPatcher::_makeLoadHeader(std::uint64_t offset, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_LOAD;
    ph.Flags = PF_R | PF_W;
    ph.Offset = offset;
    ph.MappedAddress = offset;
    ph.PhysicalAddress = offset;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = 0x1000;
    return ph;
}

Domain::ProgramHeader ElfPatcher::_makeHeaderBlockLoad(std::uint64_t vaddr, std::uint64_t size, std::uint64_t align) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_LOAD;
    ph.Flags = PF_R;
    ph.Offset = 0;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = align;
    return ph;
}

Domain::ProgramHeader ElfPatcher::_makePhdrHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_PHDR;
    ph.Flags = PF_R;
    ph.Offset = offset;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = 8;
    return ph;
}

std::vector<std::uint8_t> ElfPatcher::_buildEntryStub(const std::uint64_t stubVaddr, const std::uint64_t realEntryVaddr) const {
    std::vector<std::uint8_t> s;
    s.push_back(0x58);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0xe3);
    s.push_back(0x48); s.push_back(0x83); s.push_back(0xec); s.push_back(0x30);
    s.push_back(0x48); s.push_back(0x83); s.push_back(0xe4); s.push_back(0xf0);
    s.push_back(0x89); s.push_back(0x04); s.push_back(0x24);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0x5c); s.push_back(0x24); s.push_back(0x08);
    s.push_back(0x48); s.push_back(0x89); s.push_back(0xe7);
    s.push_back(0x48); s.push_back(0x31); s.push_back(0xf6);
    const std::uint64_t jmpInsnVaddr = stubVaddr + s.size();
    const std::uint64_t jmpNextVaddr = jmpInsnVaddr + 5;
    const std::int32_t rel32 = static_cast<std::int32_t>(realEntryVaddr - jmpNextVaddr);
    s.push_back(0xe9);
    s.push_back(static_cast<std::uint8_t>(rel32 & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 8) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 16) & 0xff));
    s.push_back(static_cast<std::uint8_t>((rel32 >> 24) & 0xff));
    return s;
}

std::uint32_t ElfPatcher::_fixLoadFlags(std::uint32_t originalFlags) const {
    return originalFlags | PF_R;
}

void ElfPatcher::_writeSectionHeader(
    std::vector<std::uint8_t>& buf,
    const std::size_t offset,
    const std::uint32_t nameOff,
    const std::uint32_t type,
    const std::uint64_t flags,
    const std::uint64_t addr,
    const std::uint64_t fileOffset,
    const std::uint64_t size,
    const std::uint32_t link,
    const std::uint32_t info,
    const std::uint64_t align,
    const std::uint64_t entSize
) const {
    _writeU32(buf, offset + 0, nameOff);
    _writeU32(buf, offset + 4, type);
    _writeU64(buf, offset + 8, flags);
    _writeU64(buf, offset + 16, addr);
    _writeU64(buf, offset + 24, fileOffset);
    _writeU64(buf, offset + 32, size);
    _writeU32(buf, offset + 40, link);
    _writeU32(buf, offset + 44, info);
    _writeU64(buf, offset + 48, align);
    _writeU64(buf, offset + 56, entSize);
}

bool ElfPatcher::_shouldSkip(const Domain::ProgramHeader& ph) const {
    if (_isSceSpecificSegment(ph.Type)) return true;
    if (ph.Type == PT_DYNAMIC) return true;
    if (_isNullPageLoad(ph)) return true;
    if (ph.Type == PT_NOTE && ph.MappedAddress == 0 && ph.FileSize > 0) return true;
    return false;
}

std::vector<std::uint8_t> ElfPatcher::Patch(
    const std::vector<std::uint8_t>& sourceElf,
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    const Domain::SysVDynamicSection& dynSection)
{
    std::vector<std::uint8_t> buf = sourceElf;

    buf[7] = 0;
    buf[8] = 0;
    buf[0x10] = static_cast<std::uint8_t>(ET_DYN & 0xFF);
    buf[0x11] = static_cast<std::uint8_t>((ET_DYN >> 8) & 0xFF);
    _writeU64(buf, 40, 0);
    _writeU16(buf, 60, 0);
    _writeU16(buf, 62, 0);

    const std::uint64_t phOff = *reinterpret_cast<const std::uint64_t*>(buf.data() + 32);
    const std::uint16_t phEntSize = *reinterpret_cast<const std::uint16_t*>(buf.data() + 54);
    const std::uint16_t phNum = *reinterpret_cast<const std::uint16_t*>(buf.data() + 56);

    const auto alignBuf = [](std::vector<std::uint8_t>& b, std::size_t alignment) {
        while (b.size() % alignment != 0)
            b.push_back(0);
    };

    const std::uint64_t dynStrOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynStrData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const std::uint64_t dynSymOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynSymData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const std::uint64_t relaOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaData)
        buf.push_back(b);
    alignBuf(buf, 24);

    const std::uint64_t relaPltOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaPltData)
        buf.push_back(b);
    alignBuf(buf, 8);

    std::vector<std::uint8_t> dynSegBuf;
    for (std::uint8_t b : dynSection.DynamicSegmentData)
        dynSegBuf.push_back(b);
    _appendDynEntry(dynSegBuf, DT_STRTAB, dynStrOff);
    _appendDynEntry(dynSegBuf, DT_STRSZ, dynSection.DynStrData.size());
    _appendDynEntry(dynSegBuf, DT_SYMTAB, dynSymOff);
    _appendDynEntry(dynSegBuf, DT_SYMENT, 24);
    if (!dynSection.RelaData.empty()) {
        _appendDynEntry(dynSegBuf, DT_RELA, relaOff);
        _appendDynEntry(dynSegBuf, DT_RELASZ, dynSection.RelaData.size());
        _appendDynEntry(dynSegBuf, DT_RELAENT, 24);
    }
    if (!dynSection.RelaPltData.empty()) {
        _appendDynEntry(dynSegBuf, DT_JMPREL, relaPltOff);
        _appendDynEntry(dynSegBuf, DT_PLTRELSZ, dynSection.RelaPltData.size());
        _appendDynEntry(dynSegBuf, DT_PLTREL, static_cast<std::uint64_t>(DT_RELA));
    }
    _appendDynEntry(dynSegBuf, DT_NULL, 0);

    const std::uint64_t dynSegOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSegBuf)
        buf.push_back(b);

    const std::uint64_t realEntryVaddr = *reinterpret_cast<const std::uint64_t*>(buf.data() + 24);
    const std::uint64_t stubOff = static_cast<std::uint64_t>(buf.size());
    const auto stubBytes = _buildEntryStub(stubOff, realEntryVaddr);
    for (std::uint8_t b : stubBytes)
        buf.push_back(b);
    _writeU64(buf, 24, stubOff);

    static constexpr char kInterp[] = "/lib64/ld-linux-x86-64.so.2";
    const std::uint64_t interpOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t interpSize = sizeof(kInterp);
    for (std::size_t i = 0; i < interpSize; i++)
        buf.push_back(static_cast<std::uint8_t>(kInterp[i]));

    std::uint16_t keptCount = 0;
    std::uint64_t keptMinVaddr = UINT64_MAX;
    std::uint64_t keptMinVaddrAlign = 0x1000;
    for (const auto& ph : originalHeaders) {
        if (_shouldSkip(ph))
            continue;
        keptCount++;
        if (ph.Type == PT_LOAD && ph.MappedAddress < keptMinVaddr) {
            keptMinVaddr = ph.MappedAddress;
            keptMinVaddrAlign = ph.Alignment;
        }
    }
    if (keptMinVaddr == UINT64_MAX)
        throw Domain::RelinkerException("No kept PT_LOAD segment to anchor the header block against");
    if (keptMinVaddrAlign == 0)
        throw Domain::RelinkerException("Lowest kept PT_LOAD has zero alignment");

    const std::uint16_t neededPh = keptCount + 4;
    if (neededPh > phNum)
        throw Domain::RelinkerException(
            "Not enough program header slots: need " + std::to_string(neededPh) +
            ", available " + std::to_string(phNum));

    const std::uint64_t headerBlockSize = phOff + static_cast<std::uint64_t>(neededPh) * phEntSize;
    if (headerBlockSize >= keptMinVaddr)
        throw Domain::RelinkerException(
            "Header block does not fit below the lowest kept PT_LOAD: need " +
            std::to_string(headerBlockSize) + " bytes, only " + std::to_string(keptMinVaddr) + " available");

    const std::uint64_t headerBlockVaddr = (keptMinVaddr - headerBlockSize) & ~(keptMinVaddrAlign - 1);
    if (headerBlockVaddr + headerBlockSize > keptMinVaddr)
        throw Domain::RelinkerException("Header block would overlap the lowest kept PT_LOAD after alignment");

    const std::uint64_t extraBlockOff = dynStrOff;
    const std::uint64_t extraBlockSize = static_cast<std::uint64_t>(buf.size()) - extraBlockOff;

    std::uint16_t writtenPh = 0;

    const std::size_t phdrEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
    _writeProgramHeader(buf, phdrEntOff, _makePhdrHeader(phOff, headerBlockVaddr + phOff, static_cast<std::uint64_t>(neededPh) * phEntSize));
    writtenPh++;

    const std::size_t headerLoadEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
    _writeProgramHeader(buf, headerLoadEntOff, _makeHeaderBlockLoad(headerBlockVaddr, headerBlockSize, keptMinVaddrAlign));
    writtenPh++;

    for (const auto& ph : originalHeaders) {
        if (_shouldSkip(ph))
            continue;
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        if (ph.Type == PT_LOAD) {
            Domain::ProgramHeader fixed = ph;
            fixed.Flags = _fixLoadFlags(ph.Flags);
            _writeProgramHeader(buf, phEntOff, fixed);
        } else {
            _writeProgramHeader(buf, phEntOff, ph);
        }
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        _writeProgramHeader(buf, phEntOff, _makeLoadHeader(extraBlockOff, extraBlockSize));
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        Domain::ProgramHeader dynPh{};
        dynPh.Type = PT_DYNAMIC;
        dynPh.Flags = PF_R | PF_W;
        dynPh.Offset = dynSegOff;
        dynPh.MappedAddress = dynSegOff;
        dynPh.PhysicalAddress = dynSegOff;
        dynPh.FileSize = dynSegBuf.size();
        dynPh.MemorySize = dynSegBuf.size();
        dynPh.Alignment = 8;
        _writeProgramHeader(buf, phEntOff, dynPh);
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        Domain::ProgramHeader interpPh{};
        interpPh.Type = PT_INTERP;
        interpPh.Flags = PF_R;
        interpPh.Offset = interpOff;
        interpPh.MappedAddress = interpOff;
        interpPh.PhysicalAddress = interpOff;
        interpPh.FileSize = interpSize;
        interpPh.MemorySize = interpSize;
        interpPh.Alignment = 1;
        _writeProgramHeader(buf, phEntOff, interpPh);
        writtenPh++;
    }

    _writeU16(buf, 56, writtenPh);

    static constexpr char kShStrTab[] = "\0.shstrtab\0.dynstr\0.dynsym\0.dynamic\0.text";
    constexpr std::uint32_t nameShStrTab = 1;
    constexpr std::uint32_t nameDynStr = 11;
    constexpr std::uint32_t nameDynSym = 19;
    constexpr std::uint32_t nameDynamic = 27;
    constexpr std::uint32_t nameText = 36;
    const std::uint64_t shStrTabOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t shStrTabSize = sizeof(kShStrTab);
    for (std::size_t i = 0; i < shStrTabSize; i++)
        buf.push_back(static_cast<std::uint8_t>(kShStrTab[i]));

    const std::uint64_t shOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::size_t shEntSize = 64;
    constexpr std::size_t shNum = 6;
    buf.resize(buf.size() + shEntSize * shNum, 0);

    _writeSectionHeader(buf, shOff + shEntSize * 0, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 1, nameShStrTab, SHT_STRTAB, 0, shStrTabOff, shStrTabOff, shStrTabSize, 0, 0, 1, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 2, nameDynStr, SHT_STRTAB, SHF_ALLOC, dynStrOff, dynStrOff, dynSection.DynStrData.size(), 0, 0, 1, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 3, nameDynSym, SHT_DYNSYM, SHF_ALLOC, dynSymOff, dynSymOff, dynSection.DynSymData.size(), 2, 1, 8, 24);
    _writeSectionHeader(buf, shOff + shEntSize * 4, nameDynamic, SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, dynSegOff, dynSegOff, dynSegBuf.size(), 2, 0, 8, 16);
    _writeSectionHeader(buf, shOff + shEntSize * 5, nameText, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, stubOff, stubOff, stubBytes.size(), 0, 0, 1, 0);

    _writeU64(buf, 40, shOff);
    _writeU16(buf, 58, static_cast<std::uint16_t>(shEntSize));
    _writeU16(buf, 60, static_cast<std::uint16_t>(shNum));
    _writeU16(buf, 62, 1);

    return buf;
}

}
