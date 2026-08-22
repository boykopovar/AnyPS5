#ifndef RELINKER_DOMAIN_IUNUSEDNIDFILTER_HPP
#define RELINKER_DOMAIN_IUNUSEDNIDFILTER_HPP

#include <relinker/domain/Types.hpp>
#include <vector>
#include <string>

namespace Relinker {

class IUnusedNidFilter {
public:
    virtual ~IUnusedNidFilter() = default;
    virtual std::vector<NidReference> Filter(const std::vector<NidReference>& nidRefs, const std::vector<std::uint8_t>& textSection, VirtualAddress textVAddr) = 0;
};

}

#endif
