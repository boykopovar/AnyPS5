#include <relinker/analysis/SceCodeExtent.hpp>
#include <algorithm>
#include <cstring>

namespace Relinker {

std::optional<VirtualAddress> FindSceCodeEnd(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions, VirtualAddress pltGot, ByteCount pltRelocationsSize) {
    VirtualAddress codeEnd = 0;
    for (const auto& function : functions)
        if (function.Begin >= segmentVAddr && function.Begin - segmentVAddr < segment.size())
            codeEnd = std::max(codeEnd, function.End);
    if (codeEnd == 0)
        return std::nullopt;
    if (codeEnd - segmentVAddr > segment.size())
        throw RelinkerException("Unwind function exceeds executable segment", codeEnd);
    if (pltRelocationsSize == 0)
        return codeEnd;
    const auto target = [&](const std::size_t at) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, segment.data() + at + 2, sizeof(displacement));
        return segmentVAddr + at + 6 + static_cast<VirtualAddress>(static_cast<std::int64_t>(displacement));
    };
    const auto isPlt = [&](const std::size_t at) {
        return segment[at] == 0xff && segment[at + 1] == 0x35 && segment[at + 6] == 0xff && segment[at + 7] == 0x25
            && target(at) == pltGot + 8 && target(at + 6) == pltGot + 16;
    };
    const std::size_t lastFunctionEnd = codeEnd - segmentVAddr;
    const ByteCount pltSize = 16 * (1 + pltRelocationsSize / 24);
    for (std::size_t at = lastFunctionEnd; segment.size() >= 16 && at <= segment.size() - 16; ++at) {
        if (!isPlt(at))
            continue;
        if (pltSize > segment.size() - at)
            throw RelinkerException("PLT exceeds executable segment", segmentVAddr + at);
        return segmentVAddr + at + pltSize;
    }
    for (std::size_t at = 0; at < lastFunctionEnd && at + 12 <= segment.size(); ++at) {
        if (!isPlt(at))
            continue;
        if (pltSize > segment.size() - at)
            throw RelinkerException("PLT exceeds executable segment", segmentVAddr + at);
        const VirtualAddress pltBegin = segmentVAddr + at;
        for (const auto& function : functions)
            if (function.Begin < pltBegin + pltSize && pltBegin < function.End)
                throw RelinkerException("PLT overlaps an unwind function", pltBegin);
        return codeEnd;
    }
    throw RelinkerException("PLT not found before or after the unwind functions", codeEnd);
}

std::vector<CodeRange> SceUnwindRanges(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions) {
    std::vector<CodeRange> ranges;
    for (const auto& function : functions)
        if (function.Begin >= segmentVAddr && function.Begin - segmentVAddr < segment.size() && function.End > function.Begin)
            ranges.push_back({function.Begin - segmentVAddr, std::min<ByteCount>(function.End - segmentVAddr, segment.size())});
    return ranges;
}

std::vector<Codegen::CodeSegmentExtent> FindCodeSegmentExtents(std::span<const std::uint8_t> bytes, const std::vector<Domain::ProgramHeader>& headers, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions, VirtualAddress pltGot, ByteCount pltRelocationsSize) {
    const bool sceImage = std::any_of(headers.begin(), headers.end(), [](const Domain::ProgramHeader& header) { return header.Type == 0x61000000; });
    const bool hasEhFrame = sceImage || std::any_of(headers.begin(), headers.end(), [](const Domain::ProgramHeader& header) { return header.Type == 0x6474e550; });
    std::vector<Codegen::CodeSegmentExtent> extents;
    for (const auto& header : headers) {
        if (header.Type != 1 || (header.Flags & 1) == 0 || header.Offset > bytes.size() || header.FileSize > bytes.size() - header.Offset)
            continue;
        std::size_t codeSize = static_cast<std::size_t>(header.FileSize);
        if (sceImage) {
            const std::span<const std::uint8_t> seg(bytes.data() + header.Offset, header.FileSize);
            if (const auto end = FindSceCodeEnd(seg, header.MappedAddress, functions, pltGot, pltRelocationsSize))
                codeSize = static_cast<std::size_t>(*end - header.MappedAddress);
        }
        std::vector<Domain::CodeRange> unwindRanges;
        if (hasEhFrame) {
            const std::span<const std::uint8_t> seg(bytes.data() + header.Offset, codeSize);
            unwindRanges = SceUnwindRanges(seg, header.MappedAddress, functions);
        }
        extents.push_back({header.MappedAddress, codeSize, std::move(unwindRanges)});
    }
    return extents;
}

}
