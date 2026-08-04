#ifndef ELFPATCHER_DOMAIN_ISECTIONHEADERTABLEBUILDER_HPP
#define ELFPATCHER_DOMAIN_ISECTIONHEADERTABLEBUILDER_HPP

#include <elfpatcher/general/SectionHeaderTableRequest.hpp>
#include <cstdint>
#include <vector>

namespace Elfpatcher {

class ISectionHeaderTableBuilder {
public:
    virtual ~ISectionHeaderTableBuilder() = default;

    virtual void WriteTable(
        std::vector<std::uint8_t>& buf,
        const SectionHeaderTableRequest& request
    ) const = 0;
};

}

#endif
