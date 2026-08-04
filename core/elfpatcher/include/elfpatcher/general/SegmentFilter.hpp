#ifndef ELFPATCHER_FILTERING_SEGMENTFILTER_HPP
#define ELFPATCHER_FILTERING_SEGMENTFILTER_HPP

#include <elfpatcher/general/ISegmentFilter.hpp>

namespace Elfpatcher {

class SegmentFilter : public ISegmentFilter {
public:
    bool ShouldSkip(const Domain::ProgramHeader& ph) const override;

private:
    [[nodiscard]] bool _isSceSpecificSegment(std::uint32_t type) const;
    [[nodiscard]] bool _isNullPageLoad(const Domain::ProgramHeader& ph) const;
};

}

#endif
