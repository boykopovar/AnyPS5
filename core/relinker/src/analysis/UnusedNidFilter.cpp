#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/IEntryPointCollector.hpp>
#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>

namespace Relinker {

class CfgBackedNidFilter : public IUnusedNidFilter {
public:
    std::vector<NidReference> Filter(
        const std::vector<NidReference>& nidRefs,
        const std::vector<std::uint8_t>& elfBytes,
        const std::vector<std::uint8_t>& textSection,
        VirtualAddress textVAddr
    ) override {
        if (textSection.empty()) throw RelinkerException("Cannot filter NIDs: text section is empty");

        auto collector = UnusedNidFilter::MakeEntryPointCollector();
        auto entries = collector->Collect(elfBytes, textVAddr, textSection.size());

        if (entries.empty()) throw RelinkerException("Cannot filter NIDs: no entry points found");

        VirtualAddress primary = entries[0];
        std::vector<VirtualAddress> extra(entries.begin() + 1, entries.end());

        auto relativeRelocations = UnusedNidFilter::BuildRelativeRelocationIndex(elfBytes);
        auto cfg = UnusedNidFilter::BuildControlFlowGraph(textSection, textVAddr, primary, extra, *relativeRelocations);
        auto index = UnusedNidFilter::BuildGotAccessIndex(*cfg, textSection, textVAddr);

        std::vector<NidReference> result;
        for (const auto& ref : nidRefs)
            if (index->IsGotSlotAccessed(ref.RelocationAddress))
                result.push_back(ref);
        return result;
    }
};

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter() {
    return std::make_shared<CfgBackedNidFilter>();
}

}
