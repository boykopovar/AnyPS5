#ifndef RELINKER_ICALLSITERESOLVER_HPP
#define RELINKER_ICALLSITERESOLVER_HPP

#include <relinker/Types.hpp>
#include <vector>

namespace Relinker {

class ICallSiteResolver {
public:
    virtual ~ICallSiteResolver() = default;

    virtual std::vector<FileByteOffset> ResolveCallSites(
        const std::vector<std::uint8_t>& textSection,
        FileByteOffset textSectionVAddr,
        VirtualAddress targetGotOrPltAddress,
        ByteCount targetGotOrPltSize
    ) = 0;
};

}

#endif // RELINKER_ICALLSITERESOLVER_HPP
