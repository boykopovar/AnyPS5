#include <elfpatcher/sections/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/domain/ElfConstants.hpp>

namespace Elfpatcher {

SectionHeaderTableBuilder::SectionHeaderTableBuilder(std::shared_ptr<Io::IByteWriter> byteWriter)
    : _byteWriter(std::move(byteWriter))
{
}

void SectionHeaderTableBuilder::_writeSectionHeader(
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
    _byteWriter->WriteU32(buf, offset + 0, nameOff);
    _byteWriter->WriteU32(buf, offset + 4, type);
    _byteWriter->WriteU64(buf, offset + 8, flags);
    _byteWriter->WriteU64(buf, offset + 16, addr);
    _byteWriter->WriteU64(buf, offset + 24, fileOffset);
    _byteWriter->WriteU64(buf, offset + 32, size);
    _byteWriter->WriteU32(buf, offset + 40, link);
    _byteWriter->WriteU32(buf, offset + 44, info);
    _byteWriter->WriteU64(buf, offset + 48, align);
    _byteWriter->WriteU64(buf, offset + 56, entSize);
}

void SectionHeaderTableBuilder::WriteTable(
    std::vector<std::uint8_t>& buf,
    const SectionHeaderTableRequest& request
) const {
    static constexpr char kShStrTab[] = "\0.shstrtab\0.dynstr\0.dynsym\0.dynamic\0.text";
    constexpr std::uint32_t nameShStrTab = 1;
    constexpr std::uint32_t nameDynStr = 11;
    constexpr std::uint32_t nameDynSym = 19;
    constexpr std::uint32_t nameDynamic = 27;
    constexpr std::uint32_t nameText = 36;
    const auto shStrTabOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t shStrTabSize = sizeof(kShStrTab);
    for (const char i : kShStrTab)
        buf.push_back(static_cast<std::uint8_t>(i));

    const auto shOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::size_t shEntSize = 64;
    constexpr std::size_t shNum = 6;
    buf.resize(buf.size() + shEntSize * shNum, 0);

    _writeSectionHeader(buf, shOff + shEntSize * 0, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 1, nameShStrTab, SHT_STRTAB, 0, shStrTabOff, shStrTabOff, shStrTabSize, 0, 0, 1, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 2, nameDynStr, SHT_STRTAB, SHF_ALLOC, request.DynStrOffset, request.DynStrOffset, request.DynStrSize, 0, 0, 1, 0);
    _writeSectionHeader(buf, shOff + shEntSize * 3, nameDynSym, SHT_DYNSYM, SHF_ALLOC, request.DynSymOffset, request.DynSymOffset, request.DynSymSize, 2, 1, 8, 24);
    _writeSectionHeader(buf, shOff + shEntSize * 4, nameDynamic, SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, request.DynamicSegmentOffset, request.DynamicSegmentOffset, request.DynamicSegmentSize, 2, 0, 8, 16);
    _writeSectionHeader(buf, shOff + shEntSize * 5, nameText, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, request.StubOffset, request.StubOffset, request.StubSize, 0, 0, 1, 0);

    _byteWriter->WriteU64(buf, 40, shOff);
    _byteWriter->WriteU16(buf, 58, static_cast<std::uint16_t>(shEntSize));
    _byteWriter->WriteU16(buf, 60, static_cast<std::uint16_t>(shNum));
    _byteWriter->WriteU16(buf, 62, 1);
}

}
