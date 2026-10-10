#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

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
    _byteWriter->WriteU32(buf, offset + kPhdrTypeOffset, ph.Type);
    _byteWriter->WriteU32(buf, offset + kPhdrFlagsOffset, ph.Flags);
    _byteWriter->WriteU64(buf, offset + kPhdrOffsetOffset, ph.Offset);
    _byteWriter->WriteU64(buf, offset + kPhdrVaddrOffset, ph.MappedAddress);
    _byteWriter->WriteU64(buf, offset + kPhdrPaddrOffset, ph.PhysicalAddress);
    _byteWriter->WriteU64(buf, offset + kPhdrFileSizeOffset, ph.FileSize);
    _byteWriter->WriteU64(buf, offset + kPhdrMemSizeOffset, ph.MemorySize);
    _byteWriter->WriteU64(buf, offset + kPhdrAlignOffset, ph.Alignment);
}

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeLoadHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_LOAD;
    ph.Flags = PF_R | PF_W | PF_X;
    ph.Offset = offset;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = kDefaultLoadAlignment;
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
    ph.Alignment = kPhdrHeaderAlignment;
    return ph;
}

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeDynamicHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_DYNAMIC;
    ph.Flags = PF_R | PF_W;
    ph.Offset = offset;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = kDynamicHeaderAlignment;
    return ph;
}

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeInterpHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_INTERP;
    ph.Flags = PF_R;
    ph.Offset = offset;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = size;
    ph.Alignment = kInterpHeaderAlignment;
    return ph;
}

std::uint32_t ProgramHeaderLayoutBuilder::_fixLoadFlags(std::uint32_t originalFlags) const {
    if (originalFlags == 0) return PF_R | PF_W | PF_X;
    return originalFlags | PF_R;
}

std::uint64_t ProgramHeaderLayoutBuilder::ComputeExtraBlockVaddr(
    const std::vector<Domain::ProgramHeader>& originalHeaders,
    std::uint64_t extraBlockOffset
) const {
    bool foundLoad = false;
    std::uint64_t highestVaddrEnd = 0;
    for (const auto& ph : originalHeaders) {
        if (_segmentFilter->ShouldSkip(ph))
            continue;
        if (ph.Type != PT_LOAD)
            continue;
        foundLoad = true;
        const std::uint64_t vaddrEnd = ph.MappedAddress + ph.MemorySize;
        if (vaddrEnd > highestVaddrEnd)
            highestVaddrEnd = vaddrEnd;
    }
    if (!foundLoad)
        throw Domain::RelinkerException("No kept PT_LOAD segment to anchor the extra block against");

    const std::uint64_t align = kDefaultLoadAlignment;
    const std::uint64_t offsetRemainder = extraBlockOffset & (align - 1);
    const std::uint64_t alignedFloor = highestVaddrEnd & ~(align - 1);
    std::uint64_t vaddr = alignedFloor | offsetRemainder;
    if (vaddr < highestVaddrEnd)
        vaddr += align;
    if (vaddr < highestVaddrEnd)
        throw Domain::RelinkerException("Extra block vaddr computation overflowed");
    return vaddr;
}

std::uint16_t ProgramHeaderLayoutBuilder::WriteLayout(
    std::vector<std::uint8_t>& buf,
    const ProgramHeaderLayoutRequest& request
) const {
    std::vector<Domain::ProgramHeader> kept;
    std::size_t frameCount = 0;
    for (const auto& ph : request.OriginalHeaders) {
        if (_segmentFilter->ShouldSkip(ph))
            continue;
        kept.push_back(ph);
        if (ph.Type == PT_GNU_EH_FRAME)
            frameCount++;
    }

    const bool keepFrames = kept.size() + kSyntheticProgramHeaderCount <= request.PhNum;
    if (!keepFrames && frameCount > 0) {
        std::erase_if(kept, [](const Domain::ProgramHeader& ph) { return ph.Type == PT_GNU_EH_FRAME; });
        std::cerr << "WARNING: No free program header slot for PT_GNU_EH_FRAME; C++ exceptions thrown in the executable cannot be caught.\n";
    }
    _sortLoadHeaders(kept);

    const std::size_t neededPh = kept.size() + kSyntheticProgramHeaderCount;
    if (neededPh > request.PhNum)
        throw Domain::RelinkerException(
            "Not enough program header slots: need " + std::to_string(neededPh) +
            ", available " + std::to_string(request.PhNum));

    const std::uint64_t headerBlockSize = request.PhOff + static_cast<std::uint64_t>(neededPh) * request.PhEntSize;
    std::uint64_t headerBlockAlign = kDefaultLoadAlignment;
    for (const auto& ph : request.OriginalHeaders) {
        if (ph.Type == PT_LOAD && !_segmentFilter->ShouldSkip(ph) && ph.Alignment > headerBlockAlign && (ph.Alignment & (ph.Alignment - 1)) == 0)
            headerBlockAlign = ph.Alignment;
    }
    const std::uint64_t headerBlockVaddr =
        (request.ExtraBlockVaddr + request.ExtraBlockSize + headerBlockAlign - 1) & ~(headerBlockAlign - 1);

    if (request.DynamicSegmentOffset < request.ExtraBlockOffset)
        throw Domain::RelinkerException("Dynamic segment offset lies before the extra block");
    if (request.DynamicSegmentOffset + request.DynamicSegmentSize > request.ExtraBlockOffset + request.ExtraBlockSize)
        throw Domain::RelinkerException("Dynamic segment does not fit within the extra block");
    if (request.InterpOffset < request.ExtraBlockOffset)
        throw Domain::RelinkerException("Interp offset lies before the extra block");
    if (request.InterpOffset + request.InterpSize > request.ExtraBlockOffset + request.ExtraBlockSize)
        throw Domain::RelinkerException("Interp data does not fit within the extra block");

    const std::uint64_t dynamicSegmentVaddr = request.ExtraBlockVaddr + (request.DynamicSegmentOffset - request.ExtraBlockOffset);
    const std::uint64_t interpVaddr = request.ExtraBlockVaddr + (request.InterpOffset - request.ExtraBlockOffset);

    std::vector<Domain::ProgramHeader> layout;
    layout.push_back(_makePhdrHeader(request.PhOff, headerBlockVaddr + request.PhOff, static_cast<std::uint64_t>(neededPh) * request.PhEntSize));
    layout.push_back(_makeInterpHeader(request.InterpOffset, interpVaddr, request.InterpSize));
    for (auto ph : kept) {
        if (ph.Type == PT_LOAD)
            ph.Flags = _fixLoadFlags(ph.Flags);
        layout.push_back(ph);
    }
    layout.push_back(_makeLoadHeader(request.ExtraBlockOffset, request.ExtraBlockVaddr, request.ExtraBlockSize));
    layout.push_back(_makeHeaderBlockLoad(headerBlockVaddr, headerBlockSize, headerBlockAlign));
    layout.push_back(_makeDynamicHeader(request.DynamicSegmentOffset, dynamicSegmentVaddr, request.DynamicSegmentSize));

    for (std::size_t index = 0; index < layout.size(); ++index)
        _writeProgramHeader(buf, static_cast<std::size_t>(request.PhOff) + index * request.PhEntSize, layout[index]);

    return static_cast<std::uint16_t>(layout.size());
}

void ProgramHeaderLayoutBuilder::_sortLoadHeaders(std::vector<Domain::ProgramHeader>& headers) const {
    std::vector<Domain::ProgramHeader> loads;
    for (const auto& ph : headers) {
        if (ph.Type == PT_LOAD)
            loads.push_back(ph);
    }
    std::stable_sort(loads.begin(), loads.end(), [](const Domain::ProgramHeader& left, const Domain::ProgramHeader& right) {
        return left.MappedAddress < right.MappedAddress;
    });
    std::size_t next = 0;
    for (auto& ph : headers) {
        if (ph.Type == PT_LOAD)
            ph = loads[next++];
    }
}

}
