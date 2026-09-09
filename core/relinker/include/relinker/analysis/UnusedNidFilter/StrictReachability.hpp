#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_STRICTREACHABILITY_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_STRICTREACHABILITY_HPP

#include <relinker/domain/Types.hpp>
#include <map>
#include <set>

namespace Relinker::UnusedNidFilter {

struct StrictCodeRegion {
    VirtualAddress Begin;
    VirtualAddress End;
    std::vector<VirtualAddress> ExtraTargets;
};

struct StrictDataRegion {
    VirtualAddress Address;
    std::vector<std::uint8_t> Bytes;
};

struct StrictReachabilityInput {
    std::vector<std::uint8_t> Text;
    VirtualAddress TextVaddr = 0;
    std::vector<VirtualAddress> Entries;
    std::map<VirtualAddress, VirtualAddress> Pointers;
    std::set<VirtualAddress> ImportSlots;
    std::vector<StrictCodeRegion> Functions;
    std::vector<StrictDataRegion> Data;
};

struct StrictReachabilityResult {
    std::set<VirtualAddress> Instructions;
    std::set<VirtualAddress> ImportSlots;
    std::size_t IndirectTransfers = 0;
    std::size_t LiveRegions = 0;
    std::size_t TotalRegions = 0;
    std::size_t AddressTakenRoots = 0;
};

StrictReachabilityResult AnalyzeStrictReachability(const StrictReachabilityInput& input);

}

#endif
