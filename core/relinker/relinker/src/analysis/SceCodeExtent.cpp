#include <relinker/analysis/SceCodeExtent.hpp>
#include <relinker/analysis/RelativeJumpTables.hpp>
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

std::vector<CodeRange> DataTableRanges(std::span<const std::uint8_t> image, const std::vector<ProgramHeader>& headers, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions, VirtualAddress pltGot, ByteCount pltRelocationsSize) {
    std::vector<std::pair<VirtualAddress, VirtualAddress>> unwindRanges;
    for (const auto& function : functions) unwindRanges.emplace_back(function.Begin, function.End);
    std::sort(unwindRanges.begin(), unwindRanges.end());
    const bool sceImage = std::any_of(headers.begin(), headers.end(), [](const ProgramHeader& header) { return header.Type == 0x61000000; });
    std::vector<CodeRange> ranges;
    for (const auto& header : headers) {
        if (header.Type != 1 || (header.Flags & 1) == 0 || header.Offset > image.size() || header.FileSize > image.size() - header.Offset) continue;
        auto code = image.subspan(header.Offset, header.FileSize);
        if (sceImage)
            if (const auto end = FindSceCodeEnd(code, header.MappedAddress, functions, pltGot, pltRelocationsSize))
                code = code.first(*end - header.MappedAddress);
        for (const auto& table : FindRelativeJumpTables(code, header.MappedAddress, unwindRanges)) {
            const auto end = std::min<VirtualAddress>(table.End, header.MappedAddress + code.size());
            if (table.Begin >= header.MappedAddress && table.Begin < end) ranges.push_back({table.Begin, end});
        }
    }
    return ranges;
}

}
