#ifndef RELINKER_ANALYSIS_UNUSED_NID_FILTER_IRELATIVERELOCATIONINDEX_HPP
#define RELINKER_ANALYSIS_UNUSED_NID_FILTER_IRELATIVERELOCATIONINDEX_HPP

#include <relinker/domain/Types.hpp>
#include <optional>
#include <memory>
#include <vector>
#include <cstdint>

namespace Relinker::UnusedNidFilter {

class IRelativeRelocationIndex {
public:
    virtual ~IRelativeRelocationIndex() = default;
    virtual std::optional<VirtualAddress> TargetOfSlot(VirtualAddress slotVaddr) const = 0;
};

std::unique_ptr<IRelativeRelocationIndex> BuildRelativeRelocationIndex(
    const std::vector<std::uint8_t>& elfBytes
);

}

#endif
