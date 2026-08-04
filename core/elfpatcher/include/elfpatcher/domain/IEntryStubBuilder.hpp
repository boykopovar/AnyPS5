#ifndef ELFPATCHER_DOMAIN_IENTRYSTUBBUILDER_HPP
#define ELFPATCHER_DOMAIN_IENTRYSTUBBUILDER_HPP

#include <cstdint>
#include <vector>

namespace Elfpatcher {

class IEntryStubBuilder {
public:
    virtual ~IEntryStubBuilder() = default;

    virtual std::vector<std::uint8_t> BuildEntryStub(
        std::uint64_t stubVaddr,
        std::uint64_t realEntryVaddr
    ) const = 0;
};

}

#endif
