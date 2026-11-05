#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_IGOTACCESSINDEX_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_IGOTACCESSINDEX_HPP

#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <unordered_set>
#include <memory>
#include <vector>
#include <cstdint>

namespace Relinker::UnusedNidFilter {

class IGotAccessIndex {
public:
    virtual ~IGotAccessIndex() = default;
    virtual bool IsGotSlotAccessed(VirtualAddress gotSlotVaddr) const = 0;
};

std::unique_ptr<IGotAccessIndex> BuildGotAccessIndex(
    const IControlFlowGraph& cfg,
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr
);

}

#endif
