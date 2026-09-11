#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>

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
    _byteWriter->WriteU32(buf, offset + kShdrNameOffset, nameOff);
    _byteWriter->WriteU32(buf, offset + kShdrTypeOffset, type);
    _byteWriter->WriteU64(buf, offset + kShdrFlagsOffset, flags);
    _byteWriter->WriteU64(buf, offset + kShdrAddrOffset, addr);
    _byteWriter->WriteU64(buf, offset + kShdrFileOffsetOffset, fileOffset);
    _byteWriter->WriteU64(buf, offset + kShdrSizeOffset, size);
    _byteWriter->WriteU32(buf, offset + kShdrLinkOffset, link);
    _byteWriter->WriteU32(buf, offset + kShdrInfoOffset, info);
    _byteWriter->WriteU64(buf, offset + kShdrAlignOffset, align);
    _byteWriter->WriteU64(buf, offset + kShdrEntSizeOffset, entSize);
}

void SectionHeaderTableBuilder::WriteTable(
    std::vector<std::uint8_t>& buf,
    const SectionHeaderTableRequest& request
) const {
    const auto shStrTabOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t shStrTabSize = sizeof(kShStrTabBlob);
    for (const char i : kShStrTabBlob)
        buf.push_back(static_cast<std::uint8_t>(i));

    const auto shOff = static_cast<std::uint64_t>(buf.size());
    buf.resize(buf.size() + kShdrEntrySize * kSectionHeaderCount, 0);

    _writeSectionHeader(buf, shOff + kShdrEntrySize * 0, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 1, kShStrTabNameOffset, SHT_STRTAB, 0, shStrTabOff, shStrTabOff, shStrTabSize, 0, 0, kNoSpecialAlignment, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 2, kDynStrNameOffset, SHT_STRTAB, SHF_ALLOC, request.DynStrOffset, request.DynStrOffset, request.DynStrSize, 0, 0, kNoSpecialAlignment, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 3, kDynSymNameOffset, SHT_DYNSYM, SHF_ALLOC, request.DynSymOffset, request.DynSymOffset, request.DynSymSize, kDynSymLinkToStrTab, kDynSymInfoFirstGlobal, kDynSymAlign, kSymEntrySize);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 4, kDynamicNameOffset, SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, request.DynamicSegmentOffset, request.DynamicSegmentOffset, request.DynamicSegmentSize, kDynamicLinkToStrTab, 0, kDynSymAlign, kDynEntrySize);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 5, kTextNameOffset, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, request.StubOffset, request.StubOffset, request.StubSize, 0, 0, kNoSpecialAlignment, 0);

    _byteWriter->WriteU64(buf, kEhdrShOffOffset, shOff);
    _byteWriter->WriteU16(buf, kEhdrShEntSizeOffset, static_cast<std::uint16_t>(kShdrEntrySize));
    _byteWriter->WriteU16(buf, kEhdrShNumOffset, static_cast<std::uint16_t>(kSectionHeaderCount));
    _byteWriter->WriteU16(buf, kEhdrShStrNdxOffset, static_cast<std::uint16_t>(kShStrTabIndex));
}

}
