#ifndef ELFPATCHER_FILTERING_SEGMENTFILTER_HPP
#define ELFPATCHER_FILTERING_SEGMENTFILTER_HPP

#include <elfpatcher/general/ISegmentFilter.hpp>

namespace Elfpatcher {

class SegmentFilter : public ISegmentFilter {
public:
    explicit SegmentFilter(bool lazyBinding = false) : _lazyBinding(lazyBinding) {}
    bool ShouldSkip(const Domain::ProgramHeader& ph) const override;

private:
    bool _lazyBinding;
    [[nodiscard]] bool _isSceSpecificSegment(std::uint32_t type) const;
};

}

#endif
