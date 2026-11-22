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
    const ByteCount stubCount = pltRelocationsSize / 24;
    const auto isHeader = [&](const std::size_t at) {
        return segment[at] == 0xff && segment[at + 1] == 0x35 && segment[at + 6] == 0xff && segment[at + 7] == 0x25
            && target(at) == pltGot + 8 && target(at + 6) == pltGot + 16;
    };
    const auto isEagerStubs = [&](const std::size_t at) {
        if (stubCount == 0 || stubCount > (segment.size() - at) / 16)
            return false;
        for (ByteCount index = 0; index < stubCount; ++index) {
            const auto stub = segment.subspan(at + 16 * index, 16);
            if (stub[0] != 0xff || stub[1] != 0x25 || target(at + 16 * index) != pltGot + 24 + 8 * index
                || std::any_of(stub.begin() + 6, stub.end(), [](const std::uint8_t byte) { return byte != 0xcc; }))
                return false;
        }
        return true;
    };
    const std::size_t lastFunctionEnd = codeEnd - segmentVAddr;
    const auto find = [&](const auto& isPlt, const ByteCount pltSize) -> std::optional<VirtualAddress> {
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
        return std::nullopt;
    };
    if (const auto end = find(isHeader, 16 * (1 + stubCount)))
        return end;
    if (const auto end = find(isEagerStubs, 16 * stubCount))
        return end;
    throw RelinkerException("PLT not found before or after the unwind functions", codeEnd);
}

std::vector<CodeRange> SceUnwindRanges(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions) {
    std::vector<CodeRange> ranges;
    for (const auto& function : functions)
        if (function.Begin >= segmentVAddr && function.Begin - segmentVAddr < segment.size() && function.End > function.Begin)
            ranges.push_back({function.Begin - segmentVAddr, std::min<ByteCount>(function.End - segmentVAddr, segment.size())});
    return ranges;
}

}
