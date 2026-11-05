#ifndef ELFPATCHER_DOMAIN_SECTIONHEADERTABLEREQUEST_HPP
#define ELFPATCHER_DOMAIN_SECTIONHEADERTABLEREQUEST_HPP

#include <cstdint>

namespace Elfpatcher {

struct SectionHeaderTableRequest {
    std::uint64_t DynStrOffset;
    std::uint64_t DynStrSize;
    std::uint64_t DynSymOffset;
    std::uint64_t DynSymSize;
    std::uint64_t DynamicSegmentOffset;
    std::uint64_t DynamicSegmentSize;
    std::uint64_t StubOffset;
    std::uint64_t StubSize;
};

}

#endif
