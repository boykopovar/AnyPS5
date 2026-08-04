#ifndef ELFPATCHER_STUB_ENTRYSTUBBUILDER_HPP
#define ELFPATCHER_STUB_ENTRYSTUBBUILDER_HPP

#include <elfpatcher/domain/IEntryStubBuilder.hpp>

namespace Elfpatcher {

class EntryStubBuilder : public IEntryStubBuilder {
public:
    std::vector<std::uint8_t> BuildEntryStub(
        std::uint64_t stubVaddr,
        std::uint64_t realEntryVaddr
    ) const override;
};

}

#endif
