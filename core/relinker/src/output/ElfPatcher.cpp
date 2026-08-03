#include <relinker/output/ElfPatcher.hpp>
#include <fstream>
#include <cstring>

namespace Relinker {

bool ElfPatcher::_isSceSpecificSegment(std::uint32_t type) const {
    return type == PT_SCE_DYNLIBDATA
        || type == PT_OS_PROCPARAM
        || type == PT_OS_RELRO
        || (type >= 0x61000000 && type <= 0x6fffffff);
}

void ElfPatcher::_writeU16(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint16_t v) const {
    std::memcpy(buf.data() + offset, &v, 2);
}

void ElfPatcher::_writeU32(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint32_t v) const {
    std::memcpy(buf.data() + offset, &v, 4);
}

void ElfPatcher::_writeU64(std::vector<std::uint8_t>& buf, std::size_t offset, std::uint64_t v) const {
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

void ElfPatcher::_writeProgramHeader(std::vector<std::uint8_t>& buf, std::size_t offset, const ProgramHeader& ph) const {
    _writeU32(buf, offset + 0, ph.Type);
    _writeU32(buf, offset + 4, ph.Flags);
    _writeU64(buf, offset + 8, ph.Offset);
    _writeU64(buf, offset + 16, ph.MappedAddress);
    _writeU64(buf, offset + 24, ph.PhysicalAddress);
    _writeU64(buf, offset + 32, ph.FileSize);
    _writeU64(buf, offset + 40, ph.MemorySize);
    _writeU64(buf, offset + 48, ph.Alignment);
}

std::uint32_t ElfPatcher::_fixLoadFlags(std::uint32_t originalFlags) const {
    std::uint32_t flags = originalFlags;
    if ((flags & (PF_R | PF_W | PF_X)) == 0)
        flags |= PF_R;
    flags |= PF_R;
    return flags;
}

void ElfPatcher::PatchAndWrite(
    const std::string& inputPath,
    const std::string& outputPath,
    const std::vector<ProgramHeader>& originalHeaders,
    const SysVDynamicSection& dynSection)
{
    std::ifstream in(inputPath, std::ios::binary | std::ios::ate);
    if (!in)
        throw RelinkerException("Cannot open input ELF: " + inputPath);
    const std::streamsize fileSize = in.tellg();
    in.seekg(0);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(fileSize));
    if (!in.read(reinterpret_cast<char*>(buf.data()), fileSize))
        throw RelinkerException("Cannot read input ELF: " + inputPath);

    buf[7] = 0;
    buf[8] = 0;
    buf[0x10] = static_cast<std::uint8_t>(ET_DYN & 0xFF);
    buf[0x11] = static_cast<std::uint8_t>((ET_DYN >> 8) & 0xFF);

    const std::uint64_t phOff = *reinterpret_cast<const std::uint64_t*>(buf.data() + 32);
    const std::uint16_t phEntSize = *reinterpret_cast<const std::uint16_t*>(buf.data() + 54);
    const std::uint16_t phNum = *reinterpret_cast<const std::uint16_t*>(buf.data() + 56);

    std::vector<ProgramHeader> loadSegs;
    for (const auto& ph : originalHeaders)
        if (ph.Type == PT_LOAD)
            loadSegs.push_back(ph);

    const std::uint64_t dynStrOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynStrData)
        buf.push_back(b);

    const std::uint64_t dynSymOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.DynSymData)
        buf.push_back(b);

    const std::uint64_t relaOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaData)
        buf.push_back(b);

    const std::uint64_t relaPltOff = static_cast<std::uint64_t>(buf.size());
    for (std::uint8_t b : dynSection.RelaPltData)
        buf.push_back(b);

    std::vector<std::uint8_t> dynSegBuf;
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

    std::uint16_t newPhNum = 0;
    for (const auto& ph : originalHeaders) {
        if (_isSceSpecificSegment(ph.Type))
            continue;
        if (ph.Type == PT_DYNAMIC)
            continue;
        newPhNum++;
    }
    newPhNum++;

    std::uint16_t writtenPh = 0;
    for (const auto& ph : originalHeaders) {
        if (_isSceSpecificSegment(ph.Type))
            continue;
        if (ph.Type == PT_DYNAMIC)
            continue;
        if (writtenPh >= phNum)
            break;
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        if (ph.Type == PT_LOAD) {
            ProgramHeader fixed = ph;
            fixed.Flags = _fixLoadFlags(ph.Flags);
            _writeProgramHeader(buf, phEntOff, fixed);
        } else {
            _writeProgramHeader(buf, phEntOff, ph);
        }
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(phOff) + writtenPh * phEntSize;
        ProgramHeader dynPh{};
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

    _writeU16(buf, 56, writtenPh);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
        throw RelinkerException("Cannot open output ELF: " + outputPath);
    out.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!out)
        throw RelinkerException("Failed to write output ELF: " + outputPath);
}

}
