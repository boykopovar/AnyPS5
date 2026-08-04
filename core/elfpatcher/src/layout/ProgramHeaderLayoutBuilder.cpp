#include <elfpatcher/layout/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/domain/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <cstdint>
#include <string>

namespace Elfpatcher {

ProgramHeaderLayoutBuilder::ProgramHeaderLayoutBuilder(
    std::shared_ptr<ISegmentFilter> segmentFilter,
    std::shared_ptr<Io::IByteWriter> byteWriter
)
    : _segmentFilter(std::move(segmentFilter))
    , _byteWriter(std::move(byteWriter))
{
}

void ProgramHeaderLayoutBuilder::_writeProgramHeader(std::vector<std::uint8_t>& buf, std::size_t offset, const Domain::ProgramHeader& ph) const {
    _byteWriter->WriteU32(buf, offset + 0, ph.Type);
    _byteWriter->WriteU32(buf, offset + 4, ph.Flags);
    _byteWriter->WriteU64(buf, offset + 8, ph.Offset);
    _byteWriter->WriteU64(buf, offset + 16, ph.MappedAddress);
    _byteWriter->WriteU64(buf, offset + 24, ph.PhysicalAddress);
    _byteWriter->WriteU64(buf, offset + 32, ph.FileSize);
    _byteWriter->WriteU64(buf, offset + 40, ph.MemorySize);
    _byteWriter->WriteU64(buf, offset + 48, ph.Alignment);
}

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeLoadHeader(std::uint64_t offset, std::uint64_t size) const {
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

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeHeaderBlockLoad(std::uint64_t vaddr, std::uint64_t size, std::uint64_t align) const {
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

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makePhdrHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const {
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

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeDynamicHeader(std::uint64_t offset, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_DYNAMIC;
    ph.Flags = PF_R | PF_W;
    ph.Offset = offset;
    ph.MappedAddress = offset;
    ph.PhysicalAddress = offset;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = 8;
    return ph;
}

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeInterpHeader(std::uint64_t offset, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_INTERP;
    ph.Flags = PF_R;
    ph.Offset = offset;
    ph.MappedAddress = offset;
    ph.PhysicalAddress = offset;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = 1;
    return ph;
}

std::uint32_t ProgramHeaderLayoutBuilder::_fixLoadFlags(std::uint32_t originalFlags) const {
    return originalFlags | PF_R;
}

std::uint16_t ProgramHeaderLayoutBuilder::WriteLayout(
    std::vector<std::uint8_t>& buf,
    const ProgramHeaderLayoutRequest& request
) const {
    std::uint16_t keptCount = 0;
    std::uint64_t keptMinVaddr = UINT64_MAX;
    std::uint64_t keptMinVaddrAlign = 0x1000;
    for (const auto& ph : request.OriginalHeaders) {
        if (_segmentFilter->ShouldSkip(ph))
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
    if (neededPh > request.PhNum)
        throw Domain::RelinkerException(
            "Not enough program header slots: need " + std::to_string(neededPh) +
            ", available " + std::to_string(request.PhNum));

    const std::uint64_t headerBlockSize = request.PhOff + static_cast<std::uint64_t>(neededPh) * request.PhEntSize;
    if (headerBlockSize >= keptMinVaddr)
        throw Domain::RelinkerException(
            "Header block does not fit below the lowest kept PT_LOAD: need " +
            std::to_string(headerBlockSize) + " bytes, only " + std::to_string(keptMinVaddr) + " available");

    const std::uint64_t headerBlockVaddr = (keptMinVaddr - headerBlockSize) & ~(keptMinVaddrAlign - 1);
    if (headerBlockVaddr + headerBlockSize > keptMinVaddr)
        throw Domain::RelinkerException("Header block would overlap the lowest kept PT_LOAD after alignment");

    std::uint16_t writtenPh = 0;

    const std::size_t phdrEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
    _writeProgramHeader(buf, phdrEntOff, _makePhdrHeader(request.PhOff, headerBlockVaddr + request.PhOff, static_cast<std::uint64_t>(neededPh) * request.PhEntSize));
    writtenPh++;

    const std::size_t headerLoadEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
    _writeProgramHeader(buf, headerLoadEntOff, _makeHeaderBlockLoad(headerBlockVaddr, headerBlockSize, keptMinVaddrAlign));
    writtenPh++;

    for (const auto& ph : request.OriginalHeaders) {
        if (_segmentFilter->ShouldSkip(ph))
            continue;
        const std::size_t phEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
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
        const std::size_t phEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
        _writeProgramHeader(buf, phEntOff, _makeLoadHeader(request.ExtraBlockOffset, request.ExtraBlockSize));
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
        _writeProgramHeader(buf, phEntOff, _makeDynamicHeader(request.DynamicSegmentOffset, request.DynamicSegmentSize));
        writtenPh++;
    }

    {
        const std::size_t phEntOff = static_cast<std::size_t>(request.PhOff) + writtenPh * request.PhEntSize;
        _writeProgramHeader(buf, phEntOff, _makeInterpHeader(request.InterpOffset, request.InterpSize));
        writtenPh++;
    }

    return writtenPh;
}

}
