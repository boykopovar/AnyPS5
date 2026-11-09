#ifndef ELFPATCHER_DOMAIN_ISEGMENTFILTER_HPP
#define ELFPATCHER_DOMAIN_ISEGMENTFILTER_HPP

#include <domain/Types.hpp>

namespace Elfpatcher {

class ISegmentFilter {
public:
    virtual ~ISegmentFilter() = default;

    virtual bool ShouldSkip(const Domain::ProgramHeader& ph) const = 0;
};

}

#endif
