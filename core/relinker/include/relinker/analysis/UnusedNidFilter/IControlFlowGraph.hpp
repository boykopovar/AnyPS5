#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_ICONTROLFLOWGRAPH_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_ICONTROLFLOWGRAPH_HPP

#include <relinker/domain/Types.hpp>
#include <unordered_set>
#include <memory>
#include <vector>

namespace Relinker::UnusedNidFilter {

class IControlFlowGraph {
public:
    virtual ~IControlFlowGraph() = default;
    virtual const std::unordered_set<VirtualAddress>& ReachableVaddrs() const = 0;
    virtual bool IsReachable(VirtualAddress vaddr) const = 0;
};

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries
);

}

#endif
