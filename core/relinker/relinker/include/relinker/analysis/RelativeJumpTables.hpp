#ifndef RELINKER_ANALYSIS_RELATIVEJUMPTABLES_HPP
#define RELINKER_ANALYSIS_RELATIVEJUMPTABLES_HPP

#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace Relinker {

struct RelativeJumpTable {
    VirtualAddress Function;
    VirtualAddress Begin;
    VirtualAddress End;
};

std::size_t FindTrailingRelativeJumpTable(std::span<const std::uint8_t> code);
std::vector<RelativeJumpTable> FindRelativeJumpTables(std::span<const std::uint8_t> segment, VirtualAddress segmentVAddr, const std::vector<std::pair<VirtualAddress, VirtualAddress>>& functions);

}

#endif
