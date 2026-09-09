#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_EHFRAMEREADER_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_EHFRAMEREADER_HPP

#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>

namespace Relinker::UnusedNidFilter {

std::vector<StrictCodeRegion> ReadExceptionFunctions(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, const std::map<VirtualAddress, VirtualAddress>& pointers, const std::set<VirtualAddress>& importSlots);

}

#endif
