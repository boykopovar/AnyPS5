#ifndef ELFPATCHER_DOMAIN_IPROGRAMHEADERLAYOUTBUILDER_HPP
#define ELFPATCHER_DOMAIN_IPROGRAMHEADERLAYOUTBUILDER_HPP

#include <elfpatcher/general/ProgramHeaderLayoutRequest.hpp>
#include <cstdint>
#include <vector>

namespace Elfpatcher {

    class IProgramHeaderLayoutBuilder {
    public:
        virtual ~IProgramHeaderLayoutBuilder() = default;

        virtual std::uint64_t ComputeExtraBlockVaddr(const std::vector<Domain::ProgramHeader>& originalHeaders) const = 0;

        virtual std::uint16_t WriteLayout(std::vector<std::uint8_t>& buf, const ProgramHeaderLayoutRequest& request) const = 0;
    };

}

#endif
