#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_IENTRYPOINTCOLLECTOR_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_IENTRYPOINTCOLLECTOR_HPP

#include <relinker/domain/Types.hpp>
#include <memory>
#include <vector>

namespace Relinker::UnusedNidFilter {

class IEntryPointCollector {
public:
    virtual ~IEntryPointCollector() = default;
    virtual std::vector<VirtualAddress> Collect(
        const std::vector<std::uint8_t>& elfBytes,
        VirtualAddress textVaddr,
        std::size_t textSize
    ) const = 0;
};

std::unique_ptr<IEntryPointCollector> MakeEntryPointCollector();

}

#endif
