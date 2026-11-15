#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/IEntryPointCollector.hpp>
#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <algorithm>

namespace Relinker {

class CfgBackedNidFilter : public IUnusedNidFilter {
public:
    std::vector<NidReference> Filter(
        const std::vector<NidReference>& nidRefs,
        const std::vector<std::uint8_t>& elfBytes,
        const std::vector<std::uint8_t>& textSection,
        VirtualAddress textVAddr,
        const std::vector<VirtualAddress>& extraEntries
    ) override {
        if (textSection.empty()) throw RelinkerException("Cannot filter NIDs: text section is empty");

        auto relativeRelocations = UnusedNidFilter::BuildRelativeRelocationIndex(elfBytes);
        auto collector = UnusedNidFilter::MakeEntryPointCollector();
        auto entries = collector->Collect(elfBytes, textVAddr, textSection.size(), *relativeRelocations);
        for (const auto entry : extraEntries)
            if (entry >= textVAddr && entry - textVAddr < textSection.size()) entries.push_back(entry);

        if (entries.empty()) throw RelinkerException("Cannot filter NIDs: no entry points found");

        VirtualAddress primary = entries[0];
        std::vector<VirtualAddress> extra(entries.begin() + 1, entries.end());

        const ElfReader reader(elfBytes);
        std::vector<UnusedNidFilter::StrictDataRegion> data;
        const auto headers = reader.ReadProgramHeaders();
        const bool hasSce = std::any_of(headers.begin(), headers.end(), [](const auto& h) { return h.Type == 0x61000000; });
        for (const auto& segment : headers) {
            if (segment.Type != 1 || segment.FileSize == 0) continue;
            if ((segment.Flags & 1) == 0) {
                if ((segment.Flags & 6) == 0) continue;
                data.push_back({segment.MappedAddress, reader.ReadSegment(segment)});
            } else if (hasSce && segment.FileSize > textSection.size()) {
                auto seg = reader.ReadSegment(segment);
                data.push_back({segment.MappedAddress + textSection.size(), std::vector<std::uint8_t>(seg.begin() + textSection.size(), seg.end())});
            }
        }

        auto cfg = UnusedNidFilter::BuildAddressTakenControlFlowGraph(textSection, textVAddr, primary, extra, *relativeRelocations, data);
        auto index = UnusedNidFilter::BuildGotAccessIndex(*cfg, textSection, textVAddr);

        std::vector<NidReference> result;
        for (const auto& ref : nidRefs)
            if (ref.RelocationTypeValue != Elfpatcher::R_X86_64_GLOB_DAT || index->IsGotSlotAccessed(ref.RelocationAddress))
                result.push_back(ref);
        return result;
    }
};

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter() {
    return std::make_shared<CfgBackedNidFilter>();
}

}
