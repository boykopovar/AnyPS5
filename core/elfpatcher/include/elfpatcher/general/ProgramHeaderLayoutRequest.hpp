#ifndef ELFPATCHER_DOMAIN_PROGRAMHEADERLAYOUTREQUEST_HPP
#define ELFPATCHER_DOMAIN_PROGRAMHEADERLAYOUTREQUEST_HPP

#include <domain/Types.hpp>
#include <cstdint>
#include <vector>

namespace Elfpatcher {

struct ProgramHeaderLayoutRequest {
    std::uint64_t PhOff;
    std::uint16_t PhEntSize;
    std::uint16_t PhNum;
    std::vector<Domain::ProgramHeader> OriginalHeaders;
    std::uint64_t ExtraBlockOffset;
    std::uint64_t ExtraBlockSize;
    std::uint64_t DynamicSegmentOffset;
    std::uint64_t DynamicSegmentSize;
    std::uint64_t InterpOffset;
    std::uint64_t InterpSize;
};

}

#endif
