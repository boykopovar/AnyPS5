#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_ICONTROLFLOWGRAPH_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_ICONTROLFLOWGRAPH_HPP

#include <relinker/domain/Types.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <unordered_set>
#include <memory>
#include <span>
#include <vector>
#include <functional>

namespace Relinker::UnusedNidFilter {

class IControlFlowGraph {
public:
    virtual ~IControlFlowGraph() = default;
    virtual const std::unordered_set<VirtualAddress>& ReachableVaddrs() const = 0;
    virtual bool IsReachable(VirtualAddress vaddr) const = 0;
};

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    std::span<const std::uint8_t> text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    bool followCodeAddresses = false
);

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(std::span<const std::uint8_t> text, VirtualAddress textVaddr, VirtualAddress entryVaddr, const std::vector<VirtualAddress>& extraEntries, const IRelativeRelocationIndex& relativeRelocations, bool followCodeAddresses, std::unordered_set<VirtualAddress>& reachable, const std::function<void(std::uint64_t)>& progress = {}, const std::function<bool(VirtualAddress)>& isData = {});

std::unique_ptr<IControlFlowGraph> BuildControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    bool followCodeAddresses = false
);

std::unique_ptr<IControlFlowGraph> BuildAddressTakenControlFlowGraph(
    const std::vector<std::uint8_t>& text,
    VirtualAddress textVaddr,
    VirtualAddress entryVaddr,
    const std::vector<VirtualAddress>& extraEntries,
    const IRelativeRelocationIndex& relativeRelocations,
    const std::vector<StrictDataRegion>& data
);

}

#endif
