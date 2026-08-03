#ifndef RELINKER_ICALLSITERESOLVER_HPP
#define RELINKER_ICALLSITERESOLVER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

class ICallSiteResolver {
public:
    virtual ~ICallSiteResolver() = default;

    virtual std::vector<Offset> ResolveCallSites(
        const std::vector<std::uint8_t>& TextSection,
        Offset TextSectionVAddr,
        Address TargetGotOrPltAddress,
        Size TargetGotOrPltSize
    ) = 0;
};

}

#endif // RELINKER_ICALLSITERESOLVER_HPP
