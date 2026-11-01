#include <elfpatcher/general/SectionHeaderTableBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

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
    if (buf.size() < kElfHeaderSize)
        throw std::runtime_error("Output program header table is out of bounds");
    const auto phOffset = Io::ReadU64(buf, kEhdrPhOffOffset);
    const auto phCount = Io::ReadU16(buf, kEhdrPhNumOffset);
    const auto phStride = Io::ReadU16(buf, kEhdrPhEntSizeOffset);
    if (phStride < 56 || (phCount > 0 && phOffset < kElfHeaderSize) || phOffset > buf.size() || phCount > (buf.size() - phOffset) / phStride)
        throw std::runtime_error("Output program header table is out of bounds");
    const auto phTableEnd = phOffset + static_cast<std::uint64_t>(phCount) * phStride;
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 4> sections{{
        {request.DynStrOffset, request.DynStrSize},
        {request.DynSymOffset, request.DynSymSize},
        {request.DynamicSegmentOffset, request.DynamicSegmentSize},
        {request.StubOffset, request.StubSize},
    }};
    for (std::size_t i = 0; i < sections.size(); ++i) {
        if (sections[i].second == 0) continue;
        for (std::size_t j = i + 1; j < sections.size(); ++j) {
            if (sections[j].second == 0) continue;
            const auto startA = sections[i].first;
            const auto endA = startA + sections[i].second;
            const auto startB = sections[j].first;
            const auto endB = startB + sections[j].second;
            if (startA < endB && startB < endA)
                throw std::runtime_error("Allocated output sections overlap");
        }
    }
    const auto mappedAddress = [&](std::uint64_t offset, std::uint64_t sectionSize) {
        if (offset > buf.size() || sectionSize > buf.size() - offset)
            throw std::runtime_error("Allocated output section exceeds file");
        if (offset < kElfHeaderSize && sectionSize > 0)
            throw std::runtime_error("Allocated output section overlaps the ELF header");
        if (sectionSize > 0 && phCount > 0 && offset < phTableEnd && phOffset < offset + sectionSize)
            throw std::runtime_error("Allocated output section overlaps the program header table");
        for (std::uint16_t index = 0; index < phCount; ++index) {
            const auto header = phOffset + index * phStride;
            if (Io::ReadU32(buf, header) != PT_LOAD) continue;
            const auto start = Io::ReadU64(buf, header + 8);
            const auto size = Io::ReadU64(buf, header + 32);
            if (offset >= start && offset - start <= size && sectionSize <= size - (offset - start)) {
                const auto relative = offset - start;
                const auto address = Io::ReadU64(buf, header + 16);
                if (relative > std::numeric_limits<std::uint64_t>::max() - address ||
                    sectionSize > std::numeric_limits<std::uint64_t>::max() - (address + relative))
                    throw std::runtime_error("Allocated output section address overflowed");
                return address + relative;
            }
        }
        throw std::runtime_error("Allocated output section is outside load segments");
    };

    const auto dynStrAddress = mappedAddress(request.DynStrOffset, request.DynStrSize);
    const auto dynSymAddress = mappedAddress(request.DynSymOffset, request.DynSymSize);
    const auto dynamicAddress = mappedAddress(request.DynamicSegmentOffset, request.DynamicSegmentSize);
    const auto stubAddress = mappedAddress(request.StubOffset, request.StubSize);

    const auto shStrTabOff = static_cast<std::uint64_t>(buf.size());
    constexpr std::uint64_t shStrTabSize = sizeof(kShStrTabBlob);
    for (const char i : kShStrTabBlob)
        buf.push_back(static_cast<std::uint8_t>(i));

    Io::AlignBuffer(buf, std::size_t{8});
    const auto shOff = static_cast<std::uint64_t>(buf.size());
    buf.resize(buf.size() + kShdrEntrySize * kSectionHeaderCount, 0);

    _writeSectionHeader(buf, shOff + kShdrEntrySize * 0, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 1, kShStrTabNameOffset, SHT_STRTAB, 0, 0, shStrTabOff, shStrTabSize, 0, 0, kNoSpecialAlignment, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 2, kDynStrNameOffset, SHT_STRTAB, SHF_ALLOC, dynStrAddress, request.DynStrOffset, request.DynStrSize, 0, 0, kNoSpecialAlignment, 0);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 3, kDynSymNameOffset, SHT_DYNSYM, SHF_ALLOC, dynSymAddress, request.DynSymOffset, request.DynSymSize, kDynSymLinkToStrTab, kDynSymInfoFirstGlobal, kDynSymAlign, kSymEntrySize);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 4, kDynamicNameOffset, SHT_DYNAMIC, SHF_ALLOC | SHF_WRITE, dynamicAddress, request.DynamicSegmentOffset, request.DynamicSegmentSize, kDynamicLinkToStrTab, 0, kDynSymAlign, kDynEntrySize);
    _writeSectionHeader(buf, shOff + kShdrEntrySize * 5, kTextNameOffset, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, stubAddress, request.StubOffset, request.StubSize, 0, 0, kNoSpecialAlignment, 0);

    _byteWriter->WriteU64(buf, kEhdrShOffOffset, shOff);
    _byteWriter->WriteU16(buf, kEhdrShEntSizeOffset, static_cast<std::uint16_t>(kShdrEntrySize));
    _byteWriter->WriteU16(buf, kEhdrShNumOffset, static_cast<std::uint16_t>(kSectionHeaderCount));
    _byteWriter->WriteU16(buf, kEhdrShStrNdxOffset, static_cast<std::uint16_t>(kShStrTabIndex));
}

}
