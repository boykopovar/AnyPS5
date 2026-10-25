#include <elfpatcher/general/ProgramHeaderLayoutBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <domain/Types.hpp>
#include <algorithm>
#include <cstdint>

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Elfpatcher {

namespace {

constexpr std::size_t kExtendedProgramHeaderCount = 0xffff;

bool hasWritableLoadCoverage(
    const std::vector<Domain::ProgramHeader>& headers,
    const Domain::ProgramHeader& relro
) {
    if (relro.MemorySize == 0) return true;
    if (relro.MemorySize > std::numeric_limits<std::uint64_t>::max() - relro.MappedAddress) return false;
    const std::uint64_t pageMask = kDefaultLoadAlignment - 1;
    const std::uint64_t relroStart = relro.MappedAddress & ~pageMask;
    const std::uint64_t relroEndBytes = relro.MappedAddress + relro.MemorySize;
    if (relroEndBytes > std::numeric_limits<std::uint64_t>::max() - pageMask) return false;
    const std::uint64_t relroEnd = (relroEndBytes + pageMask) & ~pageMask;
    if (relroStart == relroEnd) return true;

    std::vector<std::pair<std::uint64_t, std::uint64_t>> writableRanges;
    for (const auto& header : headers) {
        if (header.Type != PT_LOAD || (header.Flags & PF_W) == 0) continue;
        const std::uint64_t size = std::max(header.FileSize, header.MemorySize);
        if (size > std::numeric_limits<std::uint64_t>::max() - header.MappedAddress) continue;
        const std::uint64_t start = header.MappedAddress & ~pageMask;
        const std::uint64_t endBytes = header.MappedAddress + size;
        if (endBytes > std::numeric_limits<std::uint64_t>::max() - pageMask) continue;
        const std::uint64_t end = (endBytes + pageMask) & ~pageMask;
        if (start < end) writableRanges.emplace_back(start, end);
    }
    std::sort(writableRanges.begin(), writableRanges.end());
    std::uint64_t coveredUntil = relroStart;
    for (const auto& [start, end] : writableRanges) {
        if (start > coveredUntil) break;
        if (end > coveredUntil) coveredUntil = end;
        if (coveredUntil >= relroEnd) return true;
    }
    return false;
}

}

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

Domain::ProgramHeader ProgramHeaderLayoutBuilder::_makeLoadHeader(std::uint64_t offset, std::uint64_t vaddr, std::uint64_t size, std::uint64_t align, std::uint64_t memorySize, std::uint32_t flags) const {
    Domain::ProgramHeader ph{};
    ph.Type = PT_LOAD;
    ph.Flags = flags;
    ph.Offset = offset;
    ph.MappedAddress = vaddr;
    ph.PhysicalAddress = vaddr;
    ph.FileSize = size;
    ph.MemorySize = memorySize < size ? size : memorySize;
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
    const std::uint64_t pageFloor = GuestLoadPageEnd(originalHeaders);

    const std::uint64_t offsetRemainder = extraBlockOffset & (kDefaultLoadAlignment - 1);
    if (pageFloor > std::numeric_limits<std::uint64_t>::max() - offsetRemainder)
        throw Domain::RelinkerException("Extra block vaddr computation overflowed");
    return pageFloor + offsetRemainder;
}

std::vector<Domain::ProgramHeader> ProgramHeaderLayoutBuilder::_keptHeaders(const std::vector<Domain::ProgramHeader>& originalHeaders) const {
    std::vector<Domain::ProgramHeader> kept;
    for (const auto& ph : originalHeaders) {
        if (_segmentFilter->ShouldSkip(ph)) continue;
        if (ph.Type == PT_GNU_RELRO && !hasWritableLoadCoverage(originalHeaders, ph)) continue;
        kept.push_back(ph);
    }
    return kept;
}

std::vector<Domain::ProgramHeader> ProgramHeaderLayoutBuilder::_outputHeaders(const std::vector<Domain::ProgramHeader>& originalHeaders) const {
    auto kept = _keptHeaders(originalHeaders);
    if (std::none_of(kept.begin(), kept.end(), [](const Domain::ProgramHeader& ph) { return ph.Type == PT_GNU_STACK; }))
        kept.push_back(Domain::ProgramHeader{PT_GNU_STACK, PF_R | PF_W, 0, 0, 0, 0, 0, 16});
    if (kept.size() >= kExtendedProgramHeaderCount - kSyntheticProgramHeaderCount)
        throw Domain::RelinkerException("Extended program header counts are not supported");
    return kept;
}

std::size_t ProgramHeaderLayoutBuilder::RequiredProgramHeaderCount(const std::vector<Domain::ProgramHeader>& originalHeaders) const {
    return _outputHeaders(originalHeaders).size() + kSyntheticProgramHeaderCount;
}

std::uint64_t ProgramHeaderLayoutBuilder::GuestLoadPageEnd(
    const std::vector<Domain::ProgramHeader>& originalHeaders
) const {
    bool foundLoad = false;
    std::uint64_t highestVaddrEnd = 0;
    for (const auto& ph : originalHeaders) {
        if (_segmentFilter->ShouldSkip(ph))
            continue;
        if (ph.Type != PT_LOAD)
            continue;
        foundLoad = true;
        if (ph.FileSize > ph.MemorySize)
            throw Domain::RelinkerException("PT_LOAD file size exceeds memory size", ph.Offset);
        if (ph.MemorySize > std::numeric_limits<std::uint64_t>::max() - ph.MappedAddress)
            throw Domain::RelinkerException("Load segment virtual range overflowed");
        const std::uint64_t vaddrEnd = ph.MappedAddress + ph.MemorySize;
        if (vaddrEnd > highestVaddrEnd)
            highestVaddrEnd = vaddrEnd;
    }
    if (!foundLoad)
        throw Domain::RelinkerException("No kept PT_LOAD segment to anchor the extra block against");
    if (highestVaddrEnd > std::numeric_limits<std::uint64_t>::max() - (kDefaultLoadAlignment - 1))
        throw Domain::RelinkerException("Extra block vaddr computation overflowed");
    return (highestVaddrEnd + kDefaultLoadAlignment - 1) & ~(kDefaultLoadAlignment - 1);
}

std::uint16_t ProgramHeaderLayoutBuilder::WriteLayout(
    std::vector<std::uint8_t>& buf,
    const ProgramHeaderLayoutRequest& request
) const {
    auto kept = _outputHeaders(request.OriginalHeaders);
    const auto frameCount = std::count_if(kept.begin(), kept.end(), [](const Domain::ProgramHeader& ph) { return ph.Type == PT_GNU_EH_FRAME; });

    const bool keepFrames = kept.size() + kSyntheticProgramHeaderCount <= request.PhNum;
    if (!keepFrames && frameCount > 0) {
        throw Domain::RelinkerException("No free program header slot for PT_GNU_EH_FRAME");
    }
    _sortLoadHeaders(kept);

    const std::size_t neededPh = kept.size() + kSyntheticProgramHeaderCount;
    if (neededPh > request.PhNum)
        throw Domain::RelinkerException(
            "Not enough program header slots: need " + std::to_string(neededPh) +
            ", available " + std::to_string(request.PhNum));

    const auto checkedEnd = [](std::uint64_t start, std::uint64_t size) {
        if (size > std::numeric_limits<std::uint64_t>::max() - start)
            throw Domain::RelinkerException("Program header layout range overflowed");
        return start + size;
    };
    if (request.PhEntSize < 56)
        throw Domain::RelinkerException("Program header entry is too small");
    const std::uint64_t tableEnd = checkedEnd(request.PhOff, static_cast<std::uint64_t>(neededPh) * request.PhEntSize);
    if (tableEnd > buf.size())
        throw Domain::RelinkerException("Program header table exceeds the output file");
    const bool tableInExtraBlock = request.PhOff >= request.ExtraBlockOffset;
    if (tableInExtraBlock) {
        if (tableEnd - request.ExtraBlockOffset > request.ExtraBlockSize)
            throw Domain::RelinkerException("Program header table does not fit within the extra block");
    } else if (tableEnd > request.ExtraBlockOffset) {
        throw Domain::RelinkerException("Program header table overlaps the extra block");
    }
    const std::uint64_t headerBlockSize = tableInExtraBlock ? kElfHeaderSize : tableEnd;
    if (request.ExtraBlockOffset > buf.size() || request.ExtraBlockSize > buf.size() - request.ExtraBlockOffset)
        throw Domain::RelinkerException("Extra block exceeds the output file");
    std::uint64_t headerBlockAlign = kDefaultLoadAlignment;
    for (const auto& ph : request.OriginalHeaders) {
        if (ph.Type == PT_LOAD && !_segmentFilter->ShouldSkip(ph) && ph.Alignment > headerBlockAlign && (ph.Alignment & (ph.Alignment - 1)) == 0)
            headerBlockAlign = ph.Alignment;
    }

    const std::uint64_t extraBlockVaddr = ComputeExtraBlockVaddr(request.OriginalHeaders, request.ExtraBlockOffset);
    if (request.ExtraBlockVaddr != extraBlockVaddr)
        throw Domain::RelinkerException("Extra block vaddr does not match its file offset");
    const std::uint64_t headerBlockVaddr =
        checkedEnd(checkedEnd(extraBlockVaddr, request.ExtraBlockSize), headerBlockAlign - 1) & ~(headerBlockAlign - 1);
    checkedEnd(headerBlockVaddr, headerBlockSize);

    if (request.DynamicSegmentOffset < request.ExtraBlockOffset)
        throw Domain::RelinkerException("Dynamic segment offset lies before the extra block");
    if (request.DynamicSegmentOffset - request.ExtraBlockOffset > request.ExtraBlockSize ||
        request.DynamicSegmentSize > request.ExtraBlockSize - (request.DynamicSegmentOffset - request.ExtraBlockOffset))
        throw Domain::RelinkerException("Dynamic segment does not fit within the extra block");
    if (request.InterpOffset < request.ExtraBlockOffset)
        throw Domain::RelinkerException("Interp offset lies before the extra block");
    if (request.InterpOffset - request.ExtraBlockOffset > request.ExtraBlockSize ||
        request.InterpSize > request.ExtraBlockSize - (request.InterpOffset - request.ExtraBlockOffset))
        throw Domain::RelinkerException("Interp data does not fit within the extra block");

    const std::uint64_t dynamicSegmentVaddr = extraBlockVaddr + (request.DynamicSegmentOffset - request.ExtraBlockOffset);
    const std::uint64_t interpVaddr = extraBlockVaddr + (request.InterpOffset - request.ExtraBlockOffset);

    std::vector<Domain::ProgramHeader> layout;
    const std::uint64_t phdrVaddr = tableInExtraBlock ? extraBlockVaddr + (request.PhOff - request.ExtraBlockOffset) : headerBlockVaddr + request.PhOff;
    layout.push_back(_makePhdrHeader(request.PhOff, phdrVaddr, static_cast<std::uint64_t>(neededPh) * request.PhEntSize));
    layout.push_back(_makeInterpHeader(request.InterpOffset, interpVaddr, request.InterpSize));
    for (auto ph : kept) {
        if (ph.Type == PT_LOAD)
            ph.Flags = _fixLoadFlags(ph.Flags);
        layout.push_back(ph);
    }
    const std::uint64_t extraBlockPageEnd = checkedEnd(checkedEnd(extraBlockVaddr, request.ExtraBlockSize), kDefaultLoadAlignment - 1) & ~(kDefaultLoadAlignment - 1);
    layout.push_back(_makeLoadHeader(request.ExtraBlockOffset, extraBlockVaddr, request.ExtraBlockSize, kDefaultLoadAlignment, extraBlockPageEnd - extraBlockVaddr, PF_R | PF_W | PF_X));
    layout.push_back(_makeLoadHeader(0, headerBlockVaddr, headerBlockSize, headerBlockAlign, headerBlockSize, PF_R));
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
