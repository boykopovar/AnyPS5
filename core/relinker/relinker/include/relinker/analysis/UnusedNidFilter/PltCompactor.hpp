#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_PLTCOMPACTOR_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_PLTCOMPACTOR_HPP

#include <relinker/domain/RelinkResult.hpp>

namespace Relinker::UnusedNidFilter {

struct CompactedPlt {
    std::vector<NidReference> References;
    std::vector<RelinkPatch> Patches;
    std::uint32_t SlotCount = 0;
};

CompactedPlt CompactPlt(const std::vector<NidReference>& originalReferences, const std::vector<NidReference>& keptReferences, const std::vector<std::uint8_t>& text, VirtualAddress textVaddr, FileByteOffset textOffset, FileByteOffset tableOffset);

}

#endif
