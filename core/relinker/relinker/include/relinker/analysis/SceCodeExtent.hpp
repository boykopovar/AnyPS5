#ifndef RELINKER_ANALYSIS_SCECODEEXTENT_HPP
#define RELINKER_ANALYSIS_SCECODEEXTENT_HPP

#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <optional>
#include <span>

namespace Relinker {

std::optional<VirtualAddress> FindSceCodeEnd(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions, VirtualAddress pltGot, ByteCount pltRelocationsSize);
std::vector<CodeRange> SceUnwindRanges(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<UnusedNidFilter::StrictCodeRegion>& functions);

}

#endif
