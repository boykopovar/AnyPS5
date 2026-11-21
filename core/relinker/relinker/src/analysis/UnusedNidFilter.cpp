#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/IEntryPointCollector.hpp>
#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/UnusedNidFilter/IGotAccessIndex.hpp>
#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>

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

        const ElfReader reader(elfBytes);
        const auto headers = reader.ReadProgramHeaders();
        std::set<VirtualAddress> importSlots;
        for (const auto& ref : nidRefs) importSlots.insert(ref.RelocationAddress);
        std::set<VirtualAddress> unwindTargets;
        for (const auto& function : UnusedNidFilter::ReadExceptionFunctions(elfBytes, headers, relativeRelocations->SlotTargets(), importSlots))
            unwindTargets.insert(function.ExtraTargets.begin(), function.ExtraTargets.end());
        for (const auto target : unwindTargets)
            if (target >= textVAddr && target - textVAddr < textSection.size()) entries.push_back(target);

        if (entries.empty()) throw RelinkerException("Cannot filter NIDs: no entry points found");

        VirtualAddress primary = entries[0];
        std::vector<VirtualAddress> extra(entries.begin() + 1, entries.end());

        std::vector<UnusedNidFilter::StrictDataRegion> data;
        for (const auto& segment : headers) {
            if (segment.Type != 1 || (segment.Flags & 1) != 0 || (segment.Flags & 6) == 0 || segment.FileSize == 0) continue;
            data.push_back({segment.MappedAddress, reader.ReadSegment(segment)});
        }

        auto cfg = UnusedNidFilter::BuildAddressTakenControlFlowGraph(textSection, textVAddr, primary, extra, *relativeRelocations, data);
        auto index = UnusedNidFilter::BuildGotAccessIndex(*cfg, textSection, textVAddr);

        std::vector<NidReference> result;
        for (const auto& ref : nidRefs)
            if (ref.RelocationTypeValue != Elfpatcher::R_X86_64_GLOB_DAT || unwindTargets.contains(ref.RelocationAddress)
                || index->IsGotSlotAccessed(ref.RelocationAddress))
                result.push_back(ref);
        return result;
    }
};

std::shared_ptr<IUnusedNidFilter> MakeUnusedNidFilter() {
    return std::make_shared<CfgBackedNidFilter>();
}

}
